cmake_minimum_required(VERSION 3.15)
project(EuroScopeSnake LANGUAGES CXX)

#
# EuroScope is a 32-bit application, so the plugin DLL MUST be built for x86.
# Configure with the Win32 platform, e.g.:
#
#   cmake -A Win32 -DEUROSCOPE_SDK=C:/path/to/EuroScopeSDK -B build
#   cmake --build build --config Release
#
# EUROSCOPE_SDK must point at a folder containing EuroScopePlugIn.h and
# EuroScopePlugIn.lib (shipped with EuroScope — see README.md).
#

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED EUROSCOPE_SDK)
    set(EUROSCOPE_SDK "${CMAKE_SOURCE_DIR}/sdk" CACHE PATH "Path to the EuroScope plugin SDK")
endif()

add_library(EuroScopeSnake SHARED
    src/SnakePlugin.cpp
    src/SnakeWindow.cpp
)

target_include_directories(EuroScopeSnake PRIVATE
    "${EUROSCOPE_SDK}"
    src
)

# Link against the EuroScope import library and the Win32 GDI/User libraries.
find_library(EUROSCOPE_LIB
    NAMES EuroScopePlugIn
    PATHS "${EUROSCOPE_SDK}"
)

if(EUROSCOPE_LIB)
    target_link_libraries(EuroScopeSnake PRIVATE "${EUROSCOPE_LIB}")
else()
    message(WARNING
        "EuroScopePlugIn.lib not found under EUROSCOPE_SDK='${EUROSCOPE_SDK}'. "
        "Set -DEUROSCOPE_SDK=<folder containing EuroScopePlugIn.h/.lib>.")
endif()

target_link_libraries(EuroScopeSnake PRIVATE user32 gdi32)

if(MSVC)
    target_compile_definitions(EuroScopeSnake PRIVATE
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
    )
endif()

# Produce e.g. build/Release/EuroScopeSnake.dll
set_target_properties(EuroScopeSnake PROPERTIES
    OUTPUT_NAME "EuroScopeSnake"
    PREFIX ""
    SUFFIX ".dll"
)
