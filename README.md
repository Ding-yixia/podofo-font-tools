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

## PoDoFo 源码说明

仓库包含完整 [PoDoFo](https://github.com/podofo/podofo) 源码（`extern/podofo/`，基于 1.1.0）。
**PoDoFo 库核心代码（`extern/podofo/src/podofo/`）未做任何修改**。

### 本项目的代码变更

#### 项目自有代码（`podofo-font-tools/`）

| 文件 | 说明 |
|------|------|
| [`src/font-classifier/main.cpp`](src/font-classifier/main.cpp) | 字体分类与 PDF 生成程序（24KB），完整的 PDF 规范分类逻辑 |
| [`src/font-classifier/CMakeLists.txt`](src/font-classifier/CMakeLists.txt) | 字体分类器构建配置 |
| [`CMakeLists.txt`](CMakeLists.txt) | 主构建系统，支持 MSVC `/MP` 并行编译 |
| [`vcpkg.json`](vcpkg.json) | vcpkg 依赖清单 |
| [`scripts/setup.ps1`](scripts/setup.ps1) | 环境配置脚本 |
| [`scripts/build.ps1`](scripts/build.ps1) | 构建脚本 |
| [`scripts/run.ps1`](scripts/run.ps1) | 运行脚本 |

#### PoDoFo Playground 修改（`extern/podofo/playground/`）

在 PoDoFo 的 playground 中新增了测试程序，用于开发阶段的验证：

| 文件 | 修改内容 |
|------|----------|
| [`extern/podofo/playground/main.cpp`](extern/podofo/playground/main.cpp) | **新增** — 字体分类演示程序（与 `src/font-classifier/main.cpp` 功能相同） |
| [`extern/podofo/playground/CMakeLists.txt`](extern/podofo/playground/CMakeLists.txt) | **修改** — 添加 `podofo-font-demo` 构建目标，启用 `/MP` 并行编译和 `PODOFO_STATIC` 定义 |

> 这些 playground 修改仅为开发和验证用途。项目的正式源码在 `src/font-classifier/` 中。

### 字体嵌入机制的关键发现

本项目在使用过程中发现了一个 PoDoFo 内部的重要行为（非 Bug，设计如此）：

**`EmbedFonts()` 只处理通过 `SearchFont()` 创建的字体**。

PoDoFo 的 `PdfFontManager` 内部维护两个缓存表：
- **`m_cachedQueries`** — 通过 `SearchFont(name)` 查找的字体（需要 fontconfig 或 Win32 GDI 支持），会被 `EmbedFonts()` 处理
- **`m_cachedPaths`** — 通过 `GetOrCreateFont(filepath)` 直接加载的字体，不被 `EmbedFonts()` 处理

字体嵌入的正确使用方式：
1. 安装 `podofo[fontmanager,fontconfig]`（通过 vcpkg）
2. 使用 `SearchFont()` 查找字体
3. 调用 `EmbedFonts()` 完成嵌入

详见 [BUILD.md → Technical Deep Dive](BUILD.md#technical-deep-dive-font-embedding-mechanism)。

## Dependencies

本项目通过 **vcpkg 清单模式** 自动管理所有 C++ 依赖（定义在 [`vcpkg.json`](vcpkg.json)）：

```json
{
  "dependencies": [
    { "name": "podofo", "features": ["fontmanager", "fontconfig"] }
  ]
}
```

| 依赖 | 版本 | 用途 | 管理方式 |
|------|------|------|----------|
| **PoDoFo** | 1.1.1+ | PDF 处理核心库 | vcpkg 自动安装 |
| **FreeType** | 2.14.3 | 字体光栅化 | vcpkg 自动安装 (PoDoFo 依赖) |
| **Fontconfig** | 2.17.1 | 跨平台字体搜索 | vcpkg 自动安装 (PoDoFo 依赖) |
| **OpenSSL** | 1.1.1 | PDF 加密支持 | vcpkg 自动安装 (PoDoFo 依赖) |
| **zlib** | 1.2.11 | 数据压缩 | vcpkg 自动安装 (PoDoFo 依赖) |
| **libxml2** | 2.14.5 | XML 解析 | vcpkg 自动安装 (PoDoFo 依赖) |
| **Windows SDK** | — | Win32 API 字体枚举 | 随 Visual Studio 自带 |

> **注意**：Fontconfig 特性是**必需**的。没有它，`SearchFont()` 无法定位系统字体，导致 `EmbedFonts()` 不会嵌入字体。详见 [Technical Deep Dive](BUILD.md#technical-deep-dive-font-embedding-mechanism)。

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
