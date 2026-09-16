#pragma once
#include <cstdint>
#include <vector>
#include <sys/mman.h>
#include "Offsets.hpp"
#include "AndroidBase.hpp"

inline std::vector<void(*)()> _HookFuncs;

template <typename T>
inline void PatchBytes(uint64_t rva, T value) {
    uintptr_t ptr = Sarah::ImageBase + rva;
    uintptr_t page = ptr & ~0xFFF;
    size_t len = ((ptr + sizeof(T) - 1) & ~0xFFF) - page + 0x1000;
    mprotect((void*)page, len, PROT_READ | PROT_WRITE | PROT_EXEC);
    *(T*)ptr = value;
    __builtin___clear_cache((char*)ptr, (char*)(ptr + sizeof(T)));
}
