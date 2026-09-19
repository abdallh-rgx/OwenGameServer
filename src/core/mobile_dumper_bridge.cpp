/**
 * @file mobile_dumper_bridge.cpp
 * @brief Bridge between OwenGameServer (Project B) and the MobileDumper-7 core
 *        static library (Project A, libMobileDumperCore.a).
 *
 * Exposes exactly two C entry points:
 *
 *   extern "C" int MD7_Setup(uintptr_t imageBase);
 *   extern "C" int MD7_ReadFName(int32_t index, char* outBuf, int bufSize) -> int
 *
 * Design guarantees:
 *  - The .a was built with -DMD7_BUILD_CORE=ON: it contains NO constructor
 *    (main_android.cpp is not compiled), NO RunDump / DumperMain, NO SDK
 *    generation, NO zip / idmap / dumpspace / mappings output, NO KittyMemoryEx
 *    and NO MemoryAndroid. Nothing dumps at load time, after 60 seconds, or
 *    ever. The only code pulled from Project A is the FNamePool reader
 *    (NameArray / FNameEntry / FName) plus its globals.
 *  - DirectMemory implements Project A's IMemory interface with in-process
 *    memcpy reads guarded by a /proc/self/maps cache. It is assigned to
 *    Project A's GMemory global (which in this merged build has no other
 *    backend - MemoryAndroid/KittyMemoryEx are not linked at all). It does
 *    NOT touch or replace anything in OwenGameServer: all existing reads in
 *    the GameServer (raw pointers, ProcessEvent, Dobby hooks) keep working
 *    exactly as before. This bridge is reachable only through the two
 *    MD7_* functions below.
 *  - No symbol conflicts: Project A's globals (GObjects, GNames, GLayouts,
 *    GOffsets, GInSDKOffsets, GMemory, GSettings, GLogger) are hidden-visibility
 *    and unique to the core; OwenGameServer's FName/Utils/UC containers live in
 *    different namespaces/scopes (SDK::FName, global Utils functions, SDK::InSDKUtils).
 *
 * Offsets: Fortnite 21.30 Android ARM64 (libUnreal.so), supplied by the user.
 */

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <link.h>
#include <unistd.h>

/* MobileDumper-7 core headers (vendored under external/MobileDumperCore/include/Dumper) */
#include "Memory/IMemory.h"
#include "Engine/Unreal/NameArray.h"
#include "Engine/OffsetFinder/Offsets.h"
#include "Settings.h"
#include "Utils/Utils.h"

