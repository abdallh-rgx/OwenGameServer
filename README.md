# OwenGameServer — Fortnite 21.30 Android (ARM64) GameServer

Turns the Fortnite mobile client into a listen game-server by injecting
`libgameserver.so` into the game process.

## Current state: `4d7e591` base + Mini Dumper core

This tree is the **working `4d7e591` flow** (SetDedicatedServerMode → global
ProcessEvent hook → Misc::Listen → map travel) with the crash-causing core
**replaced**:

- `src/core/UObject.*`, `src/core/Utils.*`, `src/core/FName.hpp` — **deleted**
  (the manual GObjects/FName code, cause of the GameThread crash).
- `src/core/Dumper.hpp` / `Dumper.cpp` — the new **mini dumper**, modeled on
  [plooshi/Crystal-19.10](https://github.com/plooshi/Crystal-19.10) and
  [Ducki67/20.40](https://github.com/Ducki67/20.40):

| operation | old (crashed) | new (mini dumper) |
|---|---|---|
| FindObject / LoadObject | engine `StaticFindObject` | same (was already right) |
| `StaticClass()` / class-by-name | **walk all 203,870 GObjects**, calling `GetName()` on each (a `Conv_NameToString` ProcessEvent per object) | engine `StaticFindObject` with the `/Script/CoreUObject.Class` meta filter (+ module-prefix probing) — hash-based, thread-safe |
| FName → string | SDK wrapper → `GetFunction(char*)` → `GetName()` → **infinite recursion** (`Conv_NameToString → GetFunction → GetName → Conv_NameToString …` = stack overflow) | ONE direct ProcessEvent on the KismetStringLibrary CDO; the UFunction is resolved once by full path |
| `UClass::GetFunction(char*, char*)` | `Clss->GetName()` per super-class (ProcessEvent per lookup) | names resolved once to FName (cached), compared as plain indices |
| `UEngine::GetEngine()` | walked all GObjects with `IsA()` | reads the `GEngine` global directly |
| GObjects | layout-validation probing + full-array walks everywhere | by-index item reads only (FWeakObjectPtr), padded chunked layout |

No FNamePool offsets are used anywhere — name conversion goes through the
engine's own `Conv_NameToString` / `Conv_StringToName`.

## Runtime flow (as `4d7e591`)

1. `SetDedicatedServerMode()` — `GIsEditor=0 GIsClient=0 GIsServer=1`.
2. Global **`ProcessEvent` Dobby hook** + cached `UFunction` pointers.
3. `Misc::Listen()` — CreateNetDriver + InitListen + LevelCollections.
4. Map travel (`open Artemis_Terrain` / `open Creative_NoApollo_Terrain`).

## Build

GitHub Actions builds the artifact on every push to `main`
(ARM64, Android 24, NDK 27.0.12077973 — the toolchain the SDK headers require;
`<format>` / libc++ concepts are not available in r25c).

Local build:

```sh
cmake -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
  -DCMAKE_BUILD_TYPE=Release \
  -B build -S .
cmake --build build -j 1   # -j 2+ OOMs on machines with <4 GB RAM
```

`libgameserver.so` is self-contained (no `libc++_shared.so` DT_NEEDED —
dlopen-safe inside the game process).

Log file: `/storage/emulated/0/Android/data/<pkg>/files/OwenGameServer.txt`.

> History: the Sep-19/20 "client-app refactor" (ExecFunction hooks,
> SetClientOffOnly, MD7 bridge, CrashForensics) is preserved on branch
> `backup/crash-forensics-debug`.
