#pragma once
#include <cstdint>
#include <string>
#include <atomic>
#include "Offsets.hpp"
#include "AndroidBase.hpp"

namespace Sarah {

struct FNameEntryHeader {
    uint16_t Raw;
    int Length(int shift) const { return Raw >> shift; }
    bool IsWide() const { return (Raw & 1) != 0; }
};

struct FNameRuntime {
    int LenShift = 1;
    uint64_t BlocksOffset = 0x10;
    uint64_t CursorOffset = 0xC;
    bool BlocksArePointerMember = false;
    bool Initialized = false;
    bool Valid = false;
};

inline FNameRuntime FNameRT;

inline void* FNameBlockPtr(uint32_t blockIndex) {
    uintptr_t pool = Sarah::ImageBase + Off::GNames;
    if (FNameRT.BlocksArePointerMember) {
        uintptr_t blocksArray = *(uintptr_t*)(pool + FNameRT.BlocksOffset);
        if (!blocksArray) return nullptr;
        return *(void**)(blocksArray + (uint64_t)blockIndex * 8);
    }
    return *(void**)(pool + FNameRT.BlocksOffset + (uint64_t)blockIndex * 8);
}

inline bool FNameValidateEntry0(int lenShift, uint64_t blocksOffset, uint64_t cursorOffset, bool pointerMember) {
    uintptr_t pool = Sarah::ImageBase + Off::GNames;
    void* block0 = nullptr;
    if (pointerMember) {
        uintptr_t arr = *(uintptr_t*)(pool + blocksOffset);
        if (!arr) return false;
        block0 = *(void**)arr;
    } else {
        block0 = *(void**)(pool + blocksOffset);
    }
    if (!block0) return false;

    FNameEntryHeader header = *(FNameEntryHeader*)block0;
    if (header.IsWide()) return false;
    int len = header.Length(lenShift);
    if (len != 4) return false;

    const char16_t* str = (const char16_t*)((uint8_t*)block0 + 2);
    return str[0] == u'N' && str[1] == u'o' && str[2] == u'n' && str[3] == u'e';
}

inline bool InitFNameRuntime() {
    struct FCandidate {
        int lenShift;
        uint64_t blocksOffset;
        uint64_t cursorOffset;
        bool pointerMember;
    };

    const FCandidate candidates[] = {
        {1, 0x10, 0xC, false},
        {1, 0x40, 0x3C, false},
        {1, 0x40, 0x3C, true},
        {6, 0x10, 0xC, false},
        {6, 0x40, 0x3C, false},
        {6, 0x40, 0x3C, true},
    };

    for (const auto& c : candidates) {
        if (FNameValidateEntry0(c.lenShift, c.blocksOffset, c.cursorOffset, c.pointerMember)) {
            FNameRT.LenShift = c.lenShift;
            FNameRT.BlocksOffset = c.blocksOffset;
            FNameRT.CursorOffset = c.cursorOffset;
            FNameRT.BlocksArePointerMember = c.pointerMember;
            FNameRT.Initialized = true;
            FNameRT.Valid = true;
            return true;
        }
    }

    FNameRT.LenShift = 1;
    FNameRT.BlocksOffset = 0x10;
    FNameRT.CursorOffset = 0xC;
    FNameRT.BlocksArePointerMember = false;
    FNameRT.Initialized = true;
    FNameRT.Valid = false;
    return false;
}

class FNameReader {
public:
    static constexpr int BlockBits = 16;
    static constexpr uint32_t BlockSize = 1u << BlockBits;

    static void* GetEntryAddress(int32_t comparisonIndex) {
        if (comparisonIndex <= 0) return nullptr;
        if (!FNameRT.Initialized && !InitFNameRuntime()) return nullptr;

        uint32_t blockIdx = (uint32_t)comparisonIndex >> BlockBits;
        uint32_t offset = (uint32_t)comparisonIndex & (BlockSize - 1);

        void* block = FNameBlockPtr(blockIdx);
        if (!block) return nullptr;

        return (uint8_t*)block + (uint64_t)offset * 2;
    }

    static std::u16string ToStringU16(int32_t comparisonIndex) {
        uint8_t* addr = (uint8_t*)GetEntryAddress(comparisonIndex);
        if (!addr) return {};

        FNameEntryHeader header = *(FNameEntryHeader*)addr;
        int len = header.Length(FNameRT.LenShift);
        if (len <= 0 || len > 1024) return {};

        const char16_t* str = (const char16_t*)(addr + 2);
        return std::u16string(str, (size_t)len);
    }

    static std::wstring ToString(int32_t comparisonIndex) {
        std::u16string s = ToStringU16(comparisonIndex);
        return U16ToW(s.c_str(), s.size());
    }

    static std::string ToUtf8(int32_t comparisonIndex) {
        return U16ToUtf8(ToStringU16(comparisonIndex));
    }

    static int32_t MaxIndex() {
        if (!FNameRT.Initialized && !InitFNameRuntime()) return 0;
        uintptr_t pool = Sarah::ImageBase + Off::GNames;
        uint32_t cursor = *(uint32_t*)(pool + FNameRT.CursorOffset);
        uint32_t currentBlock = *(uint32_t*)(pool + FNameRT.CursorOffset - 4);
        return (int32_t)((currentBlock << BlockBits) + (cursor >> 1));
    }

    static int32_t FindByName(const wchar_t* target) {
        if (!target) return 0;
        if (!FNameRT.Initialized && !InitFNameRuntime()) return 0;
        if (!FNameRT.Valid) return 0;

        std::u16string needle = WToU16(target);
        int total = MaxIndex();

        for (int i = 1; i < total; i++) {
            uint8_t* addr = (uint8_t*)GetEntryAddress(i);
            if (!addr) continue;

            FNameEntryHeader header = *(FNameEntryHeader*)addr;
            int len = header.Length(FNameRT.LenShift);
            if (len <= 0 || len > 128) continue;
            if ((size_t)len != needle.size()) continue;
            if (header.IsWide()) continue;

            const char16_t* str = (const char16_t*)(addr + 2);
            if (memcmp(str, needle.c_str(), (size_t)len * 2) == 0) return i;
        }
        return 0;
    }
};

}

inline uint32_t MakeFNameIndex(const wchar_t* name) {
    return (uint32_t)Sarah::FNameReader::FindByName(name);
}

inline FName MakeFName(const wchar_t* name) {
    return FName((int32_t)MakeFNameIndex(name));
}
