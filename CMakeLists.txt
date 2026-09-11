cmake_minimum_required(VERSION 3.20)
project(Vayu LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /utf-8 /EHsc)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    # LLVM headers use some C4505-style warnings we don't care about.
    add_compile_options(/wd4244 /wd4267 /wd4624)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------- LLVM ----------
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Vayu: LLVM ${LLVM_PACKAGE_VERSION} at ${LLVM_DIR}")

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Components needed for LLJIT + native codegen on any host.
llvm_map_components_to_libnames(LLVM_LIBS
    Core
    Support
    IRReader
    OrcJIT
    OrcTargetProcess
    OrcShared
    ExecutionEngine
    RuntimeDyld
    native
    nativecodegen
    Passes
    TransformUtils
    Analysis
    MC
    MCParser
    Object
    BitReader
    Remarks
    TargetParser
    Demangle
    DebugInfoDWARF
    DebugInfoMSF
    DebugInfoPDB
    Symbolize
)

add_subdirectory(compiler)