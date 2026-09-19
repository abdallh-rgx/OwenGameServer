#include "pch.h"
#include "UObject.hpp"
#include "FName.hpp"

namespace Sarah {

ProcessEvent_t ProcessEventPtr = nullptr;

int32_t UObjectManager::Num() {
    return GObjectsNum();
}

UObject* UObjectManager::GetByIndex(int32_t index) {
    return GetObjectByIndex(index);
}

UObject* UObjectManager::Find(const wchar_t* path, UClass* cls) {
    if (!Sarah::ImageBase) return nullptr;
    std::u16string u16 = WToU16(path);
    using t = UObject*(*)(UClass*, UObject*, const char16_t*, bool);
    t fn = (t)(Sarah::ImageBase + Off::StaticFindObject);
    return fn(cls, nullptr, u16.c_str(), false);
}

UObject* UObjectManager::Load(const wchar_t* path, UClass* cls) {
    if (!Sarah::ImageBase) return nullptr;
    std::u16string u16 = WToU16(path);
    using t = UObject*(*)(UClass*, UObject*, const char16_t*, const char16_t*, uint32_t, void*, bool, void*);
    t fn = (t)(Sarah::ImageBase + Off::StaticLoadObjectPub);
    return fn(cls, nullptr, u16.c_str(), nullptr, 0, nullptr, false, nullptr);
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

}

uintptr_t InSDKUtils::GetImageBase() {
    return Sarah::ImageBase;
}

uintptr_t InSDKUtils::GetGNames() {
    return Sarah::ImageBase + Off::GNames;
}

TUObjectArray* InSDKUtils::GetGObjects() {
    return reinterpret_cast<TUObjectArray*>(Sarah::ImageBase + Off::GObjects);
}

namespace {

inline bool IsAddressSane(uintptr_t addr) {
    return addr >= 0x1000u && addr < 0x0000800000000000ull;
}

std::string DirectGetNameByIndex(int32 Index) {
    if (Index < 0) return std::string();
    if (!Sarah::ImageBase) return std::string();

    const uintptr_t poolBase = (uintptr_t)(Sarah::ImageBase + Off::GNames);
    if (!IsAddressSane(poolBase)) return std::string();

    const uintptr_t blocksAddr = poolBase + (uintptr_t)Off::FNamePool_Blocks;
    if (!IsAddressSane(blocksAddr)) return std::string();

    const uint32_t blockIdx = ((uint32_t)Index) >> (uint32_t)Off::FNamePool_BlocksBit;
    const uint32_t entryIdx = ((uint32_t)Index) & ((1u << (uint32_t)Off::FNamePool_BlocksBit) - 1u);

    if (blockIdx >= 0x2000u) return std::string();

    const uintptr_t blockSlotAddr = blocksAddr + (uintptr_t)blockIdx * sizeof(void*);
    if (!IsAddressSane(blockSlotAddr)) return std::string();

    const uint8_t* block = *(const uint8_t**)(blockSlotAddr);
    if (!block) return std::string();
    if (!IsAddressSane((uintptr_t)block)) return std::string();

    const uint64_t byteOffset = (uint64_t)entryIdx * (uint64_t)Off::FNameEntry_Stride;
    const uint64_t blockSize  = (uint64_t)Off::FNameEntry_Stride << (uint32_t)Off::FNamePool_BlocksBit;
    if (byteOffset >= blockSize) return std::string();

    const uint8_t* entryPtr = block + byteOffset;

    const uint16_t header = *(const uint16_t*)(entryPtr + (uintptr_t)Off::FNameEntry_Header);
    const bool isWide     = (header & (uint16_t)Off::FNameEntry_NameWideMask) != 0;
    const uint32_t len    = (uint32_t)(header >> (uint32_t)Off::FNameEntry_LengthShift);

    if (len == 0 || len > 1024u) return std::string();

    const uint64_t nameBytes = isWide ? ((uint64_t)len * 2ull) : (uint64_t)len;
    if ((uint64_t)Off::FNameEntry_String + nameBytes > blockSize - byteOffset)
        return std::string();

    const uint8_t* nameData = entryPtr + (uintptr_t)Off::FNameEntry_String;

    std::string result;
    result.reserve((size_t)len);

    if (isWide) {
        const char16_t* w = (const char16_t*)nameData;
        for (uint32_t i = 0; i < len; i++) {
            const char16_t c = w[i];
            if (c == 0) break;
            if (c < 0x80) {
                result.push_back((char)c);
            } else if (c < 0x800) {
                result.push_back((char)(0xC0 | (c >> 6)));
                result.push_back((char)(0x80 | (c & 0x3F)));
            } else {
                result.push_back((char)(0xE0 | (c >> 12)));
                result.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
                result.push_back((char)(0x80 | (c & 0x3F)));
            }
        }
    } else {
        const char* n = (const char*)nameData;
        for (uint32_t i = 0; i < len; i++) {
            const char c = n[i];
            if (c == 0) break;
            result.push_back(c);
        }
    }

    return result;
}

}

std::string InSDKUtils::GetNameByIndex(int32 Index) {
    return DirectGetNameByIndex(Index);
}

UObject* InSDKUtils::GetObjectByIndex(int32 Index) {
    return Sarah::GetObjectByIndex(Index);
}

class UClass* BasicFilesImplUtils::FindClassByName(const std::string& Name, bool bByFullName) {
    return bByFullName ? UObject::FindClass(Name) : UObject::FindClassFast(Name);
}

class UClass* BasicFilesImplUtils::FindClassByFullName(const std::string& Name) {
    return UObject::FindClass(Name);
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
    for (int i = 0; i < InSDKUtils::GetGObjects()->Num(); ++i) {
        UObject* Object = InSDKUtils::GetObjectByIndex(i);
        if (!Object)
            continue;
        if (Object->Name == *Name)
            return static_cast<UFunction*>(Object);
    }
    return nullptr;
}

FName BasicFilesImplUtils::StringToName(const TCHAR* Name) {
    std::u16string u16((const char16_t*)Name);
    std::wstring wide = U16ToW((const char16_t*)Name, u16.size());
    return MakeFName(wide.c_str());
}

class UObject* BasicFilesImplUtils::GetDefaultObjectImpl(UClass* Class) {
    if (Class)
        return Class->ClassDefaultObject;
    return nullptr;
}
