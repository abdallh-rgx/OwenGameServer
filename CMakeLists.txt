cmake_minimum_required(VERSION 3.18)
project(OwenGameServer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(ANDROID_ABI "arm64-v8a")

set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)

if(NOT CMAKE_AR)
    set(CMAKE_AR "ar")
endif()

set(DOBBY_LIB
    ${CMAKE_SOURCE_DIR}/external/Dobby/libs/${ANDROID_ABI}/libdobby.a
)

set(CURL_LIB
    ${CMAKE_SOURCE_DIR}/external/curl/libs/${ANDROID_ABI}/libcurl.a
)

set(CURL_PATCHED_LIB
    ${CMAKE_BINARY_DIR}/libcurl_patched.a
)

if(NOT EXISTS "${DOBBY_LIB}")
    message(FATAL_ERROR "Dobby library not found: ${DOBBY_LIB}")
endif()

if(NOT EXISTS "${CURL_LIB}")
    message(FATAL_ERROR "curl library not found: ${CURL_LIB}")
endif()

add_library(dobby STATIC IMPORTED)
set_target_properties(dobby PROPERTIES
    IMPORTED_LOCATION "${DOBBY_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/external/Dobby/include"
)

# ============================================================
# MobileDumper-7 core (FName reading bridge)
# ------------------------------------------------------------
# Static library built from MobileDumper-7 (commit 014c0de + core patch,
# branch md7-core) with -DMD7_BUILD_CORE=ON:
#
#   cmake -G Ninja \
#     -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
#     -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24 \
#     -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release \
#     -DMD7_BUILD_CORE=ON -B build-core -S /path/to/MobileDumper-7
#   cmake --build build-core
#   cp build-core/libMobileDumperCore.a  <this-repo>/external/MobileDumperCore/lib/arm64-v8a/
#
# NOTE: the final link of gameserver MUST use ANDROID_STL=c++_static (the
# NDK toolchain default). c++_shared would add DT_NEEDED "libc++_shared.so";
# the .so is loaded inside a patched Fortnite process that does not ship that
# library, so dlopen() would fail at app start with no tombstone. The STL
# setting of the .a itself is irrelevant (static archives carry unresolved
# symbols that are resolved at the final link), but c++_static is used there
# too to keep every documented command consistent.
#
# The .a contains ONLY the FNamePool reader (NameArray / FNameEntry / FName +
# globals). It has NO constructor (main_android.cpp is not compiled), NO
# RunDump / DumperMain, NO SDK generation, NO KittyMemoryEx. See
# external/MobileDumperCore/README.md for the full manifest and the nm
# verification commands.
# ============================================================
set(MD7_CORE_DIR "${CMAKE_SOURCE_DIR}/external/MobileDumperCore")
set(MD7_CORE_LIB "${MD7_CORE_DIR}/lib/${ANDROID_ABI}/libMobileDumperCore.a")

if(NOT EXISTS "${MD7_CORE_LIB}")
    message(FATAL_ERROR
        "MobileDumperCore library not found: ${MD7_CORE_LIB}\n"
        "Build it from MobileDumper-7 with -DMD7_BUILD_CORE=ON and copy "
        "libMobileDumperCore.a into external/MobileDumperCore/lib/${ANDROID_ABI}/ "
        "(see external/MobileDumperCore/README.md).")
endif()

add_library(md7core STATIC IMPORTED)
set_target_properties(md7core PROPERTIES
    IMPORTED_LOCATION "${MD7_CORE_LIB}"
    # Both styles work: #include "Dumper/Memory/IMemory.h" and #include "Memory/IMemory.h"
    INTERFACE_INCLUDE_DIRECTORIES "${MD7_CORE_DIR}/include;${MD7_CORE_DIR}/include/Dumper"
)

set(PATCH_SCRIPT "${CMAKE_BINARY_DIR}/patch_curl.sh")

file(WRITE ${PATCH_SCRIPT}
    "#!/bin/sh\n"
    "set -e\n"
    "cp -f \"${CURL_LIB}\" \"${CURL_PATCHED_LIB}\"\n"
    "for obj in $(${CMAKE_AR} t \"${CURL_PATCHED_LIB}\" | grep -E 'h2|http2' || true); do\n"
    "  ${CMAKE_AR} d \"${CURL_PATCHED_LIB}\" \"$obj\"\n"
    "done\n"
    "echo 'Patched libcurl.a'\n"
)

