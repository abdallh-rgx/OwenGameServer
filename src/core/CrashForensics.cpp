#include "CrashForensics.hpp"

#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <dlfcn.h>
#include <link.h>
#include <mutex>
#include <atomic>
#include <sys/syscall.h>
#include <sys/ucontext.h>
#include <android/log.h>

#include "Offsets.hpp"
#include "dobby.h"

// ============================================================
// Part 1: async-signal-safe crash report writer
// ============================================================

namespace Sarah {
namespace Forensics {

static char g_LogPath[512] = {0};

void SetLogPath(const char* p) {
    if (!p) return;
    strncpy(g_LogPath, p, sizeof(g_LogPath) - 1);
    g_LogPath[sizeof(g_LogPath) - 1] = 0;
}

// ---------- module snapshot (exact dlopen biases, exec ranges) ----------

struct ExecRange {
    uint64_t start;
    uint64_t end;
    int      modIdx;
};
struct ModInfo {
    uint64_t bias;
    char     name[80];
};

static ExecRange g_Exec[1536];
static int       g_ExecCount = 0;
static ModInfo   g_Mods[384];
static int       g_ModCount = 0;
static uint64_t  g_UnrealBias = 0;
static uint64_t  g_OwenBias = 0;

static int FindOrAddMod(const char* dlname, uint64_t bias) {
    const char* base = dlname && *dlname ? strrchr(dlname, '/') : nullptr;
    base = base ? base + 1 : (dlname && *dlname ? dlname : "(main)");
    for (int i = 0; i < g_ModCount; i++) {
        if (g_Mods[i].bias == bias && strncmp(g_Mods[i].name, base, sizeof(g_Mods[0].name) - 1) == 0)
            return i;
    }
    if (g_ModCount >= (int)(sizeof(g_Mods) / sizeof(g_Mods[0]))) return -1;
    strncpy(g_Mods[g_ModCount].name, base, sizeof(g_Mods[0].name) - 1);
    g_Mods[g_ModCount].name[sizeof(g_Mods[0].name) - 1] = 0;
    g_Mods[g_ModCount].bias = bias;
    return g_ModCount++;
}

static int ModSnapCB(struct dl_phdr_info* info, size_t, void*) {
    int modIdx = FindOrAddMod(info->dlpi_name, info->dlpi_addr);
    if (modIdx < 0) return 0;
    for (int i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr)& p = info->dlpi_phdr[i];
        if (p.p_type != PT_LOAD || !(p.p_flags & PF_X)) continue;
        if (g_ExecCount >= (int)(sizeof(g_Exec) / sizeof(g_Exec[0]))) return 1;
        g_Exec[g_ExecCount].start = info->dlpi_addr + p.p_vaddr;
        g_Exec[g_ExecCount].end   = g_Exec[g_ExecCount].start + p.p_memsz;
        g_Exec[g_ExecCount].modIdx = modIdx;
        g_ExecCount++;
    }
    return 0;
}

static void SnapshotModules() {
    g_ExecCount = 0;
    g_ModCount = 0;
    g_UnrealBias = 0;
    g_OwenBias = 0;
    dl_iterate_phdr(ModSnapCB, nullptr);
    for (int i = 0; i < g_ModCount; i++) {
        if (strstr(g_Mods[i].name, "libUnreal")) g_UnrealBias = g_Mods[i].bias;
        if (strstr(g_Mods[i].name, "gameserver")) g_OwenBias = g_Mods[i].bias;
    }
}

// ---------- fresh /proc/self/maps parse (used inside the handler) ----------

struct MapsRegion {
    uint64_t start;
    uint64_t end;
    uint32_t perms;   // bit0=R bit1=W bit2=X
    uint64_t offset;
    int      nameIdx; // -1 anonymous
};

static MapsRegion g_Regions[2048];
static int        g_RegionCount = 0;
static char       g_RegionNames[768][64];
static int        g_RegionNameCount = 0;
static char       g_MapsFile[262144];

static uint64_t HexParse(const char*& p) {
    uint64_t v = 0;
    int n = 0;
    while (*p) {
        char c = *p;
        uint64_t d;
        if (c >= '0' && c <= '9') d = (uint64_t)(c - '0');
        else if (c >= 'a' && c <= 'f') d = (uint64_t)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') d = (uint64_t)(c - 'A' + 10);
        else break;
        v = (v << 4) | d;
        p++;
        if (++n > 16) break;
    }
    return v;
}

static void SkipField(const char*& p) {
    while (*p && *p != ' ' && *p != '\n') p++;
}

// Parses all of /proc/self/maps into g_Regions. Async-signal-safe
// (open/read/close + hand parser only).
static void ParseMapsFresh() {
    g_RegionCount = 0;
    g_RegionNameCount = 0;

    int fd = open("/proc/self/maps", O_RDONLY | O_CLOEXEC);
    if (fd < 0) return;

    size_t total = 0;
    while (total < sizeof(g_MapsFile) - 1) {
        ssize_t n = read(fd, g_MapsFile + total, sizeof(g_MapsFile) - 1 - total);
        if (n <= 0) break;
        total += (size_t)n;
    }
    close(fd);
    g_MapsFile[total] = 0;

    const char* p = g_MapsFile;
    while (*p && g_RegionCount < (int)(sizeof(g_Regions) / sizeof(g_Regions[0]))) {
        // line: START-END PERM OFFSET DEV INODE [PATH]
        uint64_t start = HexParse(p);
        if (*p != '-') break;
        p++;
        uint64_t end = HexParse(p);
        if (*p == ' ') p++;

        uint32_t perms = 0;
        if (*p == 'r') { perms |= 1; }
        if (p[1] == 'w') { perms |= 2; }
        if (p[2] == 'x') { perms |= 4; }
        p += 3;
        if (*p == 'p' || *p == 's') p++;
        if (*p == ' ') p++;

        uint64_t offset = HexParse(p);
        if (*p == ' ') p++;
        SkipField(p); // dev (xx:yy)
        if (*p == ' ') p++;
        SkipField(p); // inode (decimal)
        if (*p == ' ') p++;

        int nameIdx = -1;
        if (*p && *p != '\n') {
            const char* nStart = p;
            SkipField(p);
            size_t nLen = (size_t)(p - nStart);
            if (nLen > 0 && nLen < sizeof(g_RegionNames[0]) - 1) {
                int found = -1;
                for (int i = 0; i < g_RegionNameCount; i++) {
                    if (strncmp(g_RegionNames[i], nStart, nLen) == 0 && g_RegionNames[i][nLen] == 0) {
                        found = i;
                        break;
                    }
                }
                if (found < 0 && g_RegionNameCount < (int)(sizeof(g_RegionNames) / sizeof(g_RegionNames[0]))) {
                    found = g_RegionNameCount++;
                    memcpy(g_RegionNames[found], nStart, nLen);
                    g_RegionNames[found][nLen] = 0;
                }
                nameIdx = found;
            }
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;

        g_Regions[g_RegionCount].start = start;
        g_Regions[g_RegionCount].end = end;
        g_Regions[g_RegionCount].perms = perms;
        g_Regions[g_RegionCount].offset = offset;
        g_Regions[g_RegionCount].nameIdx = nameIdx;
        g_RegionCount++;
    }
}

static int FindRegionIdx(uint64_t a) {
    for (int i = 0; i < g_RegionCount; i++) {
        if (a >= g_Regions[i].start && a < g_Regions[i].end) return i;
    }
    return -1;
}

// ---------- report buffer (manual formatting only) ----------

static char  g_Report[32768];
static size_t g_ReportLen = 0;

static void RB_Reset() { g_ReportLen = 0; g_Report[0] = 0; }

static void RB(const char* s) {
    if (!s) return;
    size_t n = 0;
    while (s[n] && n < 4096) n++;
    if (g_ReportLen + n + 1 >= sizeof(g_Report)) return;
    memcpy(g_Report + g_ReportLen, s, n);
    g_ReportLen += n;
    g_Report[g_ReportLen] = 0;
}

static void RBHex(uint64_t v) {
    char buf[19];
    buf[0] = '0'; buf[1] = 'x';
    int pos = 2;
    bool started = false;
    for (int shift = 60; shift >= 0; shift -= 4) {
        uint64_t d = (v >> shift) & 0xF;
        if (d || started || shift == 0) {
            buf[pos++] = (char)(d < 10 ? '0' + d : 'a' + (d - 10));
            started = true;
        }
    }
    buf[pos] = 0;
    RB(buf);
}

static void RBDec(int64_t v) {
    char buf[24];
    int pos = 23;
    buf[pos] = 0;
    bool neg = v < 0;
    uint64_t u = neg ? (uint64_t)(-v) : (uint64_t)v;
    if (u == 0) buf[--pos] = '0';
    while (u) {
        buf[--pos] = (char)('0' + (u % 10));
        u /= 10;
    }
    if (neg) buf[--pos] = '-';
    RB(buf + pos);
}

// ---------- hook-site correlation table ----------

struct HookSite {
    const char* name;
    uint64_t rva;
};

static const HookSite kHookSites[] = {
    {"GetNetMode [Dobby HOOK]", Off::GetNetMode},
    {"TickFlush [Dobby HOOK]", Off::TickFlush},
    {"ClientOnPawnDied [Dobby HOOK]", Off::ClientOnPawnDied},
    {"BuildingActor_OnDamageServer [Dobby HOOK]", Off::BuildingActor_OnDamageServer},
    {"PickTeam [Dobby HOOK]", Off::PickTeam},
    {"StartAircraftPhase [Dobby HOOK]", Off::StartAircraftPhase},
    {"SpawnDefaultPawnFor [Dobby HOOK]", Off::SpawnDefaultPawnFor},
    {"GameSessionPatch [byte patch site]", Off::GameSessionPatch},
    {"ProcessEvent [called directly]", Off::ProcessEvent},
    {"StaticFindObject [called directly]", Off::StaticFindObject},
    {"ExecuteConsoleCommand [called directly]", Off::ExecuteConsoleCommand},
};

// Returns the hook site name if addr lies inside the function body
// (rva..rva+0x500) or just below its entry (rva-0x80..rva).
// deltaOut receives addr - site (may be negative).
static const char* MatchHookSite(uint64_t addr, int64_t* deltaOut) {
    if (!g_UnrealBias) return nullptr;
    for (size_t i = 0; i < sizeof(kHookSites) / sizeof(kHookSites[0]); i++) {
        const uint64_t site = g_UnrealBias + kHookSites[i].rva;
        if (addr >= site && addr < site + 0x500) {
            if (deltaOut) *deltaOut = (int64_t)(addr - site);
            return kHookSites[i].name;
        }
        if (addr + 0x80 >= site && addr < site) {
            if (deltaOut) *deltaOut = (int64_t)(addr - site);
            return kHookSites[i].name;
        }
    }
    return nullptr;
}

// ---------- address classification ----------

struct AddrInfo {
    bool    known;      // classified at all
    char    desc[112];  // "libUnreal.so + 0x1234 (r-x)" etc.
    bool    inOurLib;
};

static void ClassifyAddr(uint64_t addr, AddrInfo& out) {
    out.known = false;
    out.inOurLib = false;
    out.desc[0] = 0;

    // 1) exact: snapshot exec ranges (dlopen bias => exact RVA)
    for (int i = 0; i < g_ExecCount; i++) {
        if (addr >= g_Exec[i].start && addr < g_Exec[i].end) {
            int m = g_Exec[i].modIdx;
            const char* name = g_Mods[m].name;
            uint64_t rva = addr - g_Mods[m].bias;
            snprintf(out.desc, sizeof(out.desc), "%s + 0x%llx [exec]",
                     name, (unsigned long long)rva);
            out.known = true;
            out.inOurLib = strstr(name, "gameserver") != nullptr;
            return;
        }
    }

    // 2) fresh maps (any region type, incl. anon exec / trampolines)
    int r = FindRegionIdx(addr);
    if (r >= 0) {
        const MapsRegion& reg = g_Regions[r];
        char perms[5];
        perms[0] = (reg.perms & 1) ? 'r' : '-';
        perms[1] = (reg.perms & 2) ? 'w' : '-';
        perms[2] = (reg.perms & 4) ? 'x' : '-';
        perms[3] = 'p';
        perms[4] = 0;
        if (reg.nameIdx >= 0) {
            uint64_t approxRva = addr - (reg.start - reg.offset);
            snprintf(out.desc, sizeof(out.desc), "%s + ~0x%llx [%s] (rva via file offset)",
                     g_RegionNames[reg.nameIdx], (unsigned long long)approxRva, perms);
        } else if (reg.perms & 4) {
            snprintf(out.desc, sizeof(out.desc),
                     "ANON EXEC 0x%llx-0x%llx [%s] (Dobby trampoline / JIT?)",
                     (unsigned long long)reg.start, (unsigned long long)reg.end, perms);
        } else {
            snprintf(out.desc, sizeof(out.desc), "anon 0x%llx-0x%llx [%s]",
                     (unsigned long long)reg.start, (unsigned long long)reg.end, perms);
        }
        out.known = true;
        return;
    }

    snprintf(out.desc, sizeof(out.desc), "UNMAPPED");
    out.known = true;
}

// ---------- signal naming ----------

static const char* SigName(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGBUS:  return "SIGBUS";
        case SIGILL:  return "SIGILL";
        case SIGFPE:  return "SIGFPE";
        default:      return "SIG?";
    }
}

static const char* SiCodeName(int sig, int code) {
    if (sig == SIGSEGV || sig == SIGBUS || sig == SIGILL || sig == SIGFPE) {
        switch (code) {
            case 1:  return "SEGV_MAPERR";
            case 2:  return "SEGV_ACCERR";
            case 3:  return "SEGV_BNDERR";
            case 128: return "SI_KERNEL";
            default: break;
        }
    }
    switch (code) {
        case 0:  return "SI_USER";
        case -6: return "SI_TKILL";
        case -1: return "SI_QUEUE";
        case -2: return "SI_TIMER";
        default: return "SI_?";
    }
}

static const char* SiCodeMeaning(int sig, int code) {
    if (code == -6) {
        return "signal was RAISED BY THE PROCESS ITSELF (tgkill) => the Unreal "
               "engine deliberately aborted (check()/ensure()/appErrorf fatal "
               "error path). The real error text was printed to logcat just "
               "before - see the [UELOG] lines captured above in this file.";
    }
    if ((sig == SIGSEGV || sig == SIGBUS) && code == 1) {
        return "genuine access to UNMAPPED memory (bad pointer dereference).";
    }
    if ((sig == SIGSEGV || sig == SIGBUS) && code == 2) {
        return "access to mapped memory with WRONG PERMISSIONS (e.g. writing "
               "to read-only code / executing data).";
    }
    if (code == 128) {
        return "raised by the kernel (SI_KERNEL), often a CPU fault the "
               "kernel could not attribute.";
    }
    return "see signal(7); classify with the register dump below.";
}

// ---------- the report ----------

static volatile sig_atomic_t g_InHandler = 0;
static volatile sig_atomic_t g_ReportCount = 0;

// Returns true when the fault looks like an ART/JIT implicit null check
// (PC in anonymous executable memory, low fault address). Those faults are
// handled - and RESUMED from - by ART's own handler in the chain, so we must
// not spend milliseconds writing reports on them.
static bool LooksLikeARTFault(int sig, siginfo_t* info, ucontext_t* uc) {
    if (sig != SIGSEGV) return false;
    if ((int)info->si_code != 1) return false; // SEGV_MAPERR only
    if ((uint64_t)info->si_addr >= 0x10000) return false;
    const uint64_t pc = (uint64_t)uc->uc_mcontext.pc;
    // PC inside a dlopen'd module's exec range -> native code, not JIT.
    for (int i = 0; i < g_ExecCount; i++) {
        if (pc >= g_Exec[i].start && pc < g_Exec[i].end) return false;
    }
    // PC inside a file-backed exec mapping -> real native library code.
    int r = FindRegionIdx(pc);
    if (r >= 0 && g_Regions[r].nameIdx >= 0) return false;
    return true; // anon exec or unclassified -> assume JIT/ART
}

static void RB_AddrLine(const char* label, uint64_t addr) {
    RB(label);
    RB(": ");
    RBHex(addr);
    RB("  ");
    AddrInfo ai;
    ClassifyAddr(addr, ai);
    RB(ai.desc);
    const char* site = MatchHookSite(addr, nullptr);
    if (site) {
        RB("   <<<<< CRITICAL: this is ");
        RB(site);
        int64_t d = 0;
        MatchHookSite(addr, &d);
        RB(" (delta ");
        RBDec(d);
        RB(")");
    }
    RB("\n");
}

static void WriteReportToFd(int fd) {
    size_t off = 0;
    while (off < g_ReportLen) {
        ssize_t n = write(fd, g_Report + off, g_ReportLen - off);
        if (n <= 0) break;
        off += (size_t)n;
    }
}

static void WriteCrashReport(int sig, siginfo_t* info, void* uctxVoid) {
    ucontext_t* uc = (ucontext_t*)uctxVoid;
    const uint64_t pc = (uint64_t)uc->uc_mcontext.pc;
    const uint64_t sp = (uint64_t)uc->uc_mcontext.sp;
    const uint64_t lr = (uint64_t)uc->uc_mcontext.regs[30];
    uint64_t       fp = (uint64_t)uc->uc_mcontext.regs[29];

    ParseMapsFresh();

    RB_Reset();
    RB("\n============ OWEN CRASH FORENSICS ============\n");
    RB("signal ");
    RBDec(sig);
    RB(" (");
    RB(SigName(sig));
    RB(")  si_code=");
    RBDec((int)info->si_code);
    RB(" (");
    RB(SiCodeName(sig, (int)info->si_code));
    RB(")\n");
    RB("meaning: ");
    RB(SiCodeMeaning(sig, (int)info->si_code));
    RB("\n");
    RB("fault address: 0x");
    RBHex((uint64_t)(info->si_addr));
    {
        AddrInfo ai;
        ClassifyAddr((uint64_t)(info->si_addr), ai);
        RB("  ");
        RB(ai.desc);
    }
    RB("\n");

    // thread identity
    {
        char tbuf[96];
        long tid = (long)syscall(SYS_gettid);
        snprintf(tbuf, sizeof(tbuf), "/proc/self/task/%ld/comm", tid);
        int tfd = open(tbuf, O_RDONLY | O_CLOEXEC);
        char comm[40] = "?";
        if (tfd >= 0) {
            ssize_t n = read(tfd, comm, sizeof(comm) - 1);
            close(tfd);
            if (n <= 0) n = 0;
            comm[n] = 0;
            for (ssize_t i = 0; i < n; i++) if (comm[i] == '\n') comm[i] = 0;
        }
        RB("crashing thread: tid=");
        RBDec(tid);
        RB(" name='");
        RB(comm);
        RB("'\n");
    }

    RB("--- key registers ---\n");
    RB_AddrLine("pc ", pc);
    RB_AddrLine("lr ", lr);
    RB_AddrLine("sp ", sp);
    RB_AddrLine("fp ", fp);

    RB("--- GPRs ---\n");
    for (int r = 0; r <= 28; r += 4) {
        RB("x");
        RBDec(r);
        RB("-");
        RBDec(r + 3 < 28 ? r + 3 : 28);
        RB(":");
        for (int j = r; j <= r + 3 && j <= 28; j++) {
            RB(" ");
            RBHex((uint64_t)uc->uc_mcontext.regs[j]);
        }
        RB("\n");
    }

    // hook-site verdict for the PC
    {
        int64_t d = 0;
        const char* site = MatchHookSite(pc, &d);
        RB("--- hook-site verdict ---\n");
        if (site) {
            RB("!!! CRASH PC IS ");
            RB(site);
            RB(" (delta ");
            RBDec(d);
            RB(" from function entry). A Dobby hook or its relocated prologue "
               "at a wrong/unaligned offset is the prime suspect !!!\n");
        } else {
            RB("crash PC is NOT inside/near any known hooked function.\n");
        }
        if (g_OwenBias) {
            bool pcOurs = pc >= g_OwenBias && pc < g_OwenBias + 0x400000;
            RB("crash PC inside libgameserver.so: ");
            RB(pcOurs ? "YES (our code faulted directly)" : "no");
            RB("\n");
        }
    }

    // frame-pointer walk
    RB("--- frame-pointer backtrace (x29 chain) ---\n");
    {
        // stack bounds from maps: region containing sp
        uint64_t stackTop = sp + 0x100000; // fallback bound
        int sr = FindRegionIdx(sp);
        if (sr >= 0 && g_Regions[sr].end > sp) stackTop = g_Regions[sr].end;

        int frames = 0;
        while (frames < 48) {
            if (fp == 0) break;
            if (fp < sp || fp + 16 > stackTop) {
                RB("  (fp out of stack bounds; walk stops)\n");
                break;
            }
            uint64_t nextFp = *(volatile uint64_t*)fp;
            uint64_t retAddr = *(volatile uint64_t*)(fp + 8);
            if (retAddr) {
                RB("  #");
                RBDec(frames);
                RB(" ");
                RB_AddrLine("", retAddr);
                frames++;
            }
            if (nextFp <= fp || nextFp + 16 > stackTop) break;
            fp = nextFp;
        }
        if (frames == 0) {
            RB("  (no frame pointers available - libUnreal likely compiled "
               "with -fomit-frame-pointer; see stack scan below)\n");
        }
    }

    // raw stack scan for plausible return addresses
    RB("--- stack scan (candidate return addresses, sp..sp+48KB) ---\n");
    {
        uint64_t stackTop = sp + 0x100000;
        int sr = FindRegionIdx(sp);
        if (sr >= 0 && g_Regions[sr].end > sp) stackTop = g_Regions[sr].end;
        uint64_t scanEnd = sp + 0xC000;
        if (scanEnd > stackTop) scanEnd = stackTop;

        int found = 0;
        uint64_t last = 0;
        for (uint64_t a = (sp + 7) & ~(uint64_t)7; a + 8 <= scanEnd && found < 64; a += 8) {
            uint64_t v = *(volatile uint64_t*)a;
            if (v == last) continue;
            bool execKnown = false;
            for (int i = 0; i < g_ExecCount; i++) {
                if (v >= g_Exec[i].start && v < g_Exec[i].end) { execKnown = true; break; }
            }
            if (!execKnown && g_RegionCount > 0) {
                int r = FindRegionIdx(v);
                if (r >= 0 && (g_Regions[r].perms & 4)) execKnown = true;
            }
            if (!execKnown) continue;
            last = v;
            RB("  @sp+");
            RBHex(a - sp);
            RB(" ");
            RB_AddrLine("", v);
            found++;
        }
        if (found == 0) RB("  (none found)\n");
    }

    RB("--- module snapshot (dlopen biases) ---\n");
    {
        RB("libUnreal.so bias: ");
        RBHex(g_UnrealBias);
        RB("   libgameserver.so bias: ");
        RBHex(g_OwenBias);
        RB("\n");
    }

    RB("==============================================\n");

    if (g_LogPath[0]) {
        int fd = open(g_LogPath, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC, 0644);
        if (fd >= 0) {
            WriteReportToFd(fd);
            close(fd);
        }
    }
}

// ============================================================
// Part 2: chained signal handlers
// ============================================================

static struct sigaction g_Prev[5];
static bool             g_HavePrev[5];

static int SigSlot(int sig) {
    switch (sig) {
        case SIGSEGV: return 0;
        case SIGABRT: return 1;
        case SIGBUS:  return 2;
        case SIGILL:  return 3;
        case SIGFPE:  return 4;
    }
    return -1;
}

extern "C" void OwenSigHandler(int sig, siginfo_t* info, void* uctx) {
    const int slot = SigSlot(sig);

    if (g_InHandler) {
        // Nested fault WHILE we are writing a report: our own report code
        // faulted. Bail out hard so a tombstone is still generated.
        struct sigaction d;
        memset(&d, 0, sizeof(d));
        d.sa_handler = SIG_DFL;
        sigaction(sig, &d, nullptr);
        syscall(SYS_tgkill, getpid(), syscall(SYS_gettid), sig);
        for (;;) pause();
    }
    g_InHandler = 1;

    // ART/JIT implicit null checks must pass through silently (cheap check
    // that needs the maps; parse lazily only when it matters).
    bool artFault = false;
    if ((int)info->si_code == 1 && sig == SIGSEGV) {
        // quick pre-check without maps: if PC is inside a known module it is
        // native code -> never an ART fault.
        const uint64_t pcQuick = (uint64_t)((ucontext_t*)uctx)->uc_mcontext.pc;
        bool inModule = false;
        for (int i = 0; i < g_ExecCount; i++) {
            if (pcQuick >= g_Exec[i].start && pcQuick < g_Exec[i].end) { inModule = true; break; }
        }
        if (!inModule) {
            ParseMapsFresh();
            artFault = LooksLikeARTFault(sig, info, (ucontext_t*)uctx);
        }
    }

    if (!artFault && g_ReportCount < 3) {
        g_ReportCount++;
        WriteCrashReport(sig, info, uctx);
    }
    g_InHandler = 0;

    // Chain to the previous handler (engine's / ART's), so its own
    // fatal-error reporting, null-check fixups and tombstone flow still run.
    // If the chained handler FIXES the context (ART null check) it returns
    // normally and execution resumes at the fixed PC - so we must NOT
    // re-raise after a successful chain.
    if (slot >= 0 && g_HavePrev[slot]) {
        struct sigaction* prev = &g_Prev[slot];
        if ((prev->sa_flags & SA_SIGINFO) &&
            prev->sa_sigaction != nullptr &&
            prev->sa_sigaction != (void (*)(int, siginfo_t*, void*))SIG_DFL &&
            prev->sa_sigaction != (void (*)(int, siginfo_t*, void*))SIG_IGN) {
            prev->sa_sigaction(sig, info, uctx);
            return; // handler may have fixed up the context - resume.
        } else if (!(prev->sa_flags & SA_SIGINFO) &&
                   prev->sa_handler != nullptr &&
                   prev->sa_handler != SIG_DFL &&
                   prev->sa_handler != SIG_IGN) {
            prev->sa_handler(sig);
            return; // handler may have fixed up the context - resume.
        }
    }

    // No previous handler: we are the last resort. Restore the default
    // disposition and re-raise so the process terminates with a tombstone.
    struct sigaction d;
    memset(&d, 0, sizeof(d));
    d.sa_handler = SIG_DFL;
    sigaction(sig, &d, nullptr);
    syscall(SYS_tgkill, getpid(), syscall(SYS_gettid), sig);
    for (;;) pause();
}

static void InstallOneSignal(int sig) {
    const int slot = SigSlot(sig);
    if (slot < 0) return;

    struct sigaction cur;
    memset(&cur, 0, sizeof(cur));
    if (sigaction(sig, nullptr, &cur) == 0) {
        void* curHandler = (cur.sa_flags & SA_SIGINFO)
            ? (void*)cur.sa_sigaction
            : (void*)cur.sa_handler;
        const void* ourHandler = (const void*)&OwenSigHandler;
        const void* dfl = (const void*)SIG_DFL;
        const void* ign = (const void*)SIG_IGN;
        if (curHandler == ourHandler) {
            // Our handler already installed: keep the saved chain as-is,
            // just refresh the module snapshot below.
        } else if (curHandler != dfl && curHandler != ign) {
            g_Prev[slot] = cur;
            g_HavePrev[slot] = true;
        } else {
            g_HavePrev[slot] = false;
        }
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = &OwenSigHandler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    sigaction(sig, &sa, nullptr);
}

void InstallSignalHandlers() {
    SnapshotModules();
    InstallOneSignal(SIGSEGV);
    InstallOneSignal(SIGABRT);
    InstallOneSignal(SIGBUS);
    InstallOneSignal(SIGILL);
    InstallOneSignal(SIGFPE);
    __android_log_print(ANDROID_LOG_INFO, "OwenGameServer",
        "[FORENSICS] signal handlers installed (modules=%d execRanges=%d unrealBias=0x%llx)",
        g_ModCount, g_ExecCount, (unsigned long long)g_UnrealBias);
}

// ============================================================
// Part 3: fatal-API hooks (normal context; may use stdio)
// ============================================================

static std::mutex& FatalLogMutex() {
    static std::mutex m;
    return m;
}

static void FatalAPILog(const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    __android_log_write(ANDROID_LOG_ERROR, "OwenGameServer", buf);

    if (!g_LogPath[0]) return;
    std::lock_guard<std::mutex> lock(FatalLogMutex());
    FILE* f = fopen(g_LogPath, "a");
    if (!f) return;
    fprintf(f, "%s\n", buf);
    fflush(f);
    fclose(f);
}

// ---- raise() ----

static int (*g_raiseOG)(int) = nullptr;

static int RaiseHook(int sig) {
    if (sig == SIGSEGV || sig == SIGABRT || sig == SIGBUS || sig == SIGILL || sig == SIGFPE) {
        FatalAPILog("[FATAL-API] raise(%d) called: an engine/component fatal error is being "
                    "delivered. Full context follows in the CRASH FORENSICS report below.", sig);
    }
    if (g_raiseOG) return g_raiseOG(sig);
    // Fallback: raw syscall, never re-enter raise().
    return (int)syscall(SYS_tgkill, getpid(), syscall(SYS_gettid), sig);
}

// ---- android_set_abort_message ----

static void (*g_abortMsgOG)(const char*) = nullptr;

static void AbortMsgHook(const char* msg) {
    FatalAPILog("[FATAL-API] android_set_abort_message: %s", msg ? msg : "(null)");
    if (g_abortMsgOG) g_abortMsgOG(msg);
}

// ---- liblog capture (the actual engine error text) ----

static int  (*g_real_log_vprint)(int, const char*, const char*, va_list) = nullptr;
static int  (*g_real_log_write)(int, const char*, const char*) = nullptr;
static int  (*g_logPrintOG)(int, const char*, const char*, ...) = nullptr;
static int  (*g_logWriteOG)(int, const char*, const char*) = nullptr;

static bool ShouldCaptureUELog(int prio, const char* tag) {
    if (prio >= ANDROID_LOG_FATAL) return true; // 7
    if (prio < ANDROID_LOG_ERROR) return false; // < 6
    if (!tag) return false;
    if (strcmp(tag, "OwenGameServer") == 0) return false; // never our own lines
    // UE tags are exactly "UE"/"UE4"/"UE5"/"UE5-Fortnite..." - prefix match,
    // not substring, to avoid e.g. "VALUES" false positives.
    if (strncmp(tag, "UE", 2) == 0) return true;
    return strstr(tag, "Fortnite") || strstr(tag, "CRASH") ||
           strstr(tag, "AndroidRuntime") || strstr(tag, "libc");
}

static void CaptureUELog(int prio, const char* tag, const char* text) {
    static std::atomic<int> captured{0};
    static std::atomic<int> overflowLogged{0};
    int n = captured.fetch_add(1);
    if (n > 512) {
        if (overflowLogged.exchange(1) == 0) {
            FatalAPILog("[UELOG] capture limit reached; further lines suppressed.");
        }
        return;
    }
    FatalAPILog("[UELOG][prio=%d][%s] %s", prio, tag ? tag : "-", text ? text : "(null)");
}

static int LogPrintHook(int prio, const char* tag, const char* fmt, ...) {
    if (ShouldCaptureUELog(prio, tag)) {
        char buf[2048];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        CaptureUELog(prio, tag, buf);
    }
    va_list ap2;
    va_start(ap2, fmt);
    int r = g_real_log_vprint ? g_real_log_vprint(prio, tag, fmt, ap2) : -1;
    va_end(ap2);
    return r;
}

static int LogWriteHook(int prio, const char* tag, const char* text) {
    if (ShouldCaptureUELog(prio, tag)) {
        CaptureUELog(prio, tag, text);
    }
    return g_real_log_write ? g_real_log_write(prio, tag, text) : -1;
}

static void* Sym(const char* name) {
    void* h = dlsym(RTLD_DEFAULT, name);
    if (!h) {
        // try explicit libs
        static void* libcH = dlopen("libc.so", RTLD_NOW);
        static void* logH = dlopen("liblog.so", RTLD_NOW);
        if (libcH) h = dlsym(libcH, name);
        if (!h && logH) h = dlsym(logH, name);
    }
    return h;
}

void InstallFatalAPIHooks() {
    // liblog: capture the engine's FATAL/ERROR text. Requires the real
    // __android_log_vprint for transparent forwarding of the print hook.
    g_real_log_vprint = (int (*)(int, const char*, const char*, va_list))Sym("__android_log_vprint");
    g_real_log_write = (int (*)(int, const char*, const char*))Sym("__android_log_write");

    if (g_real_log_vprint) {
        void* printFn = Sym("__android_log_print");
        if (printFn) {
            if (DobbyHook(printFn, (dobby_dummy_func_t)&LogPrintHook,
                          (dobby_dummy_func_t*)&g_logPrintOG) == 0) {
                __android_log_print(ANDROID_LOG_INFO, "OwenGameServer",
                    "[FORENSICS] __android_log_print hooked (engine fatal text capture ON)");
            }
        }
    }
    if (g_real_log_write) {
        void* writeFn = Sym("__android_log_write");
        if (writeFn && writeFn != (void*)&LogWriteHook) {
            if (DobbyHook(writeFn, (dobby_dummy_func_t)&LogWriteHook,
                          (dobby_dummy_func_t*)&g_logWriteOG) == 0) {
                __android_log_print(ANDROID_LOG_INFO, "OwenGameServer",
                    "[FORENSICS] __android_log_write hooked (engine fatal text capture ON)");
            }
        }
    }

    // libc: raise + android_set_abort_message
    void* raiseFn = Sym("raise");
    if (raiseFn) {
        if (DobbyHook(raiseFn, (dobby_dummy_func_t)&RaiseHook,
                      (dobby_dummy_func_t*)&g_raiseOG) == 0) {
            __android_log_print(ANDROID_LOG_INFO, "OwenGameServer",
                "[FORENSICS] raise() hooked (engine-raised fatal tagging ON)");
        }
    }
    void* abortMsgFn = Sym("android_set_abort_message");
    if (abortMsgFn) {
        if (DobbyHook(abortMsgFn, (dobby_dummy_func_t)&AbortMsgHook,
                      (dobby_dummy_func_t*)&g_abortMsgOG) == 0) {
            __android_log_print(ANDROID_LOG_INFO, "OwenGameServer",
                "[FORENSICS] android_set_abort_message hooked (abort text capture ON)");
        }
    }
}

} // namespace Forensics
} // namespace Sarah
