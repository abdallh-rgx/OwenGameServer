#include "pch.h"
#include "UObject.hpp"
#include "FName.hpp"

#include <unordered_map>
#include <mutex>

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

struct FNameToStringParams {
    int32_t   NameIndex;
    int32_t   _Padding;
    char16_t* Data;
    int32_t   Num;
    int32_t   Max;
};

std::mutex                                 g_getname_mutex;
std::unordered_map<int32_t, std::string>   g_getname_cache;
UFunction*                                 g_conv_name_to_string_fn = nullptr;
UObject*                                   g_kismet_string_cdo     = nullptr;
bool                                       g_conv_init_attempted   = false;
thread_local bool                          g_in_getname            = false;

bool InitConvNameToString() {
    if (g_conv_name_to_string_fn && g_kismet_string_cdo) return true;
    if (g_conv_init_attempted) return false;
    g_conv_init_attempted = true;

    UClass* cls = (UClass*)Sarah::UObjectManager::Find(L"/Script/Engine.KismetStringLibrary");
    if (!cls) return false;

    UObject* cdo = cls->ClassDefaultObject;
    if (!cdo) return false;

    UFunction* fn = (UFunction*)Sarah::UObjectManager::Find(
        L"/Script/Engine.KismetStringLibrary.Conv_NameToString");
    if (!fn) return false;

    g_kismet_string_cdo     = cdo;
    g_conv_name_to_string_fn = fn;
    return true;
}

std::string SafeGetNameByIndex(int32 Index) {
    if (Index < 0) return "";

    if (g_in_getname) return "";
    g_in_getname = true;

    {
        std::lock_guard<std::mutex> lock(g_getname_mutex);
        auto it = g_getname_cache.find(Index);
        if (it != g_getname_cache.end()) {
            g_in_getname = false;
            return it->second;
        }
    }

    std::string result;
    if (InitConvNameToString()) {
        FNameToStringParams parms = {};
        parms.NameIndex = Index;

        Sarah::CallProcessEvent(g_kismet_string_cdo, g_conv_name_to_string_fn, &parms);

        if (parms.Data && parms.Num > 0) {
            int32_t len = parms.Num;
            if (len > 4096) len = 4096;
            result.reserve((size_t)len);
            const char16_t* w = parms.Data;
            for (int32_t i = 0; i < len; i++) {
                char16_t c = w[i];
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
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_getname_mutex);
        g_getname_cache[Index] = result;
    }

    g_in_getname = false;
    return result;
}

}

std::string InSDKUtils::GetNameByIndex(int32 Index) {
    return SafeGetNameByIndex(Index);
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