// ============================================================
// Fortnite 21.30 constants (image-relative offsets)
// ============================================================
namespace
{

constexpr uintptr_t kFnGNamesOffset   = 0x0DC960C0; // GNames (FNamePool) RVA
constexpr uintptr_t kFnGObjectsOffset = 0x0DCD6E08; // GObjects RVA (informational)

/* FNamePool / FNameEntry layout for Fortnite 21.30 Android ARM64. */
constexpr int32_t kBlocksBit          = 16;   // FNamePool::BlocksBit
constexpr int32_t kMaxChunkIndexOff   = 0x38; // FNamePool::MaxChunkIndex
constexpr int32_t kByteCursorOff      = 0x3C; // FNamePool::ByteCursor
constexpr int32_t kBlocksOff          = 0x40; // FNamePool::Blocks
constexpr int32_t kEntryStride        = 0x4;  // FNameEntry::Stride
constexpr int32_t kEntryHeaderOff     = 0x0;  // FNameEntry::Header
constexpr int32_t kEntryStringOff     = 0x4;  // FNameEntry::String
constexpr int32_t kEntryNameWideMask  = 0x1;  // FNameEntry::NameWideMask
constexpr int32_t kEntryLengthShift   = 0x6;  // FNameEntry::LengthShiftCount

/* Minimum interval between two /proc/self/maps re-parses (rate limiter). */
constexpr auto kMapsRefreshMinInterval = std::chrono::milliseconds(100);

/* Upper bound for the index -> name cache (FName entries are append-only, so
 * cached results stay valid for the lifetime of the process). */
constexpr size_t kNameCacheLimit = 200000;

// ============================================================
// /proc/self/maps cache
// ============================================================

struct MD7MapRegion
{
    uintptr_t Start = 0;
    uintptr_t End   = 0;
    bool Readable   = false;
    bool Writeable  = false;
    bool Executable = false;
    std::string Path;
};

bool MD7_ParseMaps(std::vector<MD7MapRegion>& Out)
{
    Out.clear();

    FILE* F = std::fopen("/proc/self/maps", "r");
    if (!F)
        return false;

    char Line[1024];
    while (std::fgets(Line, sizeof(Line), F))
    {
        // Format: "start-end rwxp offset dev inode pathname"
        uintptr_t Start = 0;
        uintptr_t End   = 0;
        char Perms[8]   = {0};

        if (std::sscanf(Line, "%lx-%lx %7s", &Start, &End, Perms) != 3)
            continue;

        MD7MapRegion R;
        R.Start      = Start;
        R.End        = End;
        R.Readable   = Perms[0] == 'r';
        R.Writeable  = Perms[1] == 'w';
        R.Executable = Perms[2] == 'x';

        // Pathname: everything after the 5th field (may be empty or contain spaces).
        const char* Path = nullptr;
        {
            int Spaces = 0;
            for (const char* P = Line; *P; ++P)
            {
                if (*P == ' ')
                {
                    ++Spaces;
                    if (Spaces == 5)
                    {
                        Path = P + 1;
                        break;
                    }
                }
            }
        }
        if (Path)
        {
            const char* E = Path;
            while (*E && *E != '\n')
                ++E;
            if (E > Path)
                R.Path.assign(Path, static_cast<size_t>(E - Path));
        }

        Out.push_back(std::move(R));
    }

    std::fclose(F);

    // Keep the vector sorted by start address (maps is already sorted, but be explicit).
    std::sort(Out.begin(), Out.end(), [](const MD7MapRegion& A, const MD7MapRegion& B) { return A.Start < B.Start; });
    return !Out.empty();
}

// ============================================================
// DirectMemory: IMemory over in-process memcpy + maps guard
// ============================================================

class DirectMemory final : public IMemory
{
public:
    explicit DirectMemory(uintptr_t UnrealImageBase)
        : UnrealBase_(UnrealImageBase)
    {
    }

    // ── IMemory interface ────────────────────────────────────────

    bool Initialize() override
    {
        std::lock_guard<std::mutex> Lock(MapsMutex_);
        return RefreshMapsLocked();
    }

    std::string GetProcessName() const override
    {
        return "self";
    }

    int GetProcessID() const override
    {
        return static_cast<int>(::getpid());
    }

    bool RefreshMemory() override
    {
        std::lock_guard<std::mutex> Lock(MapsMutex_);
        return RefreshMapsLocked();
    }

    bool IsMemoryAccessOk() const override
    {
        return true;
    }

    bool IsValidAddress(uintptr_t Address) const override
    {
        Address = MemoryUtils::UntagPointer(Address);

        std::lock_guard<std::mutex> Lock(MapsMutex_);
        if (const MD7MapRegion* R = FindRegionLocked(Address))
            return true;

        if (RefreshMapsLocked())
            return FindRegionLocked(Address) != nullptr;

        return false;
    }

    bool IsAddressReadable(uintptr_t Address, size_t Size) const override
    {
        return CheckAccessLocked(Address, Size, &MD7MapRegion::Readable);
    }

    bool IsAddressWriteable(uintptr_t Address, size_t Size) const override
    {
        return CheckAccessLocked(Address, Size, &MD7MapRegion::Writeable);
    }

    bool IsAddressExecutable(uintptr_t Address, size_t Size) const override
    {
        return CheckAccessLocked(Address, Size, &MD7MapRegion::Executable);
    }

    MemRegionInfo GetAddressRegionInfo(uintptr_t Address) const override
    {
        Address = MemoryUtils::UntagPointer(Address);

        std::lock_guard<std::mutex> Lock(MapsMutex_);
        const MD7MapRegion* R = FindRegionLocked(Address);
        if (!R && RefreshMapsLocked())
            R = FindRegionLocked(Address);

        if (!R)
            return MemRegionInfo{};

        return MemRegionInfo(R->Path, R->Start, R->End, 0x0, R->Readable, R->Writeable, R->Executable);
    }

