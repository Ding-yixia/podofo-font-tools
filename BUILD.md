# Build Instructions

## Prerequisites

- **Windows 10/11** (64-bit)
- **Visual Studio 2022** with "Desktop development with C++" workload
- **CMake** 3.23+ (included with VS2022)
- **Git**

## Step 1: Install vcpkg

```powershell
# Clone vcpkg
git clone https://github.com/microsoft/vcpkg.git C:\dev\vcpkg
cd C:\dev\vcpkg

# Bootstrap
.\bootstrap-vcpkg.bat
```

## Step 2: Install Dependencies

The project uses vcpkg manifest mode. Dependencies are defined in `vcpkg.json` and will be installed automatically during CMake configuration:

```json
{
  "dependencies": [
    {
      "name": "podofo",
      "features": ["fontmanager", "fontconfig"]
    }
  ]
}
```

This will install:
- PoDoFo 1.1.1+ (with font manager and fontconfig support)
- FreeType (font rasterization)
- Fontconfig (cross-platform font search)
- zlib, libpng, libjpeg, libtiff (PoDoFo image support)
- OpenSSL (PoDoFo encryption support)
- libxml2 (PoDoFo XML support)

## Step 3: Build

```powershell
# Navigate to project root
cd podofo-font-tools

# Configure with vcpkg toolchain
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake

# Build
cmake --build build --config Release

# (Optional) Build debug version
cmake --build build --config Debug
```

The executable will be at:
- Release: `build\Release\podofo-font-classifier.exe`
- Debug:   `build\Debug\podofo-font-classifier.exe`

## Troubleshooting

### vcpkg manifest not found

If you see "manifest not found" error, make sure `vcpkg.json` is in the project root.

### PoDoFo not found

If CMake cannot find PoDoFo:
```powershell
# Clean and retry
Remove-Item -Recurse -Force build
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake
```

### Compile errors with C++17

This project requires C++17. If you see `std::string_view` or `std::filesystem` errors, ensure your CMakeLists.txt has:
```cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

### Fontconfig on Windows

Fontconfig on Windows should find fonts in `C:\Windows\Fonts` automatically.
If fonts are not found, check fontconfig configuration at:
`C:\dev\vcpkg\installed\x64-windows\etc\fonts\fonts.conf`
