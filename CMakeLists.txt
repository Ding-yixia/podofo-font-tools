cmake_minimum_required(VERSION 3.23)
project(podofo-font-tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# Multi-threaded compilation support
# ============================================================

# MSVC: /MP (Multi-Process Compilation) parallelizes compilation
# across source files within a single target.
if(MSVC)
    # Use number of processors for maximum parallelism
    cmake_host_system_information(RESULT NUM_CORES QUERY NUMBER_OF_PHYSICAL_CORES)
    if(NOT NUM_CORES)
        set(NUM_CORES 4)
    endif()
    add_compile_options("/MP${NUM_CORES}")

    # Suppress LNK warnings about missing PDBs when linking in parallel
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ignore:4099")
endif()

# Non-MSVC (Unix Makefiles, Ninja): CMake's --parallel flag handles this.
# CMake >= 3.12 supports: cmake --build build --parallel $(nproc)
# Ninja generates optimal parallelism automatically.

# Explicit target for parallel build documentation
message(STATUS "Parallel compilation: enabled")
if(MSVC)
    message(STATUS "  MSVC /MP with ${NUM_CORES} cores")
endif()

# ============================================================
# Find PoDoFo
# ============================================================
# Set PODOFO_DIR to the vcpkg installed podofo config directory
# e.g.: -DPODOFO_DIR=C:/Code/vcpkg/installed/x64-windows/share/podofo
if(DEFINED PODOFO_DIR)
    find_package(podofo CONFIG REQUIRED NO_DEFAULT_PATH PATHS "${PODOFO_DIR}")
else()
    find_package(podofo CONFIG REQUIRED)
endif()

add_subdirectory(src/font-classifier)
