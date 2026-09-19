# MobileDumper-7 Core (FName bridge)

This folder contains the minimal, dump-free core of
[MobileDumper-7](https://github.com/abdallh-rgx/MobileDumper-7) used by
OwenGameServer to read FName strings safely.

It is produced from MobileDumper-7 **commit `014c0de`** plus the core patch
(branch `md7-core`), built with `-DMD7_BUILD_CORE=ON`.

## What is inside `libMobileDumperCore.a`

| Object | Purpose |
|---|---|
| `MD7CoreDefs.cpp.o` | Definitions of `GObjects`, `GNames`, `GLayouts`, `GOffsets`, `GInSDKOffsets` (normally defined in `Offsets.cpp`, which is *not* compiled here) |
| `NameArray.cpp.o` | `NameArray` / `FNameEntry` — Project A's FNamePool entry lookup |
| `UnrealTypes.cpp.o` | `FName` helpers used by `NameArray` |

## What is *not* inside (guaranteed)

- `main_android.cpp` — the `__attribute__((constructor))` that sleeps 60 s and
  calls `RunDump`. **Not compiled.**
- `DumperMain.cpp` (`RunDump` / `FDumperMain::Run`) — **not compiled.**
- `Generator/*` — no SDK generation, no `.usmap`, no IDA mapping, no
  dumpspace, no GObjects dumps, no zip packaging.
- `MemoryAndroid.cpp` + KittyMemoryEx — not compiled, not linked.
- `UEAnalyzerKitty`, `ObjectArray`, `UnrealObjects`, `UnrealContainers`,
  layout/offset detectors, `Logger.cpp`, `Utils.cpp` — not compiled.

The only `.init_array` entries in the `.a` are standard C++ inline-variable
guard initializers (`GMemory`, `GSettings`, `GLogger`, `InternalSettings`,
`NameArray::Decrypt*Fn`, ...) — they initialize empty `std::function` /
`std::string` objects and never call into the dumper.

## Verifying the `.a` (NDK r25c = 25.2.9519653)

```sh
NM=$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-nm

# 1) No dumper entry points of any kind (must print nothing):
$NM --defined-only external/MobileDumperCore/lib/arm64-v8a/libMobileDumperCore.a \
    | grep -iE "RunDump|DumperMain|KittyMemory|MemoryAndroid|Dumpspace|JNI_OnLoad|InitializeProfiles"

# 2) Which translation units contribute static initializers?
#    (expected: only MD7CoreDefs.cpp / NameArray.cpp / UnrealTypes.cpp
#     with plain inline-variable guards — NOT main_android.cpp)
$NM --defined-only external/MobileDumperCore/lib/arm64-v8a/libMobileDumperCore.a \
    | grep "GLOBAL__sub_I"

# 3) The only globally visible definitions are the FName reader (expected):
$NM --defined-only external/MobileDumperCore/lib/arm64-v8a/libMobileDumperCore.a \
    | grep -E " [TDB] " | c++filt | sort -u
```

## Rebuilding the `.a`

```sh
git clone --branch md7-core --recursive https://github.com/abdallh-rgx/MobileDumper-7.git
cd MobileDumper-7

cmake -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-24 \
    -DANDROID_STL=c++_shared \
    -DCMAKE_BUILD_TYPE=Release \
    -DMD7_BUILD_CORE=ON \
    -B build-core -S .

cmake --build build-core

cp build-core/libMobileDumperCore.a \
   <OwenGameServer>/external/MobileDumperCore/lib/arm64-v8a/
```

`$ANDROID_NDK` must point at NDK **25.2.9519653 (r25c)** — the same NDK used to
build `gameserver.so`. The core needs no submodules other than `libs/fmt`
(headers only; no fmt code is linked).

## Header license

The headers under `include/Dumper/` are taken from MobileDumper-7 (MIT license,
see the upstream repository). They are vendored verbatim (plus the two
r25c-compatibility fixes described in the MobileDumper-7 commit message) so
that OwenGameServer can compile `src/core/mobile_dumper_bridge.cpp` without a
checkout of the dumper repository.
