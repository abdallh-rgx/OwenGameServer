#pragma once
#include <cstdint>

// ============================================================
// OwenGameServer crash forensics
// ------------------------------------------------------------
// The recurring crash (SIGSEGV raised with SI_TKILL on the
// GameThread, all frames inside libUnreal.so) is an
// ENGINE-DETECTED fatal error: the engine logged the real
// reason ("Fatal error: ..." / "Assertion failed: ...") to
// logcat and then raised the signal deliberately via
// tgkill(). That message is invisible in OwenGameServer.txt.
//
// This module makes the library self-diagnosing:
//
//  1. InstallSignalHandlers() - chained SA_SIGINFO handlers for
//     SIGSEGV/SIGABRT/SIGBUS/SIGILL. On crash they append a
//     full report to the log file (write(2) only, async-signal
//     safe): si_code meaning, fault address, PC/LR/SP/FP, all
//     GPRs, a frame-pointer backtrace plus a raw stack scan -
//     every address classified as module+RVA, and CORRELATED
//     with every Dobby hook site (a crash PC inside/near a
//     hooked function = smoking gun for a bad offset).
//
//  2. InstallFatalAPIHooks() - Dobby hooks on libc/liblog
//     fatal APIs:
//       * raise()                 -> tags engine-raised fatals
//       * android_set_abort_message() -> captures the abort text
//       * __android_log_print/write() -> captures the engine's
//         FATAL/ERROR logcat lines (the actual "Assertion
//         failed: ..." message) into the log file.
//
//  3. Everything is forwarded to the original implementation,
//     so engine behaviour (including tombstone generation) is
//     unchanged.
//
// Call SetLogPath() once InitLogFile picked a writable path,
// then InstallSignalHandlers() as early as possible and again
// after the engine is fully initialized (the second install
// re-snapshots modules and chains onto the engine's own crash
// handlers). InstallFatalAPIHooks() is called once.
// ============================================================

namespace Sarah {
namespace Forensics {

// Remember which file the main log uses. nullptr clears it.
void SetLogPath(const char* path);

// Install/refresh chained crash signal handlers + module snapshot.
// Safe to call repeatedly (re-install keeps the original chain).
void InstallSignalHandlers();

// Dobby-hook raise / android_set_abort_message / liblog print+write.
// Requires dlsym + Dobby. Call once, as early as possible.
void InstallFatalAPIHooks();

} // namespace Forensics
} // namespace Sarah