    size_t ReadBytes(uintptr_t Address, void* Buffer, size_t Size) const override
    {
        if (Address == 0 || Buffer == nullptr || Size == 0)
            return 0;

        Address = MemoryUtils::UntagPointer(Address);

        // Full-range validation before any dereference: this is what replaces the
        // old raw *(T*) reads that SIGSEGV'd on unmapped/stale pointers.
        if (!CheckAccessLocked(Address, Size, &MD7MapRegion::Readable))
            return 0;

        std::memcpy(Buffer, reinterpret_cast<const void*>(Address), Size);
        return Size;
    }

    bool WriteBytes(uintptr_t Address, const void* Buffer, size_t Size) const override
    {
        if (Address == 0 || Buffer == nullptr || Size == 0)
            return false;

        Address = MemoryUtils::UntagPointer(Address);

        if (!CheckAccessLocked(Address, Size, &MD7MapRegion::Writeable))
            return false;

        std::memcpy(reinterpret_cast<void*>(Address), Buffer, Size);
        return true;
    }

    bool ReadRelocationPointer(uintptr_t Slot, uintptr_t& Out) const override
    {
        Out = 0;

        if (!IsAddressReadable(Slot, sizeof(uintptr_t)))
            return false;

        Out = Read<uintptr_t>(Slot);
        return Out != 0 && IsAddressReadable(Out, sizeof(uintptr_t));
    }

    ModuleInfo GetModuleInfo(const std::string& ModuleName) override
    {
        std::lock_guard<std::mutex> Lock(MapsMutex_);

        const std::string Needle = "/" + ModuleName;

        uintptr_t Start = UINTPTR_MAX;
        uintptr_t End   = 0;
        std::vector<MemRegionInfo> Segments;

        for (const MD7MapRegion& R : Maps_)
        {
            if (R.Path.empty())
                continue;

            const std::string& P = R.Path;
            if (P.size() >= ModuleName.size() + 1 &&
                P.compare(P.size() - ModuleName.size() - 1, ModuleName.size() + 1, Needle) == 0)
            {
                Start = std::min(Start, R.Start);
                End   = std::max(End, R.End);
                Segments.emplace_back(R.Path, R.Start, R.End, 0x0, R.Readable, R.Writeable, R.Executable);
            }
        }

        if (Start == UINTPTR_MAX || End == 0)
            return ModuleInfo{};

        return ModuleInfo(ModuleName, Start, End, Start, std::move(Segments));
    }

    uintptr_t FindModuleSymbol(const std::string& ModuleName, const std::string& SymbolName) override
    {
        // The core never resolves exported symbols (offsets are fixed for 21.30).
        ((void)ModuleName);
        ((void)SymbolName);
        return 0;
    }

    ModuleInfo GetUnrealModule() override
    {
        std::lock_guard<std::mutex> Lock(MapsMutex_);

        if (!UnrealModuleCache_.IsValid())
        {
            uintptr_t Start = UINTPTR_MAX;
            uintptr_t End   = 0;
            std::vector<MemRegionInfo> Segments;

            for (const MD7MapRegion& R : Maps_)
            {
                if (R.Path.find("libUnreal.so") == std::string::npos)
                    continue;

                Start = std::min(Start, R.Start);
                End   = std::max(End, R.End);
                Segments.emplace_back(R.Path, R.Start, R.End, 0x0, R.Readable, R.Writeable, R.Executable);
            }

            if (Start != UINTPTR_MAX && End != 0)
                UnrealModuleCache_ = ModuleInfo("libUnreal.so", Start, End, UnrealBase_, std::move(Segments));
        }

        return UnrealModuleCache_;
    }

    uintptr_t FindUnrealSymbol(const std::string& SymbolName) override
    {
        ((void)SymbolName);
        return 0;
    }

    /* Pattern scan APIs: unused by the FName path; provided because IMemory is abstract. */
    uintptr_t FindPatternInRange(uintptr_t Start, size_t Range, const std::string& Pattern, int Step, uint32_t SkipCount) const override
    {
        ((void)Start);
        ((void)Range);
        ((void)Pattern);
        ((void)Step);
        ((void)SkipCount);
        return 0;
    }

