#include "pch.h"
#include "Utils.hpp"
#include "UObject.hpp"
#include "FName.hpp"

#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <atomic>
#include <string>

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
    std::u16string u16 = WToU16(s);
    u16.push_back(u'\0');
    FString result;
    for (char16_t c : u16) result.Add(c);
    return result;
}

std::wstring Utils::FromFString(const FString& s) {
    if (!s) return L"";
    std::u16string u16;
    int32 n = s.Num();
    const char16_t* data = (const char16_t*)s.GetData();
    if (!data) return L"";
    for (int32 i = 0; i < n; i++) {
        if (data[i] == 0) break;
        u16.push_back(data[i]);
    }
    return U16ToW(u16.c_str(), u16.size());
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

    std::u16string u16 = WToU16(name);
    if (u16.empty()) return FName{};
    u16.push_back(u'\0');

    struct FStringRaw {
        char16_t* Data;
        int32     NumElements;
        int32     MaxElements;
    };

    FString fs;
    FStringRaw& raw = reinterpret_cast<FStringRaw&>(fs);
    raw.Data        = u16.data();
    raw.NumElements = (int32)u16.size();
    raw.MaxElements = raw.NumElements;

    FName result = UKismetStringLibrary::Conv_StringToName(fs);

    if (result.ComparisonIndex != 0) {
        std::lock_guard<std::mutex> lock(mtx);
        cache[wname] = result.ComparisonIndex;
    }

    return result;
}

uint32_t MakeFNameIndex(const wchar_t* name) {
    FName n = MakeFName(name);
    return (uint32_t)n.ComparisonIndex;
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
