#pragma once
#include <cstdint>
#include <link.h>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>
#include <string>
#include "Offsets.hpp"

namespace Sarah {
    inline uint64_t ImageBase = 0;
    inline uint64_t TextStart = 0;
    inline uint64_t TextEnd   = 0;
}

inline int _dl_cb(struct dl_phdr_info* info, size_t, void*) {
    if (!info->dlpi_name || !*info->dlpi_name) return 0;
    if (!strstr(info->dlpi_name, "libUnreal.so")) return 0;

    Sarah::ImageBase = info->dlpi_addr;

    for (int i = 0; i < info->dlpi_phnum; i++) {
        const auto& p = info->dlpi_phdr[i];
        if (p.p_type != PT_LOAD) continue;
        if (!(p.p_flags & PF_X)) continue;
        Sarah::TextStart = info->dlpi_addr + p.p_vaddr;
        Sarah::TextEnd   = Sarah::TextStart + p.p_memsz;
        break;
    }
    return 1;
}

inline bool InitImageBase() {
    dl_iterate_phdr(_dl_cb, nullptr);
    return Sarah::ImageBase != 0;
}

template <typename T = void>
inline T* OffPtr(uint64_t rva) {
    return (T*)(Sarah::ImageBase + rva);
}

inline bool GetGIsEditor() {
    return *(volatile uint8_t*)(Sarah::ImageBase + Off::GIsEditor) != 0;
}

inline bool GetGIsClient() {
    return *(volatile uint8_t*)(Sarah::ImageBase + Off::GIsClient) != 0;
}

inline bool GetGIsServer() {
    return *(volatile uint8_t*)(Sarah::ImageBase + Off::GIsServer) != 0;
}

inline void SetClientOffOnly() {
    uintptr_t page = (Sarah::ImageBase + Off::GIsEditor) & ~0xFFF;
    mprotect((void*)page, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC);
    *(volatile uint8_t*)(Sarah::ImageBase + Off::GIsEditor) = 0;
    *(volatile uint8_t*)(Sarah::ImageBase + Off::GIsClient) = 0;
}

inline void SetServerOnOnly() {
    uintptr_t page = (Sarah::ImageBase + Off::GIsServer) & ~0xFFF;
    mprotect((void*)page, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC);
    *(volatile uint8_t*)(Sarah::ImageBase + Off::GIsServer) = 1;
}

inline void SetDedicatedServerMode() {
    uintptr_t page = (Sarah::ImageBase + Off::GIsEditor) & ~0xFFF;
    mprotect((void*)page, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC);
    *(volatile uint8_t*)(Sarah::ImageBase + Off::GIsEditor) = 0;
    *(volatile uint8_t*)(Sarah::ImageBase + Off::GIsClient) = 0;
    *(volatile uint8_t*)(Sarah::ImageBase + Off::GIsServer) = 1;
}

inline std::u16string WToU16(const wchar_t* s) {
    std::u16string out;
    if (!s) return out;
    for (const wchar_t* p = s; *p; ++p) out.push_back((char16_t)*p);
    return out;
}

inline std::u16string WToU16(const std::wstring& s) {
    std::u16string out;
    out.reserve(s.size());
    for (wchar_t c : s) out.push_back((char16_t)c);
    return out;
}

inline std::wstring U16ToW(const char16_t* s, size_t n) {
    std::wstring out;
    if (!s) return out;
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) out.push_back((wchar_t)s[i]);
    return out;
}

inline std::string U16ToUtf8(const std::u16string& s) {
    return UtfN::Utf16StringToUtf8String<std::string>(s);
}
