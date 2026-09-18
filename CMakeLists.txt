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

target_link_options(gameserver PRIVATE
    -Wl,--gc-sections
    -Wl,--as-needed
    -Wl,-z,relro
    -Wl,-z,lazy
)

target_link_libraries(gameserver
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
message(STATUS "As-needed: enabled")
message(STATUS "curl: patched (no HTTP2 objects)")
message(STATUS "AR tool: ${CMAKE_AR}")
message(STATUS "================================")