    std::vector<uintptr_t> FindAllPatternInRange(uintptr_t Start, size_t Range, const std::string& Pattern, int Step, size_t MaxHits) const override
    {
        ((void)Start);
        ((void)Range);
        ((void)Pattern);
        ((void)Step);
        ((void)MaxHits);
        return {};
    }

    uintptr_t FindAlignedValueInRange(uintptr_t Value, int32_t Alignment, uintptr_t Start, size_t Range) const override
    {
        ((void)Value);
        ((void)Alignment);
        ((void)Start);
        ((void)Range);
        return 0;
    }

    std::vector<uintptr_t> FindAllAlignedValuesInRange(uintptr_t Value, int32_t Alignment, uintptr_t Start, size_t Range, size_t MaxHits) const override
    {
        ((void)Value);
        ((void)Alignment);
        ((void)Start);
        ((void)Range);
        ((void)MaxHits);
        return {};
    }

    uintptr_t FindRawDataInRange(const void* Data, size_t DataSize, uintptr_t Start, size_t Range) const override
    {
        ((void)Data);
        ((void)DataSize);
        ((void)Start);
        ((void)Range);
        return 0;
    }

    std::vector<uintptr_t> FindAllRawDataInRange(const void* Data, size_t DataSize, uintptr_t Start, size_t Range, size_t MaxHits) const override
    {
        ((void)Data);
        ((void)DataSize);
        ((void)Start);
        ((void)Range);
        ((void)MaxHits);
        return {};
    }

private:
    using RegionFlag = bool MD7MapRegion::*;

    /* Validates that [Address, Address+Size) lies in mapped memory with the
     * requested permission, refreshing the maps cache once on a miss. */
    bool CheckAccessLocked(uintptr_t Address, size_t Size, RegionFlag Flag) const
    {
        Address = MemoryUtils::UntagPointer(Address);

        std::lock_guard<std::mutex> Lock(MapsMutex_);

        if (Size == 0)
        {
            const MD7MapRegion* R = FindRegionLocked(Address);
            return R != nullptr && (R->*Flag);
        }

        const MD7MapRegion* First = FindRegionLocked(Address);
        if (First && (First->*Flag) && First->End >= Address + Size)
            return true; // fully inside one region (the common case)

        // Miss: either unmapped, or the range may cross into a region added after
        // the cache was built (FNamePool blocks are allocated as the game registers
        // new names). Re-parse /proc/self/maps once and retry.
        if (RefreshMapsLocked())
        {
            const MD7MapRegion* A = FindRegionLocked(Address);
            if (A && (A->*Flag))
            {
                const MD7MapRegion* B = FindRegionLocked(Address + Size - 1);
                if (B && (B->*Flag))
                    return true;
            }
        }

        return false;
    }

    const MD7MapRegion* FindRegionLocked(uintptr_t Address) const
    {
        const auto It = std::upper_bound(Maps_.begin(), Maps_.end(), Address, [](uintptr_t Value, const MD7MapRegion& R) { return Value < R.Start; });
        if (It == Maps_.begin())
            return nullptr;

        const MD7MapRegion& Candidate = *(It - 1);
        return Candidate.Start <= Address && Address < Candidate.End ? &Candidate : nullptr;
    }

    /* Caller must hold MapsMutex_. Rate-limited so bursts of bad indices can't
     * turn into a /proc/self/maps re-parse storm. */
    bool RefreshMapsLocked() const
    {
        const auto Now = std::chrono::steady_clock::now();
        if (MapsEverRefreshed_ && Now - LastRefresh_ < kMapsRefreshMinInterval)
            return !Maps_.empty();

        MapsEverRefreshed_ = true;
        LastRefresh_       = Now;
        return MD7_ParseMaps(Maps_);
    }

    mutable std::mutex MapsMutex_;
    mutable std::vector<MD7MapRegion> Maps_;
    mutable std::chrono::steady_clock::time_point LastRefresh_{};
    mutable bool MapsEverRefreshed_ = false;
    mutable ModuleInfo UnrealModuleCache_{};

