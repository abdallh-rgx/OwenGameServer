cmake_minimum_required(VERSION 3.18)
project(OwenGameServer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(ANDROID_ABI "arm64-v8a")

set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)

set(DOBBY_LIB
    ${CMAKE_SOURCE_DIR}/external/Dobby/libs/${ANDROID_ABI}/libdobby.a
)

set(CURL_LIB
    ${CMAKE_SOURCE_DIR}/external/curl/libs/${ANDROID_ABI}/libcurl.a
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

add_library(curl STATIC IMPORTED)

set_target_properties(curl PROPERTIES
    IMPORTED_LOCATION "${CURL_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/external/curl/include"
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

target_include_directories(gameserver PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src/core"
    "${CMAKE_SOURCE_DIR}/src/modules"
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/external/Dobby/include"
    "${CMAKE_SOURCE_DIR}/external/curl/include"
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
    -Wl,-z,now
)

target_link_libraries(gameserver
    dobby
    curl
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
message(STATUS "PCH: disabled")
message(STATUS "Debug sections: removed")
message(STATUS "================================")
