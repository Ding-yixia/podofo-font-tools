# PoDoFo Font Tools

PDF 字体分类与嵌入工具，基于 [PoDoFo](https://github.com/podofo/podofo) 库和 PDF 规范（ISO 32000-1:2008 / ISO 32000-2:2020）实现。

## 功能

- **字体扫描**：扫描 Windows 系统字体目录（`C:\Windows\Fonts`），支持 `.ttf`、`.ttc`、`.otf`、`.pfb`、`.pfa`
- **PDF 规范分类**：按 ISO 32000 标准将字体分类为 TrueType、OpenType CFF、Type 1、CIDFont 等类型
- **字体嵌入**：为每个字体生成两版 PDF：
  - `full.pdf` - 全量嵌入（`DontSubset`）
  - `subset.pdf` - 子集嵌入（仅含使用的字形）
- **字体类型过滤**：支持按 PDF 字体类型筛选，只处理特定类型的字体

## Output Structure

```
output/
  TrueType/              TrueType/OpenType with "glyf" table
    Arial/
      full.pdf           全量嵌入 (FontFile2)
      subset.pdf         子集嵌入
  OpenType_CFF/          OpenType with CFF/CFF2 table
    NotoSansSC/
      full.pdf           全量嵌入 (FontFile3 /Subtype /OpenType)
      subset.pdf         子集嵌入
  Type1/                 PostScript Type 1 (if found)
  Type1_CFF/             Type 1 in CFF format
  CIDFontType0_CFF/      CIDFont with CFF outlines
  CIDFontType2_TrueType/ CIDFont with TrueType outlines
```

## Dependencies

- [PoDoFo](https://github.com/podofo/podofo) 1.1.1+ - PDF manipulation library
- [vcpkg](https://github.com/microsoft/vcpkg) - C++ package manager
- Windows SDK (for Win32 font enumeration)

## Quick Start

```bash
# 1. Install vcpkg if not already installed
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg && .\bootstrap-vcpkg.bat

# 2. Configure and build
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

# 3. Run (Windows: generate PDFs for all TrueType fonts, max 10 fonts)
.\build\Release\podofo-font-classifier.exe --type truetype --max 10 output_dir
```

See [BUILD.md](BUILD.md) for detailed build instructions.
See [USAGE.md](USAGE.md) for detailed usage instructions.

## License

MIT
