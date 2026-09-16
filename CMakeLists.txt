cmake_minimum_required(VERSION 3.18)
project(OwenGameServer CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT ANDROID_ABI STREQUAL "arm64-v8a")
    message(FATAL_ERROR "OwenGameServer supports arm64-v8a only")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -O2 -Wno-everything -fno-rtti -fvisibility=hidden")

add_library(dobby STATIC IMPORTED)
set_target_properties(dobby PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/external/Dobby/libs/${ANDROID_ABI}/libdobby.a
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/external/Dobby/include
)

add_library(curl STATIC IMPORTED)
set_target_properties(curl PROPERTIES
    IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/external/curl/libs/${ANDROID_ABI}/libcurl.a
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/external/curl/include
)

file(GLOB_RECURSE OWEN_SOURCES
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

set(SDK_DIR ${CMAKE_SOURCE_DIR}/include/SDK)

set(SDK_SOURCES
    ${SDK_DIR}/CoreUObject_functions.cpp
    ${SDK_DIR}/Engine_functions.cpp
    ${SDK_DIR}/FortniteGame_functions.cpp
    ${SDK_DIR}/GameplayAbilities_functions.cpp
)

add_library(gameserver SHARED ${OWEN_SOURCES} ${SDK_SOURCES})

target_include_directories(gameserver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/modules
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/external/Dobby/include
    ${CMAKE_SOURCE_DIR}/external/curl/include
)

target_precompile_headers(gameserver PRIVATE ${CMAKE_SOURCE_DIR}/src/pch.h)

target_link_libraries(gameserver
    dobby
    curl
    log
    android
    z
    dl
)