add_custom_command(
    OUTPUT ${CURL_PATCHED_LIB}
    COMMAND chmod +x ${PATCH_SCRIPT}
    COMMAND sh ${PATCH_SCRIPT}
    DEPENDS ${CURL_LIB} ${PATCH_SCRIPT}
    COMMENT "Patching libcurl.a (removing all HTTP2-related objects)"
)

add_custom_target(patch_curl ALL
    DEPENDS ${CURL_PATCHED_LIB}
)

file(GLOB_RECURSE OWEN_SOURCES
    CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

set(SDK_DIR "${CMAKE_SOURCE_DIR}/include/SDK")

set(SDK_SOURCES
    "${SDK_DIR}/CoreUObject_functions.cpp"
    "${SDK_DIR}/Engine_functions.cpp"
    "${SDK_DIR}/FortniteGame_functions.cpp"
    "${SDK_DIR}/GameplayAbilities_functions.cpp"
)

add_library(gameserver SHARED
    ${OWEN_SOURCES}
    ${SDK_SOURCES}
)

add_dependencies(gameserver patch_curl)

target_include_directories(gameserver PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src/core"
    "${CMAKE_SOURCE_DIR}/src/modules"
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/external/Dobby/include"
    "${CMAKE_SOURCE_DIR}/external/curl/include"
)

target_compile_definitions(gameserver PRIVATE
    CURL_STATICLIB
)

target_compile_options(gameserver PRIVATE
    -O2
    -fPIC
    -fno-rtti
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
    -fomit-frame-pointer
    -Wno-everything
)

# The MobileDumper-7 bridge TU derives from IMemory (a polymorphic interface
# compiled WITH rtti inside libMobileDumperCore.a), so it needs RTTI enabled
# and frame pointers kept for diagnosability. Source-file options are appended
# AFTER the target options above, so -frtti wins over -fno-rtti.
set_source_files_properties(
    "${CMAKE_SOURCE_DIR}/src/core/mobile_dumper_bridge.cpp"
    PROPERTIES
        COMPILE_OPTIONS "-frtti;-fno-omit-frame-pointer"
)

target_link_options(gameserver PRIVATE
    -Wl,--gc-sections
    -Wl,--as-needed
    -Wl,-z,relro
    -Wl,-z,lazy
    -Wl,--allow-multiple-definition
    # Hide every symbol that comes from a static archive (libc++_static.a,
    # libc++abi.a, libunwind.a, libdobby.a, libcurl.a, libMobileDumperCore.a)
    # from the DYNAMIC symbol table. Internal references are already bound at
    # static-link time, so this only stops the .so from EXPORTING ~2000
    # libc++/Dobby/curl symbols into the game process - which could otherwise
    # interpose on libraries loaded later in the same classloader namespace.
    # The only remaining exports are JNI_OnLoad / JNI_OnUnload (defined in our
    # own object files), exactly what a loadable JNI library needs.
    -Wl,--exclude-libs,ALL
)

target_link_libraries(gameserver
    md7core
    dobby
    ${CURL_PATCHED_LIB}
    log
    android
    z
    dl
)

find_program(LLVM_OBJCOPY
    NAMES
        llvm-objcopy
        llvm-objcopy-21
)

if(NOT LLVM_OBJCOPY)
    message(FATAL_ERROR "llvm-objcopy not found")
endif()

add_custom_command(TARGET gameserver POST_BUILD
    COMMAND ${LLVM_OBJCOPY}
        --remove-section=.debug_abbrev
        --remove-section=.debug_info
        --remove-section=.debug_line
        --remove-section=.debug_loc
        --remove-section=.debug_ranges
        --remove-section=.debug_str
        $<TARGET_FILE:gameserver>
    COMMAND ${CMAKE_COMMAND} -E echo
        "Optimized gameserver.so"
)

message(STATUS "================================")
message(STATUS "OwenGameServer")
message(STATUS "ABI: ${ANDROID_ABI}")
message(STATUS "Build: ${CMAKE_BUILD_TYPE}")
message(STATUS "Optimization: O2")
message(STATUS "GC Sections: enabled")
message(STATUS "curl: patched + stubs")
message(STATUS "MobileDumper-7 core: ${MD7_CORE_LIB}")
message(STATUS "================================")
