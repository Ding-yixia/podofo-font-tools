# Build Instructions

## Table of Contents

1. [Prerequisites](#prerequisites)
2. [Step 1: Install vcpkg](#step-1-install-vcpkg)
3. [Step 2: Dependencies and Fontconfig](#step-2-dependencies-and-fontconfig)
4. [Step 3: Build](#step-3-build)
5. [Step 4: Run](#step-4-run)
6. [Troubleshooting](#troubleshooting)
7. [Technical Deep Dive: Font Embedding Mechanism](#technical-deep-dive-font-embedding-mechanism)
8. [Build Variants Comparison](#build-variants-comparison)

---

## Prerequisites

- **Windows 10/11** (64-bit)
- **Visual Studio 2022** with "Desktop development with C++" workload
- **CMake** 3.23+ (included with VS2022)
- **Git**

---

## Step 1: Install vcpkg

```powershell
# Clone vcpkg
git clone https://github.com/microsoft/vcpkg.git C:\dev\vcpkg
cd C:\dev\vcpkg

# Bootstrap
.\bootstrap-vcpkg.bat
```

---

## Step 2: Dependencies and Fontconfig

### 2.1 vcpkg Manifest

The project uses vcpkg manifest mode. Dependencies are defined in `vcpkg.json`:

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

> **IMPORTANT**: The `fontconfig` feature is **mandatory** for correct font embedding. See the [Technical Deep Dive](#technical-deep-dive-font-embedding-mechanism) section below for explanation.

### 2.2 Installing with fontconfig

If you need to install or reinstall podofo with fontconfig support:

```powershell
cd C:\dev\vcpkg

# Install podofo with fontconfig
.\vcpkg install podofo[fontmanager,fontconfig] --recurse

# (Optional) Verify the installed features
.\vcpkg list | Select-String "podofo|fontconfig"
```

Expected output:
```
fontconfig:x64-windows                         2.17.1          ...
podofo:x64-windows                             1.1.1           PoDoFo is a library...
podofo[fontconfig]:x64-windows                                  Use Fontconfig
podofo[fontmanager]:x64-windows                                 Enable font manager
```

### 2.3 What Gets Installed

| Package | Version | Role |
|---------|---------|------|
| **PoDoFo** | 1.1.1+ | PDF manipulation library (core) |
| **FreeType** | 2.14.3 | Font rasterization and glyph metrics |
| **Fontconfig** | 2.17.1 | Cross-platform font search (critical for embedding) |
| **zlib** | 1.2.11 | DEFLATE compression for PDF streams |
| **libxml2** | 2.14.5 | XML parsing for fontconfig configuration |
| **OpenSSL** | 1.1.1 | PDF encryption support |
| **libpng / libjpeg / libtiff** | - | Image format support in PDF |
| **brotli / bzip2** | - | WOFF2 and compressed font support (transitive) |

---

## Step 3: Build

### 3.1 Configure

```powershell
# Navigate to project root
cd podofo-font-tools

# Configure with vcpkg toolchain
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake
```

> **Note**: If CMake cannot find PoDoFo, or if you want to use a specific pre-installed
> PoDoFo package (e.g., to bypass manifest mode), you can specify the path directly:
> ```powershell
> cmake -B build -S . -DPODOFO_DIR=C:/dev/vcpkg/installed/x64-windows/share/podofo
> ```

### 3.2 Compile

```powershell
# Release build
cmake --build build --config Release

# Debug build (optional)
cmake --build build --config Debug
```

### 3.3 Output

The executable will be at:
- Release: `build\Release\podofo-font-classifier.exe`
- Debug:   `build\Debug\podofo-font-classifier.exe`

---

## Step 4: Run

```powershell
# Basic: process first 5 fonts
build\Release\podofo-font-classifier.exe --max 5 output

# Process only TrueType fonts
build\Release\podofo-font-classifier.exe --type truetype --max 10 output

# Process only OpenType CFF fonts
build\Release\podofo-font-classifier.exe --type opentype output
```

See [USAGE.md](USAGE.md) for complete usage documentation.

---

## Troubleshooting

### PoDoFo not found

If CMake cannot find PoDoFo:
```powershell
# Clean and retry with explicit path
Remove-Item -Recurse -Force build
cmake -B build -S . -DPODOFO_DIR=C:/dev/vcpkg/installed/x64-windows/share/podofo
```

### C++17 compilation errors

If you see `std::string_view` or `std::filesystem` errors, ensure C++17 standard is set:
```cmake
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

### vcpkg manifest not found

If you see "manifest not found" error, make sure `vcpkg.json` is in the project root directory.

### Fonts not found on Windows

Fontconfig on Windows should find fonts in `C:\Windows\Fonts` automatically.
If fonts are not found, check fontconfig configuration at:
`C:\dev\vcpkg\installed\x64-windows\etc\fonts\fonts.conf`

### Font embedding not working (fonts not embedded in PDF)

If the generated PDF files contain fonts but they are NOT embedded
(no `/FontFile`, `/FontFile2`, or `/FontFile3` entries), the most likely cause
is that PoDoFo was built **without** the `fontconfig` feature. See the
[Technical Deep Dive](#technical-deep-dive-font-embedding-mechanism) section
for a complete explanation of why this happens and how to fix it.

Check your installation:
```powershell
.\vcpkg list | Select-String "podofo"
```

If you see only `podofo[core,fontmanager]` (no `fontconfig`), reinstall with:
```powershell
.\vcpkg install podofo[fontmanager,fontconfig] --recurse
```

---

## Technical Deep Dive: Font Embedding Mechanism

### Overview

Font embedding in PoDoFo involves two core classes: `PdfFontManager` (font
manager, orchestrates font creation and embedding) and `PdfFont` (font object,
handles the actual PDF font dictionary and font program data).

The embedding process has three stages:
1. **Font creation** -- How a font is loaded into memory
2. **Font caching** -- How the font manager tracks fonts
3. **Font embedding** -- How font program data is written into the PDF

### 1. Font Creation: Two Code Paths

PoDoFo provides two ways to create a font:

#### Path A: `SearchFont()` -- Name-based lookup (enables embedding)

```cpp
PdfFont* font = doc.GetFonts().SearchFont("Arial", searchParams, createParams);
```

Internal call chain:
```
PdfFontManager::SearchFont(pattern)
  → getImportedFont(pattern)
    → searchFontMetrics(fontName)     // Find font file on system
      → [Fontconfig] SearchFontPath() // Needs PODOFO_HAVE_FONTCONFIG
      → [Win32 GDI] getWin32FontData() // Needs PODOFO_ENABLE_WIN32GDI_FONT_SEARCH
      → return filepath or nullptr
    → PdfFontMetrics::Create(filepath, faceIndex)
    → PdfFont::Create(doc, metrics, createParams)
    → addImported(font)               // <-- KEY: adds to m_cachedQueries
      → m_cachedQueries[Descriptor] = fonts  // Font goes into the QUERY CACHE
      → m_fonts[ref] = font                  // Font also goes into global FONT MAP
```

Source: `PdfFontManager.cpp`, ~line 370-404 (`searchFontMetrics`)

```cpp
static unique_ptr<const PdfFontMetrics> searchFontMetrics(
    const string_view& fontName, const PdfFontSearchParams& params,
    const PdfFontMetrics* refMetrics, bool skipNormalization)
{
    string path;
#ifdef PODOFO_HAVE_FONTCONFIG
    // Path 1: Fontconfig search (needs PODOFO_HAVE_FONTCONFIG defined)
    auto& fc = GetFontConfigWrapper();
    path = fc.SearchFontPath(fontName, fcParams, faceIndex);
#endif

    unique_ptr<const PdfFontMetrics> ret = nullptr;
    if (!path.empty())
        ret = PdfFontMetrics::CreateFromFile(path, faceIndex, ...);

    if (ret == nullptr)
    {
#if defined(_WIN32) && defined(PODOFO_ENABLE_WIN32GDI_FONT_SEARCH)
        // Path 2: Win32 GDI search (needs PODOFO_ENABLE_WIN32GDI_FONT_SEARCH defined)
        auto data = getWin32FontData(fontName, params);
        if (data != nullptr)
            ret = PdfFontMetrics::CreateFromFace(face, std::move(data), ...);
#endif
    }
    return ret;  // Both paths fail → returns nullptr
}
```

#### Path B: `GetOrCreateFont(filepath)` -- Direct file load (NO embedding)

```cpp
PdfFont& font = doc.GetFonts().GetOrCreateFont(
    "C:\\Windows\\Fonts\\arial.ttf", 0, createParams);
```

Internal call chain:
```
PdfFontManager::GetOrCreateFont(filepath, faceIndex, params)
  → PdfFontMetrics::CreateFromFile(filepath, faceIndex)
  → PdfFont::Create(doc, metrics, params)
  → getOrCreateFontHashed(metrics, params)
    → m_fonts[ref] = {false, std::move(font)}  // Font goes into global FONT MAP only
    // NOTE: Does NOT add to m_cachedQueries !!
```

### 2. Font Caching: Two Internal Tables

Defined in `PdfFontManager.h`, ~line 183-193:

```cpp
// Fonts created via SearchFont() -- used by EmbedFonts()
CachedQueries m_cachedQueries;
// Key:   Descriptor(fontName, std14Type, encodingId, hasFontStyle, style)
// Value: vector<PdfFont*>

// Fonts created via GetOrCreateFont(filepath) -- NOT used by EmbedFonts()
CachedPaths m_cachedPaths;
// Key:   PathDescriptor(filepath, faceIndex, encodingId)
// Value: PdfFont*

// Global font map -- ALL fonts are tracked here
FontMap m_fonts;
// Key:   PdfReference (indirect object reference)
// Value: Storage{bool IsLoaded, unique_ptr<PdfFont> Font}
```

**Critical distinction**: Both `m_cachedQueries` and `m_cachedPaths` point to
the same font objects in `m_fonts`. The issue is NOT about font objects not
existing -- it's about which entries `EmbedFonts()` iterates over.

### 3. Font Embedding: Where the Break Occurs

Source: `PdfFontManager.cpp`, ~line 406-423

```cpp
void PdfFontManager::EmbedFonts()
{
    // 1. Collect fonts from cached queries ONLY
    set<PdfReference> fontToEmbeds;
    for (auto& pair : m_cachedQueries)        // <-- ONLY m_cachedQueries
    {
        for (auto& font : pair.second)
            fontToEmbeds.insert(font->GetObject().GetIndirectReference());
    }

    // 2. Fonts in m_cachedPaths are COMPLETELY IGNORED here

    // 3. Embed each collected font
    for (auto& ref : fontToEmbeds)
        m_fonts[ref].Font->EmbedFont();

    // 4. The EmbedFont() call (in PdfFont.cpp) dispatches based on font file type:
    //    - PdfFontFileType::Type1       → EmbedFontFileType1()    → /FontFile
    //    - PdfFontFileType::TrueType    → EmbedFontFileTrueType() → /FontFile2
    //    - PdfFontFileType::OpenTypeCFF → EmbedFontFileOpenType() → /FontFile3 /Subtype /OpenType

    m_cachedQueries.clear();
}
```

The concrete embedding methods in `PdfFont.cpp` (~line 570-620):

```cpp
void PdfFont::EmbedFontFileTrueType(PdfDictionary& descriptor, const bufferview& data) const
{
    embedFontFileData(descriptor, "FontFile2"_n, [&data](PdfDictionary& dict)
    {
        dict.AddKey("Length1"_n, static_cast<int64_t>(data.size()));
    }, data);
}

void PdfFont::EmbedFontFileOpenType(PdfDictionary& descriptor, const bufferview& data) const
{
    embedFontFileData(descriptor, "FontFile3"_n, [](PdfDictionary& dict)
    {
        dict.AddKey("Subtype"_n, "OpenType"_n);
    }, data);
}

void PdfFont::embedFontFileData(PdfDictionary& descriptor, const PdfName& fontFileName,
    const function<void(PdfDictionary& dict)>& dictWriter, const bufferview& data) const
{
    auto& contents = GetDocument().GetObjects().CreateDictionaryObject();
    descriptor.AddKeyIndirect(fontFileName, contents);
    dictWriter(contents.GetDictionary());
    contents.GetOrCreateStream().SetData(data);  // Write font program data as PDF stream
}
```

### 4. The Root Cause: Why fontconfig Matters

vcpkg's podofo port can be built with three feature configurations:

| Configuration | `PODOFO_HAVE_FONTCONFIG` | `PODOFO_ENABLE_WIN32GDI_FONT_SEARCH` | `SearchFont()` Result | Font Embedded? |
|---|---|---|---|---|
| **core only** | Undefined | Undefined | Always returns nullptr | **NO** |
| **core + fontmanager** | Undefined | Undefined | Always returns nullptr | **NO** |
| **core + fontmanager + fontconfig** | **Defined** | Undefined | **Finds fonts via Fontconfig** | **YES** |
| Playground 1.2.0 build | Defined | Defined | Dual search (FC + GDI) | **YES** |

When `SearchFont()` returns nullptr (no fontconfig, no GDI):
```
main.cpp:

  // Step 1: Try SearchFont (preferred, enables embedding)
  pdfFont = doc.GetFonts().SearchFont("Arial");     // → nullptr
  //                                ↑
  //                    Needs fontconfig to work!

  // Step 2: Fallback to file path load
  pdfFont = &doc.GetFonts().GetOrCreateFont(
      "C:\\Windows\\Fonts\\arial.ttf", 0, params);  // → Works, but NO embedding
```

### 5. How the Fix Works

When `podofo[fontmanager,fontconfig]` is installed:

1. `PODOFO_HAVE_FONTCONFIG` is defined in the build
2. `searchFontMetrics()` uses `PdfFontConfigWrapper::SearchFontPath()` to
   find font files by name
3. `SearchFont("Arial")` finds the file and creates the font through `addImported()`
4. `addImported()` adds the font to `m_cachedQueries`
5. `EmbedFonts()` iterates `m_cachedQueries` and calls `EmbedFont()` on each
6. `EmbedFont()` reads the font program data via `GetOrLoadFontFileData()`
7. The font data is written as a PDF stream (`/FontFile2` for TrueType,
   `/FontFile3 /Subtype /OpenType` for OpenType CFF)
8. **Result**: The generated PDF contains fully embedded fonts

### 6. Verification: How to Check if Fonts Are Embedded

After generating a PDF, you can verify font embedding by inspecting the PDF's
internal structure. Each embedded font will have a `FontDescriptor` dictionary
containing one of these keys:

| PDF Key | Font Type | Example |
|---------|-----------|---------|
| `/FontFile` | Type 1 (PFB/PFA) | `FontFile (Length1=XXXX)` |
| `/FontFile2` | TrueType / OpenType (glyf) | `FontFile2 (Length1=123456)` |
| `/FontFile3` | CFF / OpenType CFF | `FontFile3 /Subtype /OpenType` |

A non-embedded font will have a `FontDescriptor` with **none** of these keys.

You can also check file size: a fully embedded CJK font (like Noto Sans SC)
will produce a PDF of **~7MB** (full embedding with ~30,000 glyphs), while
a non-embedded version will be under **100KB** regardless of the font size.

---

## Build Variants Comparison

| Aspect | vcpkg (no fontconfig) | vcpkg (with fontconfig) | Playground 1.2.0 |
|--------|----------------------|------------------------|-------------------|
| **Build time** | Fast (pre-built) | Medium (builds fontconfig) | Slow (builds entire PoDoFo) |
| **Font scanning** | Yes | Yes | Yes |
| **PDF spec classification** | Yes | Yes | Yes |
| **Subsetting** | Partial (metrics only) | Yes | Yes |
| **Font embedding** | **NO** | **YES** | **YES** |
| **CJK fonts embedded** | NO | YES | YES |
| **Dependency management** | vcpkg | vcpkg | Manual (submodule) |
| **PoDoFo version** | 1.1.1 | 1.1.1 | 1.2.0 (source) |