    uintptr_t UnrealBase_ = 0;
};

// ============================================================
// FNamePool walking — Project A's IProfile logic (FNamePool branch),
// specialized for Fortnite 21.30 (no encryption).
// ============================================================

/* Index -> address of the FNameEntry header.
 * Mirrors IProfile::GetNameEntryByIndex() (Pool branch) from MobileDumper-7. */
uintptr_t MD7_GetNameEntryByIndex(int32_t Index)
{
    if (Index < 0 || GNames == 0 || !GMemory || !GLayouts.NamesLayout)
        return 0;

    if (GLayouts.NamesLayout->GetType() != ENamesType::Pool)
        return 0;

    const FNamePoolLayout* Layout = static_cast<const FNamePoolLayout*>(GLayouts.NamesLayout.get());

    const int32_t ChunkIdx      = Index >> Layout->BlocksBit;
    const int32_t InChunkOffset = (Index & ((1 << Layout->BlocksBit) - 1)) * Layout->FNameEntry.Stride;

    if (ChunkIdx < 0)
        return 0;

    const uintptr_t ChunksBase = GNames + static_cast<uintptr_t>(Layout->Blocks);
    const uintptr_t ChunkAddr  = GMemory->Read<uintptr_t>(ChunksBase + static_cast<uintptr_t>(ChunkIdx) * sizeof(void*));

    /* Fortnite 21.30: no DecryptNameChunk / DecryptNameEntry hooks. */

    if (!GMemory->IsAddressReadable(ChunkAddr))
        return 0;

    return ChunkAddr + static_cast<uintptr_t>(InChunkOffset);
}

/* FNameEntry address -> name string.
 * Mirrors IProfile::GetNameEntryString() (Pool branch) from MobileDumper-7, with
 * one correctness fix: after following an outline-number entry (header length == 0)
 * to its base entry, the base entry's header is re-read. The upstream code kept the
 * stale zero-length header, which made every numbered name resolve to "". */
std::wstring MD7_GetNameEntryString(uintptr_t NameEntry)
{
    if (!GMemory || !GLayouts.NamesLayout || !GMemory->IsAddressReadable(NameEntry))
        return {};

    if (GLayouts.NamesLayout->GetType() != ENamesType::Pool)
        return {};

    const FNamePoolLayout* Layout = static_cast<const FNamePoolLayout*>(GLayouts.NamesLayout.get());

    uint16_t Header   = GMemory->Read<uint16_t>(NameEntry + Layout->FNameEntry.Header);
    int32_t  NameLen  = Header >> Layout->FNameEntry.LengthShiftCount;
    int32_t  StrNumber = 0;

    if (NameLen == 0)
    {
        /* Outline-number entry: stores {NextEntryIndex, StrNumber} instead of characters. */
        const int32_t EntryIdOffset  = Layout->FNameEntry.String + ((Layout->FNameEntry.String == 6) * 2);
        const int32_t NextEntryIndex = GMemory->Read<int32_t>(NameEntry + EntryIdOffset);
        StrNumber                    = GMemory->Read<int32_t>(NameEntry + EntryIdOffset + sizeof(int32_t));

        NameEntry = MD7_GetNameEntryByIndex(NextEntryIndex);

        if (!GMemory->IsAddressReadable(NameEntry))
            return {};

        Header  = GMemory->Read<uint16_t>(NameEntry + Layout->FNameEntry.Header);
        NameLen = Header >> Layout->FNameEntry.LengthShiftCount;
    }

    const bool     IsWide  = (Header & Layout->FNameEntry.NameWideMask) != 0;
    const int32_t  StrLen  = NameLen;
    const uintptr_t StrAddr = NameEntry + Layout->FNameEntry.String;

    if (!GMemory->IsAddressReadable(StrAddr) || StrLen <= 0 || StrLen > GSettings.General.MaxFNameLen)
        return {};

    std::wstring Result;

    if (IsWide)
    {
        if (!InternalSettings::bUseChar16String)
        {
            const std::u32string Str = GMemory->ReadUTF32(StrAddr, StrLen);
            if (!Str.empty())
                Result = Utils::String::UTF32ToWString(Str);
        }
        else
        {
            const std::u16string Str = GMemory->ReadUTF16(StrAddr, StrLen);
            if (!Str.empty())
                Result = Utils::String::UTF16ToWString(Str);
        }
    }
    else
    {
        const std::string Str = GMemory->ReadUTF8(StrAddr, StrLen);
        if (!Str.empty())
            Result = Utils::String::StringToWString(Str);
    }

    if (!Result.empty() && StrNumber > 0)
        return Result + L'_' + std::to_wstring(StrNumber - 1);

    return Result;
}

// ============================================================
// Bridge state
// ============================================================

std::mutex g_MD7SetupMutex;
std::atomic<bool> g_MD7Initialized{false};
uintptr_t g_MD7ActiveBase = 0;

std::mutex g_MD7CacheMutex;
std::unordered_map<int32_t, std::string> g_MD7NameCache;

/* Fallback base detection so the bridge still self-bootstraps when
 * MD7_Setup was not called explicitly. Same lookup OwenGameServer's
 * InitImageBase() performs (dl_iterate_phdr for libUnreal.so). */
uintptr_t MD7_DetectUnrealBase()
{
    struct Ctx
    {
        uintptr_t Base;
    } C{0};

    dl_iterate_phdr([](struct dl_phdr_info* Info, size_t, void* Data) -> int {
        auto* C = static_cast<Ctx*>(Data);
        if (Info->dlpi_name && std::strstr(Info->dlpi_name, "libUnreal.so"))
        {
            C->Base = Info->dlpi_addr;
            return 1;
        }
        return 0;
    }, &C);

    return C.Base;
}

} // namespace

