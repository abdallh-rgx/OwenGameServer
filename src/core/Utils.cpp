#include "pch.h"
#include "Utils.hpp"
#include "UObject.hpp"
#include "FName.hpp"
#include "SDK/Engine_classes.hpp"

#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>

namespace Sarah {

std::atomic<uint32_t> GFastArrayIDCounter{1};

void* EngineRealloc(void* ptr, int64_t newLen, uint32_t alignment) {
    if (newLen == 0) {
        if (ptr) {
            if (alignment > 16) {
                void* original = *((void**)ptr - 1);
                free(original);
            } else {
                free(ptr);
            }
        }
        return nullptr;
    }

    if (alignment <= 16) {
        return realloc(ptr, (size_t)newLen);
    }

    void* original = ptr ? *((void**)ptr - 1) : nullptr;

    const size_t totalSize = (size_t)newLen + (size_t)alignment - 1 + sizeof(void*);
    void* raw = realloc(original, totalSize);
    if (!raw) return nullptr;

    uintptr_t addr = ((uintptr_t)raw + sizeof(void*) + (uintptr_t)alignment - 1)
                   & ~((uintptr_t)alignment - 1);

    *((void**)addr - 1) = raw;
    return (void*)addr;
}

namespace Enc {

inline std::wstring UTF8ToW(const char* s, size_t len) {
    std::wstring out;
    out.reserve(len);
    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp = 0;
        int extra = 0;
        if (c < 0x80) { cp = c; extra = 0; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
        else { i++; continue; }
        i++;
        for (int j = 0; j < extra && i < len; j++, i++)
            cp = (cp << 6) | ((unsigned char)s[i] & 0x3F);
        out.push_back((wchar_t)cp);
    }
    return out;
}

inline std::string WToUTF8(const std::wstring& s) {
    std::string out;
    out.reserve(s.size());
    for (wchar_t wc : s) {
        uint32_t cp = (uint32_t)wc;
        if (cp < 0x80) {
            out.push_back((char)cp);
        } else if (cp < 0x800) {
            out.push_back((char)(0xC0 | (cp >> 6)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back((char)(0xE0 | (cp >> 12)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        } else {
            out.push_back((char)(0xF0 | (cp >> 18)));
            out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

inline std::wstring UTF16ToW(const char16_t* s, size_t len) {
    std::wstring out;
    out.reserve(len);
    for (size_t i = 0; i < len; i++) {
        char16_t c = s[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < len) {
            char16_t c2 = s[i + 1];
            if (c2 >= 0xDC00 && c2 <= 0xDFFF) {
                uint32_t cp = 0x10000u + (((uint32_t)(c - 0xD800) << 10) | (uint32_t)(c2 - 0xDC00));
                out.push_back((wchar_t)cp);
                i++;
                continue;
            }
        }
        out.push_back((wchar_t)c);
    }
    return out;
}

} // namespace Enc

namespace NameRaw {

constexpr int32     kBlocksBit   = 0x10;
constexpr int32     kStride      = 0x4;
constexpr int32     kHeaderOff   = 0x0;
constexpr int32     kStringOff   = 0x4;
constexpr int32     kLengthShift = 0x6;
constexpr uint16_t  kWideMask    = 0x1;
constexpr int32     kMaxNameLen  = 255;
constexpr uintptr_t kBlocksOff   = 0x40;

inline uintptr_t GetPool() {
    return (uintptr_t)(Sarah::ImageBase + (uintptr_t)Off::GNames);
}

inline uintptr_t GetBlocksBase() {
    return *(uintptr_t*)(GetPool() + kBlocksOff);
}

inline uintptr_t GetBlock(int32 blockIdx) {
    uintptr_t base = GetBlocksBase();
    if (!base) return 0;
    return *(uintptr_t*)(base + (uintptr_t)blockIdx * sizeof(void*));
}

inline std::string ReadByIndex(int32 index) {
    if (index < 0) return "";

    const int32 blockIdx = index >> kBlocksBit;
    const int32 inBlock  = (index & ((1 << kBlocksBit) - 1)) * kStride;

    uintptr_t block = GetBlock(blockIdx);
    if (!block) return "";

    uintptr_t entry = block + (uintptr_t)inBlock;

    uint16_t hdr = 0;
    std::memcpy(&hdr, (void*)(entry + kHeaderOff), sizeof(hdr));

    int32 len  = (int32)(hdr >> kLengthShift);
    bool  wide = (hdr & kWideMask) != 0;

    if (len <= 0 || len > kMaxNameLen) return "";

    const void* src = (void*)(entry + kStringOff);

    if (wide) {
        std::wstring w = Enc::UTF16ToW((const char16_t*)src, (size_t)len);
        return Enc::WToUTF8(w);
    }

    return std::string((const char*)src, (size_t)len);
}

} // namespace NameRaw

namespace NameIndex {

std::unordered_map<std::wstring, int32_t> g_map;
std::mutex                                g_mtx;
std::atomic<bool>                         g_built{false};

void BuildOnce() {
    if (g_built.load(std::memory_order_acquire)) return;

    std::lock_guard<std::mutex> lock(g_mtx);
    if (g_built.load(std::memory_order_relaxed)) return;

    const int32 total = 0x200000;
    g_map.reserve((size_t)total);

    for (int32 i = 0; i < total; ++i) {
        std::string s = NameRaw::ReadByIndex(i);
        if (s.empty()) continue;

        std::wstring ws = Enc::UTF8ToW(s.c_str(), s.size());
        if (ws.empty()) continue;

        g_map.try_emplace(std::move(ws), i);
    }

    g_built.store(true, std::memory_order_release);
}

int32 Lookup(const std::wstring& target) {
    BuildOnce();

    std::lock_guard<std::mutex> lock(g_mtx);
    auto it = g_map.find(target);
    return it == g_map.end() ? 0 : it->second;
}

} // namespace NameIndex

} // namespace Sarah

FName MakeFName(const wchar_t* name) {
    if (!name || !name[0]) return FName{};

    static std::mutex                      cacheMtx;
    static std::map<std::wstring, int32_t> cache;

    std::wstring wname(name);

    {
        std::lock_guard<std::mutex> lock(cacheMtx);
        auto it = cache.find(wname);
        if (it != cache.end()) return FName(it->second);
    }

    int32_t index = Sarah::NameIndex::Lookup(wname);

    if (index > 0) {
        std::lock_guard<std::mutex> lock(cacheMtx);
        cache[wname] = index;
    }

    return FName(index);
}

uint32_t MakeFNameIndex(const wchar_t* name) {
    FName n = MakeFName(name);
    return (uint32_t)n.ComparisonIndex;
}

UObject* Utils::FindObject(const wchar_t* path, UClass* cls) {
    return Sarah::UObjectManager::Find(path, cls);
}

UObject* Utils::LoadObject(const wchar_t* path, UClass* cls) {
    return Sarah::UObjectManager::Load(path, cls);
}

UObject* Utils::FindOrLoad(const wchar_t* path, UClass* cls) {
    return Sarah::UObjectManager::FindOrLoad(path, cls);
}

AActor* Utils::SpawnActor(UClass* cls, const FVector& loc, const FRotator& rot, AActor* owner) {
    if (!cls) return nullptr;

    struct FSpawnParams {
        FName                    Name;
        uint32_t                 _pad0;
        AActor*                  Template;
        AActor*                  Owner;
        SDK::APawn*              Instigator;
        void*                    OverrideLevel;
        SDK::UActorComponent*    OverrideParentComponent;
        uint8_t                  SpawnCollisionHandlingOverride;
        uint8_t                  BitFlags;
        uint8_t                  NameMode;
        uint8_t                  _pad1;
        uint32_t                 ObjectFlags;
    };

    FSpawnParams params = {};
    params.Owner = owner;
    params.SpawnCollisionHandlingOverride = 1;
    params.NameMode = 3;

    CoreUObject::FTransform transform = MakeTransform(loc, rot);

    using SpawnFn = AActor* (*)(UWorld*, UClass*, CoreUObject::FTransform*, FSpawnParams*);
    static SpawnFn fn = nullptr;
    if (!fn) fn = (SpawnFn)(Sarah::ImageBase + Off::UWorld_SpawnActor);

    UWorld* world = UWorld::GetWorld();
    if (!world) return nullptr;

    return fn(world, cls, &transform, &params);
}

std::vector<AActor*> Utils::GetAllActors(UClass* cls) {
    std::vector<AActor*> out;
    if (!cls) return out;

    UWorld* world = UWorld::GetWorld();
    if (!world) return out;

    TArray<AActor*> actors;
    UGameplayStatics::GetAllActorsOfClass(world, cls, &actors);
    for (int i = 0; i < actors.Num(); i++) out.push_back(actors[i]);
    actors.Free();
    return out;
}

float Utils::EvaluateScalableFloat(FScalableFloat& value) {
    if (!value.Curve.CurveTable)
        return value.Value;

    float out = 0.f;
    FString ctx;
    UDataTableFunctionLibrary::EvaluateCurveTableRow(
        value.Curve.CurveTable, value.Curve.RowName, 0.f, nullptr, &out, ctx);
    return out;
}

float Utils::EvaluateCurve(FCurveTableRowHandle& handle, float inTime) {
    if (!handle.CurveTable)
        return 0.f;

    float out = 0.f;
    FString ctx;
    UDataTableFunctionLibrary::EvaluateCurveTableRow(
        handle.CurveTable, handle.RowName, inTime, nullptr, &out, ctx);
    return out;
}

void MakeWeakPtrInto(FWeakObjectPtr& out, void* obj) {
    out.ObjectIndex = 0;
    out.ObjectSerialNumber = 0;

    if (!obj || !Sarah::GObjectsLayout.Initialized) return;

    int32_t index = *(int32_t*)((uint8_t*)obj + 0xC);
    if (index < 0) return;

    uint8_t* item = Sarah::GetItemByIndex(index);
    if (!item) return;

    if (*(void**)item != obj) return;

    out.ObjectIndex = index;
    out.ObjectSerialNumber = *(int32_t*)(item + 0x10) * 2;
}

FString Utils::ToFString(const std::wstring& s) {
    std::u16string u16;
    for (wchar_t wc : s) {
        uint32_t cp = (uint32_t)wc;
        if (cp <= 0xFFFF) {
            u16.push_back((char16_t)cp);
        } else {
            cp -= 0x10000;
            u16.push_back((char16_t)(0xD800 + (cp >> 10)));
            u16.push_back((char16_t)(0xDC00 + (cp & 0x3FF)));
        }
    }
    u16.push_back(u'\0');

    FString result;
    for (char16_t c : u16) result.Add(c);
    return result;
}

std::wstring Utils::FromFString(const FString& s) {
    if (!s) return L"";
    int32 n = s.Num();
    const char16_t* data = (const char16_t*)s.GetData();
    if (!data) return L"";
    std::u16string u16;
    for (int32 i = 0; i < n; i++) {
        if (data[i] == 0) break;
        u16.push_back(data[i]);
    }
    return Sarah::Enc::UTF16ToW(u16.c_str(), u16.size());
}

bool Utils::TagContainerHasTag(const FGameplayTagContainer& container, const wchar_t* tagName) {
    if (container.GameplayTags.Num() == 0) return false;
    FName tag = MakeFName(tagName);
    if (tag.ComparisonIndex == 0) return false;
    for (int i = 0; i < container.GameplayTags.Num(); i++) {
        if (container.GameplayTags[i].TagName == tag)
            return true;
    }
    return false;
}

bool Utils::TagContainerHasAll(const FGameplayTagContainer& container, const FGameplayTagContainer& required) {
    if (required.GameplayTags.Num() == 0) return true;
    for (int i = 0; i < required.GameplayTags.Num(); i++) {
        bool found = false;
        for (int j = 0; j < container.GameplayTags.Num(); j++) {
            if (container.GameplayTags[j].TagName == required.GameplayTags[i].TagName) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

void Utils::MarkItemDirty(FFastArraySerializer& serializer, FFastArraySerializerItem& item) {
    int32_t& arrayKey = *(int32_t*)((uint8_t*)&serializer + 0x54);
    arrayKey++;
    if (item.ReplicationID == -1) {
        item.ReplicationID = Sarah::GFastArrayIDCounter.fetch_add(1);
    }
    item.ReplicationKey = Sarah::GFastArrayIDCounter.fetch_add(1);
    item.MostRecentArrayReplicationKey = arrayKey;
}

void Utils::MarkArrayDirty(FFastArraySerializer& serializer) {
    int32_t& arrayKey = *(int32_t*)((uint8_t*)&serializer + 0x54);
    arrayKey++;
}

namespace SDK {

UObject* FWeakObjectPtr::Get() const {
    if (ObjectSerialNumber == 0 || ObjectIndex < 0)
        return nullptr;

    uint8_t* item = Sarah::GetItemByIndex(ObjectIndex);
    if (!item) return nullptr;

    UObject* obj = *(UObject**)item;
    if (!obj) return nullptr;

    int32_t serial = *(int32_t*)(item + 0x10) * 2;
    if (serial != ObjectSerialNumber) return nullptr;

    return obj;
}

bool FWeakObjectPtr::IsValid() const {
    return Get() != nullptr;
}

bool FWeakObjectPtr::operator==(const FWeakObjectPtr& Other) const {
    return ObjectIndex == Other.ObjectIndex
        && ObjectSerialNumber == Other.ObjectSerialNumber;
}

bool FWeakObjectPtr::operator!=(const FWeakObjectPtr& Other) const {
    return !(*this == Other);
}

bool FWeakObjectPtr::operator==(const class UObject* Other) const {
    return Get() == Other;
}

bool FWeakObjectPtr::operator!=(const class UObject* Other) const {
    return Get() != Other;
}

}

namespace UC {

void* ContainerRealloc(void* ptr, int64 newLen, uint32 alignment) {
    (void)alignment;
    return std::realloc(ptr, (size_t)newLen);
}

void ContainerFree(void* ptr) {
    (void)ptr;
}

}
