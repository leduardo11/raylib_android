cmake_minimum_required(VERSION 3.16)
project(raylib_android LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Platform detection ─────────────────────────────────────────────────────
if(ANDROID)
    set(PLATFORM_ANDROID TRUE)
    message(STATUS "Building for Android")
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(PLATFORM_LINUX TRUE)
elseif(WIN32)
    set(PLATFORM_WIN32 TRUE)
elseif(APPLE)
    set(PLATFORM_MACOS TRUE)
endif()

# ── Source files ──────────────────────────────────────────────────────────
file(GLOB_RECURSE APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# AndroidEngine/crash_handler.cpp won't compile outside Android
if(NOT PLATFORM_ANDROID)
    list(REMOVE_ITEM APP_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/AndroidEngine/crash_handler.cpp"
    )
endif()

# ── Fetch raylib ──────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.5
    GIT_SHALLOW    TRUE
)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

# Tell raylib to build for Android platform (no GLFW, uses native backend)
if(PLATFORM_ANDROID)
    set(PLATFORM "Android" CACHE STRING "Platform target for raylib" FORCE)
endif()

FetchContent_MakeAvailable(raylib)

# ── Build target ──────────────────────────────────────────────────────────
if(PLATFORM_ANDROID)
    add_library(raylib_android SHARED ${APP_SOURCES})
    set_target_properties(raylib_android PROPERTIES
        OUTPUT_NAME "raylib_android"
        PREFIX "lib"
    )
else()
    add_executable(raylib_android ${APP_SOURCES})
endif()

target_include_directories(raylib_android PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/AndroidEngine
)

# ── Link ──────────────────────────────────────────────────────────────────
if(PLATFORM_ANDROID)
    # --whole-archive required: raylib's ANativeActivity_onCreate is called
    # by Android's NativeActivity via dlsym (linker can't see the reference).
    target_link_libraries(raylib_android PRIVATE
        -Wl,--whole-archive
        raylib
        -Wl,--no-whole-archive
        android
        log
    )
else()
    target_link_libraries(raylib_android PRIVATE raylib)
endif()