// ============================================================
// Public C API
// ============================================================

extern "C" {

/**
 * @brief Initializes the MobileDumper-7 FName bridge.
 *
 * Assigns DirectMemory to Project A's GMemory (there is no other backend in
 * this build - MemoryAndroid/KittyMemoryEx are not linked), fixes GNames to
 * imageBase + 0xDC960C0, installs the Fortnite 21.30 FNamePool layout and
 * wires NameArray's ByIndex / GetStr hooks to Project A's pool-walking logic.
 *
 * Idempotent: calling it again with the same (or 0) base is a no-op; calling
 * it with a different base re-points GNames (useful after a module reload).
 *
 * @param imageBase Load base of libUnreal.so (Sarah::ImageBase). 0 = auto-detect.
 * @return 0 on success, -1 base unavailable, -2 maps parse failed, -3 exception.
 */
int MD7_Setup(uintptr_t imageBase)
{
    try
    {
        std::lock_guard<std::mutex> Lock(g_MD7SetupMutex);

        if (g_MD7Initialized.load(std::memory_order_acquire) && (imageBase == 0 || imageBase == g_MD7ActiveBase))
            return 0; // already set up

        if (imageBase == 0)
        {
            imageBase = MD7_DetectUnrealBase();
            if (imageBase == 0)
                return -1;
        }

        auto Mem = std::make_unique<DirectMemory>(imageBase);
        if (!Mem->Initialize())
            return -2;

        /* Project A globals (definitions live in MD7CoreDefs.cpp inside the .a). */
        GMemory  = std::move(Mem);
        GNames   = imageBase + kFnGNamesOffset;
        GObjects = imageBase + kFnGObjectsOffset; // informational; the FName path never reads it

        /* Fortnite 21.30 FNamePool layout. */
        auto Pool     = std::make_unique<FNamePoolLayout>();
        Pool->BlocksBit         = kBlocksBit;
        Pool->MaxChunkIndex     = kMaxChunkIndexOff;
        Pool->ByteCursor        = kByteCursorOff;
        Pool->Blocks            = kBlocksOff;
        Pool->FNameEntry.NameWideMask     = kEntryNameWideMask;
        Pool->FNameEntry.LengthShiftCount = kEntryLengthShift;
        Pool->FNameEntry.Stride           = kEntryStride;
        Pool->FNameEntry.Header           = kEntryHeaderOff;
        Pool->FNameEntry.String           = kEntryStringOff;
        GLayouts.NamesLayout  = std::move(Pool);
        GLayouts.ObjectsLayout = nullptr; // object-array walking stays in OwenGameServer

        /* Engine behaviour flags (Fortnite 21.30, UE5). */
        InternalSettings::bUseNamePool           = true;
        InternalSettings::bUseChar16String       = true;
        InternalSettings::bUseOutlineNumberName  = true;
        InternalSettings::bUseFProperty          = true;
        InternalSettings::bUseCasePreservingName = false;
        GSettings.General.MaxFNameLen            = 1024;

        /* FName field offsets (defaults of FInGenOffsets, restated for clarity). */
        GOffsets.FName.CompIdx = 0;
        GOffsets.FName.Number  = -1;
        GOffsets.FName.SizeOf  = 8;

        /* Same wiring Generator::InitNames() performs in the full dumper. */
        NameArray::SetByIndexFn([](int32_t Index) -> uintptr_t {
            return MD7_GetNameEntryByIndex(Index);
        });

        FNameEntry::SetGetStrFn([](uintptr_t NameEntry) -> std::wstring {
            return MD7_GetNameEntryString(NameEntry);
        });

        /* No DecryptUTF8/16/32, DecryptNameChunk or DecryptNameEntry hooks:
         * Fortnite 21.30 uses an unencrypted FNamePool (the stock profile's
         * decryption overrides are empty). */

        g_MD7ActiveBase = imageBase;
        g_MD7Initialized.store(true, std::memory_order_release);

        return 0;
    }
    catch (...)
    {
        return -3;
    }
}

/**
 * @brief Reads the string of an FName by index through the MobileDumper-7 core.
 *
 * Signature required by OwenGameServer's InSDKUtils::GetNameByIndex:
 *
 *     MD7_ReadFName(int32 index, char* outBuf, int bufSize) -> int
 *
 * Never throws and never dereferences unmapped memory: every read goes through
 * DirectMemory's /proc/self/maps guard, so a bad index or a not-yet-allocated
 * block yields an empty string instead of a SIGSEGV.
 *
 * @param index   FName comparison index (FName::ComparisonIndex).
 * @param outBuf  Destination buffer (always NUL-terminated).
 * @param bufSize Size of outBuf; names longer than bufSize-1 are truncated.
 * @return  >0  number of bytes written (excluding the NUL).
 *            0  entry resolved to an empty string (unreadable/missing entry).
 *           -1  invalid arguments (outBuf null, bufSize <= 0, index < 0).
 *           -2  bridge not initialized and auto-setup failed.
 *           -3  internal exception (reported, never propagated).
 */
int MD7_ReadFName(int32_t index, char* outBuf, int bufSize)
{
    if (outBuf == nullptr || bufSize <= 0)
        return -1;

    outBuf[0] = '\0';

    if (index < 0)
        return -1;

    if (!g_MD7Initialized.load(std::memory_order_acquire))
    {
        if (MD7_Setup(0) != 0)
            return -2;
    }

    try
    {
        /* Fast path: cached names (FName entries are append-only, so the mapping
         * is immutable once observed). */
        {
            std::lock_guard<std::mutex> Lock(g_MD7CacheMutex);
            const auto It = g_MD7NameCache.find(index);
            if (It != g_MD7NameCache.end())
            {
                const int Len = static_cast<int>(It->second.size());
                const int Copy = Len < bufSize - 1 ? Len : bufSize - 1;
                std::memcpy(outBuf, It->second.data(), static_cast<size_t>(Copy));
                outBuf[Copy] = '\0';
                return Copy;
            }
        }

        /* Project A: NameArray::GetNameEntry(int32) -> ByIndexFn -> pool walker,
         * FNameEntry::GetString() -> GetStrFn -> entry decoder, wstring -> UTF-8. */
        FNameEntry Entry = NameArray::GetNameEntry(index);
        const std::string Name = Entry.GetString();

        const int Len  = static_cast<int>(Name.size());
        const int Copy = Len < bufSize - 1 ? Len : bufSize - 1;
        std::memcpy(outBuf, Name.data(), static_cast<size_t>(Copy));
        outBuf[Copy] = '\0';

        /* Cache only successful reads: an empty result usually means the pool
         * wasn't ready yet, and caching it would freeze the failure forever. */
        if (!Name.empty())
        {
            std::lock_guard<std::mutex> Lock(g_MD7CacheMutex);
            if (g_MD7NameCache.size() < kNameCacheLimit)
                g_MD7NameCache.emplace(index, Name);
        }

        return Copy;
    }
    catch (...)
    {
        return -3;
    }
}

} // extern "C"
