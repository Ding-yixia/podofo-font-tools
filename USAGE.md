# Usage Instructions

## Running the Program

```powershell
podofo-font-classifier.exe [options] <output_dir>
```

## Options

| Option | Description | Default |
|--------|-------------|---------|
| `--type <type>` | Filter by font type | `all` |
| `--max <N>` | Maximum fonts to process | all |
| `--dir <path>` | Font directory | `C:\Windows\Fonts` |
| `--help` | Show help | - |

### Font Type Filters

| Filter | PDF Specification Type | Description |
|--------|----------------------|-------------|
| `truetype` | TrueType | TrueType/OpenType with "glyf" table |
| `opentype` | OpenType CFF | OpenType with CFF/CFF2 table |
| `type1` | Type 1 / Type 1 CFF | PostScript Type 1 fonts |
| `all` | All types | Process all found fonts |

## Examples

### Basic usage - process all font types (first 5 fonts)

```powershell
.\podofo-font-classifier.exe --max 5 ..\output
```

### Process only TrueType fonts (first 10)

```powershell
.\podofo-font-classifier.exe --type truetype --max 10 ..\output
```

### Process only OpenType CFF fonts

```powershell
.\podofo-font-classifier.exe --type opentype ..\output
```

### Process all fonts from a custom directory

```powershell
.\podofo-font-classifier.exe --dir "D:\MyFonts" --max 50 ..\output
```

## Output Structure

```
output/
  TrueType/                    # PDF Subtype: /TrueType
    FontName1_full.pdf          # Full embedding (DontSubset)
    FontName1_subset.pdf        # Subset embedding (default)
    FontName2_full.pdf
    FontName2_subset.pdf
  OpenType_CFF/                # PDF Subtype: Type0 with CIDFontType0
    FontName3_full.pdf
    FontName3_subset.pdf
  Type1/                       # PDF Subtype: /Type1
  Type1_CFF/                   # FontFile3 /Subtype /Type1C
  CIDFontType0_CFF/            # FontFile3 /Subtype /CIDFontType0C
  CIDFontType2_TrueType/       # FontFile2 (CID keyed)
```

## File Size Comparison

Example with NotoSansSC-Regular (CJK font, ~30,000 glyphs):

```
Full embedding (full.pdf):  7.0 MB  - All CJK glyphs included
Subset embedding (subset.pdf): 15 KB - Only used glyphs (ABCDEFG...)
Ratio: 468x reduction
```

Example with Aharoni-Bold (Hebrew font):

```
Full embedding (full.pdf):  54 KB
Subset embedding (subset.pdf): 20 KB
```

## PDF Specification Reference

Each generated PDF includes metadata showing:

- **Font Name**: PostScript name of the font
- **Family**: Font family name
- **Type**: PDF spec classification (TrueType, OpenType CFF, etc.)
- **Embedding**: Full Embedding or Subset Embedding
- **Embedding Key**: The PDF spec key used (`/FontFile2`, `/FontFile3 /Subtype /OpenType`, etc.)
- **Spec Reference**: Reference to PDF Spec Table 126
