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
    Arial_full.pdf       全量嵌入 (FontFile2)
    Arial_subset.pdf     子集嵌入
  OpenType_CFF/          OpenType with CFF/CFF2 table
    NotoSansSC_full.pdf  全量嵌入 (FontFile3 /Subtype /OpenType)
    NotoSansSC_subset.pdf 子集嵌入
  Type1/                 PostScript Type 1 (if found)
  Type1_CFF/             Type 1 in CFF format
  CIDFontType0_CFF/      CIDFont with CFF outlines
  CIDFontType2_TrueType/ CIDFont with TrueType outlines
```

## Repository Structure

```
podofo-font-tools/
├── CMakeLists.txt              # 主构建文件
├── vcpkg.json                  # vcpkg 依赖管理
├── src/font-classifier/        # 字体分类程序源码
│   ├── main.cpp
│   └── CMakeLists.txt
├── extern/podofo/              # PoDoFo 库完整源码 (bundled)
│   ├── src/podofo/main/        # PoDoFo 核心 API
│   ├── src/podofo/private/     # PoDoFo 内部实现
│   ├── src/podofo/auxiliary/   # PoDoFo 工具类型
│   └── 3rdparty/               # 附带的第三方库
├── scripts/                    # 构建和运行脚本
│   ├── setup.ps1
│   ├── build.ps1
│   └── run.ps1
├── BUILD.md                    # 详细构建文档
└── USAGE.md                    # 详细使用文档
```

## Dependencies

- **PoDoFo** 1.1.1+ - PDF manipulation library (源码在 `extern/podofo/`)
- **vcpkg** (https://github.com/microsoft/vcpkg) - C++ package manager
- **FreeType** - 字体光栅化 (通过 vcpkg 自动安装)
- **Fontconfig** - 跨平台字体搜索 (通过 vcpkg 自动安装)
- Windows SDK (for Win32 font enumeration)

所有 C++ 依赖通过 vcpkg 清单模式自动管理（定义在 `vcpkg.json`）。

## Quick Start

```bash
# 1. Install vcpkg if not already installed
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg && .\bootstrap-vcpkg.bat

# 2. Configure and build
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg_root>/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release --parallel 12

# 3. Run (process 10 TrueType fonts)
.\build\Release\podofo-font-classifier.exe --type truetype --max 10 output_dir
```

See [BUILD.md](BUILD.md) for detailed build instructions.
See [USAGE.md](USAGE.md) for detailed usage instructions.

## License

MIT (许可证覆盖自己编写的代码)
PoDoFo 库本身遵循 LGPL-2.0-or-later OR MPL-2.0 许可证
