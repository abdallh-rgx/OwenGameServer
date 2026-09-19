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

static std::string DirectGetNameByIndex(int32 Index) {
    if (Index < 0) return "";

    uint8_t* poolBase = (uint8_t*)(Sarah::ImageBase + Off::GNames);
    if (!poolBase) return "";

    uint8_t** blocks = *(uint8_t***)(poolBase + Off::FNamePool_Blocks);
    if (!blocks) return "";

    uint32_t blockIdx = ((uint32_t)Index) >> 16;
    uint32_t offsetInBlock = ((uint32_t)Index) & 0xFFFF;

    if (blockIdx >= 8192) return "";

    uint8_t* block = blocks[blockIdx];
    if (!block) return "";

    uint8_t* entryPtr = block + (uint64_t)offsetInBlock * Off::FNameEntry_Stride;
    if (!entryPtr) return "";

    uint16_t header = *(uint16_t*)entryPtr;
    bool isWide = (header & 0x01) != 0;
    uint32_t len = header >> 1;

    if (len == 0) {
        uint16_t header2 = *(uint16_t*)(entryPtr + 2);
        isWide = (header2 & 0x01) != 0;
        len = header2 >> 1;
        entryPtr += 2;
    }

    if (len == 0 || len > 4096) return "";

    uint8_t* nameData = entryPtr + 2;
    std::string result;
    result.reserve(len);

    if (isWide) {
        char16_t* name = (char16_t*)nameData;
        for (uint32_t i = 0; i < len; i++) {
            char16_t c = name[i];
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
        char* name = (char*)nameData;
        for (uint32_t i = 0; i < len; i++) {
            char c = name[i];
            if (c == 0) break;
            result.push_back(c);
        }
    }

    return result;
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
