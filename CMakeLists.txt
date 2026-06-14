cmake_minimum_required(VERSION 3.23)
project(podofo-font-tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find PoDoFo via vcpkg
find_package(podofo CONFIG REQUIRED)

add_subdirectory(src/font-classifier)
