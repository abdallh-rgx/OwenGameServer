# OwenGameServer — Fortnite 21.30 Android (ARM64) GameServer

> **This tree is the RESTORED working state of commit `4d7e591`** (Sep 18,
> "Update curl_stubs.cpp") — the last known-good build. Everything after it
> (the Sep-19 client-app refactor: MD7 FName bridge, ExecFunction hooks,
> SetClientOffOnly, GameThread deferral, CrashForensics, RuntimeConfig) is
> preserved on branch **`backup/crash-forensics-debug`** and can be recovered
> at any time.

## Architecture of this working build (target: `com.epicgames.fortnite2130GameServer`)

1. `SetDedicatedServerMode()` — `GIsEditor=0 GIsClient=0 GIsServer=1`.
2. Global **`ProcessEvent` Dobby hook** + cached `UFunction` pointers
   (`Hooks::CacheFunctions()`), dispatched in `ProcessEventHook`.
3. `Misc::Listen()` — starts the net driver listening **after** hooks.
4. Map travel (`open Artemis_Terrain` / `open Creative_NoApollo_Terrain`)
   issued directly via `KismetSystemLibrary.ExecuteConsoleCommand`.
5. FName reads go through the engine's own
   `UKismetStringLibrary::Conv_NameToString`.

## Why the Sep-19 refactor crashed (root cause, kept for the record)

The refactor switched the target app to the regular client
(`com.epicgames.fortnite`) **and** changed the whole runtime architecture in
one go: `SetClientOffOnly` left the engine in `GIsClient=0 GIsServer=0`
(standalone limbo), `Misc::Listen()` was never called, `GetNetMode` returned
`NM_DedicatedServer` while the world was still a client frontend world, and
`ProcessEvent` was replaced by `UFunction::ExecFunction` pointer swaps. The
GameThread crash inside `libUnreal.so` was a symptom of that inconsistent
engine state — not of the FName/Utils code that was later replaced while
chasing it.

## Build

```sh
cmake -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
  -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release \
  -B build -S .
cmake --build build -j 2
```

Requirements: NDK (r25c verified locally), ninja, llvm-objcopy.
`libgameserver.so` is output in `build/`.
