#include "pch.h"
#include "Dumper.hpp"
#include "SDK/Engine_classes.hpp"

#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

// ============================================================
// Mini Dumper implementation
// ------------------------------------------------------------
// Everything object/name related routes through ENGINE-NATIVE
// functions (StaticFindObject / StaticLoadObject / a single
// ProcessEvent on the KismetStringLibrary CDO). There is no
// GObjects iteration anywhere in this file.
// ============================================================

namespace Sarah {

std::atomic<uint32_t> GFastArrayIDCounter{1};

ProcessEvent_t ProcessEventPtr = nullptr;

// ------------------------------------------------------------
// Engine-native lookups
// ------------------------------------------------------------
UObject* StaticFind(UClass* cls, const wchar_t* path, bool exactClass) {
    if (!Sarah::ImageBase || !path) return nullptr;
    std::u16string u16 = WToU16(path);
    using t = UObject* (*)(UClass*, UObject*, const char16_t*, bool);
    t fn = (t)(Sarah::ImageBase + Off::StaticFindObject);
    return fn(cls, nullptr, u16.c_str(), exactClass);
}

UObject* StaticLoad(UClass* cls, const wchar_t* path) {
    if (!Sarah::ImageBase || !path) return nullptr;
    std::u16string u16 = WToU16(path);
    using t = UObject* (*)(UClass*, UObject*, const char16_t*, const char16_t*, uint32_t, void*, bool, void*);
    t fn = (t)(Sarah::ImageBase + Off::StaticLoadObjectPub);
    return fn(cls, nullptr, u16.c_str(), nullptr, 0, nullptr, false, nullptr);
}

// ------------------------------------------------------------
// GObjects by-index access (SDK's padded chunked layout:
// Objects field at +0x10, 0x10000 elements per chunk,
// 0x18-byte FUObjectItem). Plain guarded reads only.
// ------------------------------------------------------------
int32_t GObjectsNum() {
    if (!Sarah::ImageBase) return 0;
    TUObjectArray* arr = reinterpret_cast<TUObjectArray*>(Sarah::ImageBase + Off::GObjects);
    int32_t n = arr->NumElements;
    return (n > 0 && n < 4000000) ? n : 0;
}

uint8_t* GetItemByIndex(int32_t index) {
    if (!Sarah::ImageBase || index < 0) return nullptr;
    TUObjectArray* arr = reinterpret_cast<TUObjectArray*>(Sarah::ImageBase + Off::GObjects);
    FUObjectItem** chunks = arr->Objects;
    if (!chunks) return nullptr;
    const int32_t perChunk = TUObjectArray::ElementsPerChunk;
    FUObjectItem* chunk = chunks[index / perChunk];
    if (!chunk) return nullptr;
    return reinterpret_cast<uint8_t*>(chunk + (index % perChunk));
}

UObject* GetObjectByIndex(int32_t index) {
    uint8_t* item = GetItemByIndex(index);
    return item ? *reinterpret_cast<UObject**>(item) : nullptr;
}

// ------------------------------------------------------------
// UObjectManager
// ------------------------------------------------------------
int32_t UObjectManager::Num() {
    return GObjectsNum();
}

UObject* UObjectManager::GetByIndex(int32_t index) {
    return GetObjectByIndex(index);
}

UObject* UObjectManager::Find(const wchar_t* path, UClass* cls) {
    if (!Sarah::ImageBase) return nullptr;
    std::u16string u16 = WToU16(path);
    using t = UObject* (*)(UClass*, UObject*, const char16_t*, bool);
    t fn = (t)(Sarah::ImageBase + Off::StaticFindObject);
    return fn(cls, nullptr, u16.c_str(), false);
}

UObject* UObjectManager::Load(const wchar_t* path, UClass* cls) {
    return StaticLoad(cls, path);
}

UObject* UObjectManager::FindOrLoad(const wchar_t* path, UClass* cls) {
    UObject* obj = Find(path, cls);
    if (obj) return obj;
    return Load(path, cls);
}

void CallProcessEvent(UObject* obj, UFunction* func, void* params) {
    if (!ProcessEventPtr) {
        ProcessEventPtr = (ProcessEvent_t)(Sarah::ImageBase + Off::ProcessEvent);
    }
    if (!obj || !func) return;
    ProcessEventPtr(obj, func, params);
}

// ------------------------------------------------------------
// Encoding helpers (from the old Utils.cpp)
// ------------------------------------------------------------
namespace Enc {

static std::wstring UTF16ToW(const char16_t* s, size_t len) {
    std::wstring out;
    if (!s) return out;
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

} // namespace Sarah

// ============================================================
// KismetStringLibrary safe access
// ------------------------------------------------------------
// The two Conv_* functions are resolved ONCE by full path through
// the engine's StaticFindObject (hash lookup, no array walking)
// and invoked through the raw ProcessEvent pointer. They must
// NEVER be called through the generated SDK wrappers: the wrapper
// resolves its UFunction via UClass::GetFunction(const char*,
// const char*) which calls GetName() on every super-class — and
// GetName() ends up right back here (infinite recursion at
// 4d7e591, the "works then randomly stack-overflows" crash).
// Game-thread use only (ProcessEvent requirement).
// ============================================================

static UObject* KSLClassDefaultObject() {
    static UObject* cdo = nullptr;
    if (!cdo) {
        UClass* cls = (UClass*)Sarah::UObjectManager::Find(L"/Script/Engine.KismetStringLibrary");
        if (cls) cdo = cls->ClassDefaultObject;
    }
    return cdo;
}

static UFunction* KSLFunction(const wchar_t* path) {
    static std::mutex mtx;
    static std::map<std::wstring, UFunction*> cache;
    std::wstring key(path);
    {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = cache.find(key);
        if (it != cache.end()) return it->second;
    }
    UFunction* fn = (UFunction*)Sarah::UObjectManager::Find(path);
    {
        std::lock_guard<std::mutex> lock(mtx);
        cache[key] = fn;
    }
    return fn;
}

// ============================================================
// InSDKUtils — required by the SDK headers (Basic.hpp)
// ============================================================

uintptr_t InSDKUtils::GetImageBase() {
    return Sarah::ImageBase;
}

uintptr_t InSDKUtils::GetGNames() {
    return Sarah::ImageBase + Off::GNames;
}

TUObjectArray* InSDKUtils::GetGObjects() {
    return reinterpret_cast<TUObjectArray*>(Sarah::ImageBase + Off::GObjects);
}

std::string InSDKUtils::GetNameByIndex(int32 Index) {
    // FName -> string. Direct ProcessEvent on the KSL CDO (see the
    // KSLFunction comment above for why the SDK wrapper is poison).
    if (Index <= 0) return std::string();

    UObject* cdo = KSLClassDefaultObject();
    UFunction* fn = KSLFunction(L"/Script/Engine.KismetStringLibrary.Conv_NameToString");
    if (!cdo || !fn) return std::string();

    // Params::UKismetStringLibrary_Conv_NameToString layout:
    //   FName  InName      (0x00)
    //   pad                 (0x04)
    //   FString ReturnValue(0x08)
    struct {
        int32_t InName;
        uint32_t _pad;
        char16_t* RetData;
        int32_t RetNum;
        int32_t RetMax;
    } parms = {};

    parms.InName = Index;

    auto flgs = fn->FunctionFlags;
    fn->FunctionFlags |= 0x400; // FUNC_Native (same dance the SDK wrapper does)
    Sarah::CallProcessEvent(cdo, fn, &parms);
    fn->FunctionFlags = flgs;

    if (!parms.RetData || parms.RetNum <= 0) return std::string();

    std::wstring wide = Sarah::Enc::UTF16ToW(parms.RetData, (size_t)parms.RetNum);
    return std::string(wide.begin(), wide.end());
}

UObject* InSDKUtils::GetObjectByIndex(int32 Index) {
    return Sarah::GetObjectByIndex(Index);
}

// ============================================================
// BasicFilesImplUtils — required by the SDK headers (Basic.hpp)
// ------------------------------------------------------------
// Class lookups go through the engine's own StaticFindObject with
// the "/Script/CoreUObject.Class" meta-class as filter: this
// matches UClass AND its subclasses (BlueprintGeneratedClass),
// is hash-based and thread-safe, and never reads a single name.
// ============================================================

static UClass* GetClassMeta() {
    static UClass* meta = nullptr;
    if (!meta) {
        meta = (UClass*)Sarah::UObjectManager::Find(L"/Script/CoreUObject.Class");
    }
    return meta;
}

static UClass* FindClassEngineNative(const std::string& nameUtf8) {
    if (nameUtf8.empty()) return nullptr;
    UClass* meta = GetClassMeta();

    std::wstring wide(nameUtf8.begin(), nameUtf8.end());

    // 1) Short-name lookup — the engine resolves it through the
    //    object hash table (any thread, no array walk).
    UObject* found = Sarah::StaticFind(meta, wide.c_str(), false);
    if (found) return (UClass*)found;

    // 2) Fallback: probe the known script module prefixes.
    static const wchar_t* const modules[] = {
        L"CoreUObject", L"Engine", L"FortniteGame", L"GameplayAbilities",
        L"AIModule", L"UMG", L"Slate", L"SlateCore", L"NetCore",
        L"GameplayTags", L"GameplayTasks", L"Chaos", L"PhysicsCore",
        L"MovieScene", L"InputCore", L"CoreOnline", L"DeveloperSettings",
        L"AudioExtensions", L"PacketHandler", L"PropertyPath",
        L"CommonUI", L"CommonInput", L"CinematicCamera", L"Niagara",
        L"OnlineSubsystemUtils", L"GameplayDebugger", L"Water",
        L"ModelViewViewModel", L"AnimGraphRuntime", L"AIModule"
    };
    for (const wchar_t* m : modules) {
        std::wstring full = std::wstring(L"/Script/") + m + L"." + wide;
        found = Sarah::StaticFind(meta, full.c_str(), false);
        if (found) return (UClass*)found;
    }
    return nullptr;
}

class UClass* BasicFilesImplUtils::FindClassByName(const std::string& Name, bool bByFullName) {
    if (bByFullName) return FindClassByFullName(Name);
    return FindClassEngineNative(Name);
}

class UClass* BasicFilesImplUtils::FindClassByFullName(const std::string& Name) {
    if (Name.empty()) return nullptr;
    // Full names arrive as "Class /Script/Module.Name" (or a
    // "/Game/..." asset path). Strip the "Class " prefix and hand
    // the path straight to the engine.
    std::string path = Name;
    if (path.rfind("Class ", 0) == 0) path = path.substr(6);
    std::wstring wide(path.begin(), path.end());
    return (UClass*)Sarah::StaticFind(GetClassMeta(), wide.c_str(), false);
}

std::string BasicFilesImplUtils::GetObjectName(class UClass* Class) {
    return Class->GetName();
}

int32 BasicFilesImplUtils::GetObjectIndex(class UClass* Class) {
    return Class->Index;
}

uint64 BasicFilesImplUtils::GetObjFNameAsUInt64(class UClass* Class) {
    return *reinterpret_cast<uint64*>(&Class->Name);
}

class UObject* BasicFilesImplUtils::GetObjectByIndex(int32 Index) {
    return InSDKUtils::GetObjectByIndex(Index);
}

UFunction* BasicFilesImplUtils::FindFunctionByFName(const FName* Name) {
    // Not used anywhere in the gameserver (verified by audit).
    // The old implementation walked the entire GObjects array
    // comparing names — exactly the crash pattern this file kills.
    (void)Name;
    return nullptr;
}

FName BasicFilesImplUtils::StringToName(const TCHAR* Name) {
    if (!Name) return FName{};
    std::u16string u16((const char16_t*)Name);
    std::wstring wide = U16ToW((const char16_t*)Name, u16.size());
    return MakeFName(wide.c_str());
}

FName BasicFilesImplUtils::StringToName(const char* Name) {
    if (!Name) return FName{};
    std::wstring wide;
    for (const char* p = Name; *p; ++p) wide.push_back((wchar_t)(unsigned char)*p);
    return MakeFName(wide.c_str());
}

class UObject* BasicFilesImplUtils::GetDefaultObjectImpl(UClass* Class) {
    if (Class)
        return Class->ClassDefaultObject;
    return nullptr;
}

// ============================================================
// FName construction (game thread; cached). Same pattern as the
// old Utils.cpp — direct ProcessEvent, never the SDK wrapper.
// ============================================================

FName MakeFName(const wchar_t* name) {
    if (!name || !name[0]) return FName{};

    static std::mutex mtx;
    static std::map<std::wstring, int32_t> cache;

    std::wstring wname(name);

    {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = cache.find(wname);
        if (it != cache.end()) return FName(it->second);
    }

    std::u16string u16;
    for (wchar_t wc : wname) {
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

    UObject* cdo = KSLClassDefaultObject();
    UFunction* fn = KSLFunction(L"/Script/Engine.KismetStringLibrary.Conv_StringToName");
    if (!cdo || !fn) {
        LOGE("[Dumper] Conv_StringToName unavailable (CDO=%p Fn=%p)", (void*)cdo, (void*)fn);
        return FName(0);
    }

    // Params::UKismetStringLibrary_Conv_StringToName layout:
    //   FString InString    (0x00)
    //   FName   ReturnValue (0x10)
    struct {
        char16_t* Data;
        int32_t Num;
        int32_t Max;
        int32_t RetIdx;
        int32_t RetNum;
    } parms = {};

    parms.Data = (char16_t*)u16.data();
    parms.Num = (int32_t)(u16.size() - 1);
    parms.Max = (int32_t)u16.size();

    auto flgs = fn->FunctionFlags;
    fn->FunctionFlags |= 0x400;
    Sarah::CallProcessEvent(cdo, fn, &parms);
    fn->FunctionFlags = flgs;

    int32_t idx = parms.RetIdx;

    if (idx > 0) {
        std::lock_guard<std::mutex> lock(mtx);
        cache[wname] = idx;
    }

    return FName(idx);
}

uint32_t MakeFNameIndex(const wchar_t* name) {
    FName n = MakeFName(name);
    return (uint32_t)n.ComparisonIndex;
}

// ============================================================
// Utils — same API the modules already use
// ============================================================

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

    if (!obj || !Sarah::ImageBase) return;

    int32_t index = *(int32_t*)((uint8_t*)obj + 0xC);
    if (index < 0) return;

    uint8_t* item = Sarah::GetItemByIndex(index);
    if (!item) return;

    if (*(void**)item != obj) return;

    out.ObjectIndex = index;
    out.ObjectSerialNumber = *(int32_t*)(item + 0x10) * 2;
}

FString Utils::ToFString(const std::wstring& s) {
    // thread_local: ToFString is called from both the game thread
    // (hooks) and background workers; the old shared static buffer
    // was a data race.
    static thread_local char16_t buf[1024];
    int n = (int)s.size();
    if (n > 1020) n = 1020;

    for (int i = 0; i < n; i++) {
        buf[i] = (char16_t)s[i];
    }
    buf[n] = 0;

    struct FStringRaw {
        char16_t* Data;
        int32_t Num;
        int32_t Max;
    };

    FString result;
    FStringRaw* raw = (FStringRaw*)&result;
    raw->Data = buf;
    raw->Num = n;
    raw->Max = n + 1;
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

// ============================================================
// SDK::FWeakObjectPtr — required by the SDK headers
// ============================================================

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

} // namespace SDK

// ============================================================
// UC containers — same allocator policy as 4d7e591:
// libc realloc for OUR arrays, free is a no-op (engine-owned
// out-param storage is intentionally leaked — a cross-allocator
// free is what corrupts FMalloc's heap).
// ============================================================

namespace UC {

void* ContainerRealloc(void* ptr, int64 newLen, uint32 alignment) {
    (void)alignment;
    return std::realloc(ptr, (size_t)newLen);
}

void ContainerFree(void* ptr) {
    (void)ptr;
}

} // namespace UC
