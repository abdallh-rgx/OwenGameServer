#pragma once
#include <cstdint>
#include <string>
#include "Offsets.hpp"
#include "AndroidBase.hpp"

// UObject/UClass/UFunction come from the SDK (include/SDK.hpp) and are visible
// through `using namespace SDK;` in pch.h. Re-declaring them in the global
// namespace would make every unqualified use ambiguous, so no forward
// declarations are made here.

using ProcessEvent_t = void(*)(UObject*, UFunction*, void*);

namespace Sarah {

struct FFixedUObjectArrayLayout {
    bool Padded = true;
    int32_t ElementsPerChunk = 0x10000;
    bool Initialized = false;
    bool Valid = false;
};

inline FFixedUObjectArrayLayout GObjectsLayout;

inline uint8_t* GObjectsBase() {
    return (uint8_t*)(Sarah::ImageBase + Off::GObjects);
}

inline void ReadGObjectsHeader(bool padded, int32_t& maxElements, int32_t& numElements, void** objects) {
    uint8_t* base = GObjectsBase();
    if (padded) {
        objects = objects;
        *objects = *(void***)(base + 0x10);
        maxElements = *(int32_t*)(base + 0x20);
        numElements = *(int32_t*)(base + 0x24);
    } else {
        *objects = *(void***)(base + 0x00);
        maxElements = *(int32_t*)(base + 0x10);
        numElements = *(int32_t*)(base + 0x14);
    }
}

inline int32_t GObjectsNum() {
    int32_t maxE = 0, numE = 0;
    void* objs = nullptr;
    ReadGObjectsHeader(GObjectsLayout.Padded, maxE, numE, &objs);
    if (numE < 0 || numE > 4000000) return 0;
    return numE;
}

inline uint8_t* GetItemByIndex(int32_t index) {
    if (!GObjectsLayout.Initialized) return nullptr;
    if (index < 0) return nullptr;

    uint8_t* base = GObjectsBase();
    uint64_t objectsField = GObjectsLayout.Padded ? 0x10 : 0x00;

    void** chunks = *(void***)(base + objectsField);
    if (!chunks) return nullptr;

    int32_t perChunk = GObjectsLayout.ElementsPerChunk;
    if (perChunk <= 0) perChunk = 0x10000;

    int32_t chunkIdx = index / perChunk;
    int32_t within = index % perChunk;

    uint8_t* chunk = (uint8_t*)chunks[chunkIdx];
    if (!chunk) return nullptr;

    return chunk + (uint64_t)within * 0x18;
}

inline UObject* GetObjectByIndex(int32_t index) {
    if (!GObjectsLayout.Initialized) return nullptr;
    if (index < 0) return nullptr;

    uint8_t* base = GObjectsBase();
    uint64_t objectsField = GObjectsLayout.Padded ? 0x10 : 0x00;

    void** chunks = *(void***)(base + objectsField);
    if (!chunks) return nullptr;

    int32_t perChunk = GObjectsLayout.ElementsPerChunk;
    if (perChunk <= 0) perChunk = 0x10000;

    int32_t chunkIdx = index / perChunk;
    int32_t within = index % perChunk;

    uint8_t* chunk = (uint8_t*)chunks[chunkIdx];
    if (!chunk) return nullptr;

    return *(UObject**)(chunk + (uint64_t)within * 0x18);
}

inline bool ValidateGObjectsLayout(bool padded) {
    int32_t maxE = 0, numE = 0;
    void* chunks = nullptr;
    ReadGObjectsHeader(padded, maxE, numE, &chunks);
    if (!chunks) return false;
    if (numE <= 0 || numE > 4000000) return false;

    void** chunkArray = (void**)chunks;
    if (!chunkArray[0]) return false;

    UObject* first = *(UObject**)((uint8_t*)chunkArray[0]);
    if (!first) return false;

    uint64_t vtable = *(uint64_t*)first;
    if (vtable < Sarah::ImageBase || vtable > Sarah::ImageBase + 0x20000000) return false;

    return true;
}

inline bool InitGObjectsLayout() {
    if (ValidateGObjectsLayout(true)) {
        GObjectsLayout.Padded = true;
        uint8_t* base = GObjectsBase();
        int32_t runtimePerChunk = *(int32_t*)(base + 0x30);
        if (runtimePerChunk > 0 && (runtimePerChunk & (runtimePerChunk - 1)) == 0) {
            GObjectsLayout.ElementsPerChunk = runtimePerChunk;
        }
    } else if (ValidateGObjectsLayout(false)) {
        GObjectsLayout.Padded = false;
        uint8_t* base = GObjectsBase();
        int32_t runtimePerChunk = *(int32_t*)(base + 0x20);
        if (runtimePerChunk > 0 && (runtimePerChunk & (runtimePerChunk - 1)) == 0) {
            GObjectsLayout.ElementsPerChunk = runtimePerChunk;
        }
    } else {
        GObjectsLayout.Padded = true;
        GObjectsLayout.ElementsPerChunk = 0x10000;
        GObjectsLayout.Initialized = true;
        GObjectsLayout.Valid = false;
        return false;
    }
    GObjectsLayout.Initialized = true;
    GObjectsLayout.Valid = true;
    return true;
}

class UObjectManager {
public:
    static int32_t Num();
    static UObject* GetByIndex(int32_t index);
    static UObject* Find(const wchar_t* path, UClass* cls = nullptr);
    static UObject* Load(const wchar_t* path, UClass* cls = nullptr);
    static UObject* FindOrLoad(const wchar_t* path, UClass* cls = nullptr);
};

extern ProcessEvent_t ProcessEventPtr;

void CallProcessEvent(UObject* obj, UFunction* func, void* params);

}
