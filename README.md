# OwenGameServer — Fortnite 21.30 Android (ARM64) GameServer

Turns the Fortnite mobile client into a listen game-server by injecting
`libgameserver.so` into the game process.

## Diagnostic build (crash forensics + runtime bisect)

The GameThread crash (`SIGSEGV` with `si_code=SI_TKILL`, all frames inside
`libUnreal.so`) is an **engine-detected fatal error**: Unreal prints the real
reason (`Fatal error:` / `Assertion failed:`) to logcat and then raises the
signal deliberately. This build captures everything into
`OwenGameServer.txt` so no adb/tombstone is required:

1. **Crash report** — on any SIGSEGV/SIGABRT/SIGBUS/SIGILL the library appends
   a full report: `si_code` meaning, fault address, PC/LR/SP/FP, all GPRs, a
   frame-pointer backtrace **plus** a raw stack scan, every address classified
   as `module + RVA`, and a verdict when the crash PC sits inside/near one of
   our Dobby hook sites (smoking gun for a bad offset).
2. **Engine error text** — `__android_log_print` / `__android_log_write` /
   `android_set_abort_message` / `raise` are hooked: every UE FATAL/ERROR
   logcat line is mirrored into the log as `[UELOG] ...` — this is the actual
   "Assertion failed: ..." message that explains the crash.
3. **Heartbeat** — the 15s/60s waits log `[HB] ... world=... GIsClient=...`
   every second, so the log shows exactly *when* the crash happened relative
   to our timeline.

### Runtime config (bisect without rebuilding)

Create `OwenGameServer.cfg` next to the log file:

```
/storage/emulated/0/Android/data/com.epicgames.fortnite/files/OwenGameServer.cfg
```

One `key=value` per line (`1/true/yes/on` = enabled), `#` comments:

| key | effect |
|-----|--------|
| `safe_mode=1` | do NOTHING (no flag flips, no hooks, no travel) — control experiment |
| `no_setclientoffonly=1` | skip flipping GIsEditor/GIsClient |
| `late_flip=1` | flip GIsClient only right before map travel |
| `no_getnetmode_hook=1` | skip the GetNetMode Dobby hook |
| `honest_netmode=1` | GetNetMode hook returns the engine's true value (frontend stays client) |
| `no_tickflush_hook=1` | skip the TickFlush Dobby hook |
| `no_native_hooks=1` | skip ALL Dobby native hooks |
| `no_exec_hooks=1` | skip all UFunction ExecFunction hooks |
| `no_map_travel=1` | skip the `open <map>` command |
| `no_fatal_api_hooks=1` | skip raise/liblog hooks (signal handlers stay on) |

Defaults = previous behaviour exactly (crash still reproduces, but now fully
instrumented).

### Suggested bisect order

1. Run as-is (no cfg) → crash → **read the `[UELOG]` line and the crash
   report** — they name the failing engine check and the faulting RVA.
2. `safe_mode=1` → still crashes? the cause is outside this library.
3. `honest_netmode=1` → crash gone? the GetNetMode lie in the frontend was it.
4. `no_setclientoffonly=1` → crash gone? the GIsClient flip was it.
5. `no_getnetmode_hook=1` / `no_tickflush_hook=1` → crash gone? a bad hook
   offset was it (the crash report already tells you via the hook-site
   verdict).

## Build

GitHub Actions builds the artifact (ARM64, Android 24, NDK r25c, c++_static).
Local build:

```sh
cmake -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
  -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release \
  -B build -S .
cmake --build build -j 2
```

Requirements: NDK r25c (25.2.9519653), ninja, llvm-objcopy.
`external/MobileDumperCore` is prebuilt (see its own README).
