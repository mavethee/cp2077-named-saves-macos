cmake_minimum_required(VERSION 3.20)
project(NamedSaves VERSION 2.7.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_OSX_ARCHITECTURES "arm64")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -undefined dynamic_lookup")

add_compile_definitions(RED4EXT_PLATFORM_MACOS)

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/../RED4ext.SDK-macos/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

add_library(NamedSaves SHARED
    src/Main.cpp
)

set_target_properties(NamedSaves PROPERTIES
    PREFIX ""
    SUFFIX ".dylib"
    OUTPUT_NAME "NamedSaves"
)
