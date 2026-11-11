cmake_minimum_required(VERSION 3.28)
project(onlyc VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "OnlyC requires Clang. Configure with -DCMAKE_C_COMPILER=clang.")
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "wasm32")
    set(ONLYC_WASM ON)
    set(ONLYC_BUILD_TESTS OFF)
else()
    option(ONLYC_BUILD_TESTS "Build native GoogleTest tests" ON)
    option(ONLYC_BUILD_WEB "Build the WebAssembly website" ON)
endif()
option(ONLYC_SANITIZE "Enable address and undefined behavior sanitizers in native builds" OFF)
set(
    ONLYC_SITE_ORIGIN
    "https://34049.trungnt2910.com"
    CACHE
        STRING
        "Public origin for link previews"
)
set(ONLYC_GENERATED_DIR "${PROJECT_BINARY_DIR}/generated" CACHE PATH "Native generated artifacts")
set(ONLYC_SITE_DIR "${PROJECT_BINARY_DIR}/site" CACHE PATH "Deployable static website")
set(ONLYC_WASM_CMAKE_ARGS "" CACHE STRING "Additional CMake arguments for the WASM build")

if(ONLYC_BUILD_TESTS)
    enable_language(CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

include(cmake/Targets.cmake)
include(cmake/Strings.cmake)
include(cmake/ThirdParty.cmake)
include(cmake/Tests.cmake)
add_subdirectory(src)
if(NOT ONLYC_WASM)
    add_subdirectory(tools)
    include(cmake/Web.cmake)
    if(ONLYC_BUILD_TESTS)
        add_subdirectory(tests)
    endif()
endif()
