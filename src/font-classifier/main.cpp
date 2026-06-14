// SPDX-License-Identifier: MIT-0
//
// PoDoFo PDF Font Classification & Embedding Demo
//
// Based on PDF Specification (ISO 32000-1:2008 / ISO 32000-2:2020)
// Font type classification per PDF spec Section 9.6 "Font Types":
//
// +------------------+---------------------+-------------------------------+------------+
// | PDF Subtype      | Outline Format      | Embedded Font Key             | Max Glyphs |
// +------------------+---------------------+-------------------------------+------------+
// | Type 1           | PostScript (cubic)  | /FontFile (Length1-3)         | 256        |
// | MMType1          | PostScript (cubic)  | /FontFile (Length1-3)         | 256        |
// | TrueType         | Quadratic splines   | /FontFile2 (Length1)          | 256        |
// | Type 3           | PDF content streams | N/A (inline)                  | 256        |
// | Type 0 (CIDFont) | Composite           | /FontFile2 or /FontFile3      | 65535      |
// |   CIDFontType0   | CFF (Type1/Cubic)   | /FontFile3 /Subtype CIDFont0C | 65535      |
// |   CIDFontType2   | TrueType            | /FontFile2                    | 65535      |
// +------------------+---------------------+-------------------------------+------------+
//
// Embedded Font File Types (PDF Spec Table 126):
//   FontFile  (Length1, Length2, Length3) -> Type 1 PFB/PFA
//   FontFile2 (Length1)                   -> TrueType / OpenType with "glyf"
//   FontFile3 /Subtype /Type1C           -> Type 1 in CFF format
//   FontFile3 /Subtype /CIDFontType0C    -> CIDFont with CFF
//   FontFile3 /Subtype /OpenType         -> OpenType with CFF/CFF2
//
// Output structure:
//   <output_dir>/
//     TrueType/          <fontname>_full.pdf, <fontname>_subset.pdf
//     OpenType_CFF/      <fontname>_full.pdf, <fontname>_subset.pdf
//     Type1/             ...
//     Type1_CFF/         ...
//     CIDFontType0_CFF/  ...
//     CIDFontType2_TrueType/ ...

#include <podofo/podofo.h>

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <map>
#include <set>
#include <cstring>
#include <cctype>
#include <filesystem>
#include <fstream>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif

using namespace std;
using namespace PoDoFo;
namespace fs = std::filesystem;

// ============================================================
// PDF Spec Font Type Classification (per ISO 32000)
// ============================================================
enum class PdfSpecFontCategory
{
    Unknown,
    Type1,          // PostScript Type 1 (PFB/PFA) - /Subtype: Type1
    Type1CFF,       // Type 1 in CFF format - /FontFile3 /Subtype /Type1C
    TrueType,       // TrueType with "glyf" table - /Subtype: TrueType
    OpenTypeCFF,    // OpenType with CFF/CFF2 table - /FontFile3 /Subtype /OpenType
    CIDType0,       // CIDFont with CFF - /Subtype: CIDFontType0
    CIDType2,       // CIDFont with TrueType - /Subtype: CIDFontType2
    Type3,          // PDF content stream glyphs - /Subtype: Type3
};

const char* GetPdfSpecFontCategoryName(PdfSpecFontCategory cat)
{
    switch (cat)
    {
    case PdfSpecFontCategory::Type1:       return "Type1";
    case PdfSpecFontCategory::Type1CFF:    return "Type1_CFF";
    case PdfSpecFontCategory::TrueType:    return "TrueType";
    case PdfSpecFontCategory::OpenTypeCFF: return "OpenType_CFF";
    case PdfSpecFontCategory::CIDType0:    return "CIDFontType0_CFF";
    case PdfSpecFontCategory::CIDType2:    return "CIDFontType2_TrueType";
    case PdfSpecFontCategory::Type3:       return "Type3";
    default:                               return "Unknown";
    }
}

const char* GetPdfSpecFontCategoryDescription(PdfSpecFontCategory cat)
{
    switch (cat)
    {
    case PdfSpecFontCategory::Type1:
        return "PostScript Type 1 (PFB/PFA, cubic Bezier, max 256 glyphs, /FontFile)";
    case PdfSpecFontCategory::Type1CFF:
        return "Type 1 in Compact Font Format (/FontFile3 /Subtype /Type1C)";
    case PdfSpecFontCategory::TrueType:
        return "TrueType/OpenType with glyf table (quadratic B-splines, /FontFile2)";
    case PdfSpecFontCategory::OpenTypeCFF:
        return "OpenType with CFF/CFF2 table (/FontFile3 /Subtype /OpenType)";
    case PdfSpecFontCategory::CIDType0:
        return "CIDFont Type0 - CFF outlines (/FontFile3 /Subtype /CIDFontType0C)";
    case PdfSpecFontCategory::CIDType2:
        return "CIDFont Type2 - TrueType outlines (/FontFile2, Unicode via /ToUnicode)";
    case PdfSpecFontCategory::Type3:
        return "Type 3 - glyphs as PDF content stream (no embedding needed)";
    default:
        return "Unknown font type";
    }
}

// Map PoDoFo's PdfFontFileType to PDF Spec category
PdfSpecFontCategory MapToPdfSpecCategory(PdfFontFileType fileType)
{
    switch (fileType)
    {
    case PdfFontFileType::Type1:       return PdfSpecFontCategory::Type1;
    case PdfFontFileType::Type1CFF:    return PdfSpecFontCategory::Type1CFF;
    case PdfFontFileType::CIDKeyedCFF: return PdfSpecFontCategory::CIDType0;
    case PdfFontFileType::TrueType:    return PdfSpecFontCategory::TrueType;
    case PdfFontFileType::OpenTypeCFF: return PdfSpecFontCategory::OpenTypeCFF;
    case PdfFontFileType::Type3:       return PdfSpecFontCategory::Type3;
    default:                           return PdfSpecFontCategory::Unknown;
    }
}

// ============================================================
// Font Information
// ============================================================
struct FontInfo
{
    string FilePath;
    string Extension;
    string FontName;
    string FamilyName;
    PdfFontFileType FileType;
    PdfSpecFontCategory SpecCategory;
    unsigned FaceIndex;
    bool LoadSuccess;
    string ErrorMessage;

    FontInfo()
        : FileType(PdfFontFileType::Unknown),
          SpecCategory(PdfSpecFontCategory::Unknown),
          FaceIndex(0), LoadSuccess(false) {}
};

// ============================================================
// Windows Font Scanner
// ============================================================
class WindowsFontScanner
{
public:
    static vector<string> ScanFontFiles(const string& directory)
    {
        vector<string> files;
        string searchPath = directory + "\\*";

        WIN32_FIND_DATAW findData;
        HANDLE hFind = FindFirstFileW(Utf8ToUtf16(searchPath).c_str(), &findData);
        if (hFind == INVALID_HANDLE_VALUE)
            return files;

        set<string, less<>> validExts = { ".ttf", ".ttc", ".otf", ".pfb", ".pfa" };

        do
        {
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;

            string fn = Utf16ToUtf8(findData.cFileName);
            size_t dot = fn.rfind('.');
            if (dot == string::npos) continue;

            string ext = fn.substr(dot);
            string lower;
            for (char c : ext) lower += (char)tolower((unsigned char)c);
            if (validExts.find(lower) != validExts.end())
                files.push_back(directory + "\\" + fn);
        } while (FindNextFileW(hFind, &findData) != 0);

        FindClose(hFind);
        return files;
    }

    static FontInfo AnalyzeFontFile(const string& filePath)
    {
        FontInfo info;
        info.FilePath = filePath;

        size_t dot = filePath.rfind('.');
        if (dot != string::npos)
        {
            info.Extension = filePath.substr(dot);
            string lower;
            for (char c : info.Extension) lower += (char)tolower((unsigned char)c);

            if (lower == ".ttc")
            {
                unsigned fi = 0;
                bool anyLoaded = false;
                while (true)
                {
                    try
                    {
                        auto m = PdfFontMetrics::Create(filePath, fi);
                        if (!m) break;
                        if (!anyLoaded)
                        {
                            info.FontName = string(m->GetFontName());
                            info.FamilyName = string(m->GetFontFamilyName());
                            info.FileType = m->GetFontFileType();
                            info.SpecCategory = MapToPdfSpecCategory(info.FileType);
                            info.FaceIndex = fi;
                            info.LoadSuccess = true;
                            anyLoaded = true;
                        }
                        fi++;
                    }
                    catch (PdfError&) { break; }
                }
                if (!anyLoaded)
                    info.ErrorMessage = "No face loaded from TTC";
            }
            else
            {
                try
                {
                    auto m = PdfFontMetrics::Create(filePath, 0);
                    if (m)
                    {
                        info.FontName = string(m->GetFontName());
                        info.FamilyName = string(m->GetFontFamilyName());
                        info.FileType = m->GetFontFileType();
                        info.SpecCategory = MapToPdfSpecCategory(info.FileType);
                        info.LoadSuccess = true;
                    }
                    else
                        info.ErrorMessage = "Metrics creation returned null";
                }
                catch (PdfError& e)
                {
                    info.ErrorMessage = "PoDoFo error: " + to_string((int)e.GetCode());
                }
                catch (exception& e)
                {
                    info.ErrorMessage = e.what();
                }
            }
        }
        return info;
    }

    static vector<FontInfo> ScanAndAnalyze(const string& dir = "C:\\Windows\\Fonts")
    {
        auto files = ScanFontFiles(dir);
        cout << "Found " << files.size() << " font files in '" << dir << "'" << endl;
        cout << "Analyzing..." << endl;

        vector<FontInfo> results;
        results.reserve(files.size());
        for (size_t i = 0; i < files.size(); i++)
        {
            if ((i + 1) % 50 == 0)
                cout << "  Progress: " << (i + 1) << "/" << files.size() << endl;
            results.push_back(AnalyzeFontFile(files[i]));
        }
        return results;
    }

private:
    static wstring Utf8ToUtf16(const string& utf8)
    {
        if (utf8.empty()) return wstring();
        int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
        if (len <= 0) return wstring();
        wstring r((size_t)len, L'\\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), &r[0], len);
        return r;
    }
    static string Utf16ToUtf8(const wstring& utf16)
    {
        if (utf16.empty()) return string();
        int len = WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), (int)utf16.size(), nullptr, 0, nullptr, nullptr);
        if (len <= 0) return string();
        string r((size_t)len, '\\0');
        WideCharToMultiByte(CP_UTF8, 0, utf16.c_str(), (int)utf16.size(), &r[0], len, nullptr, nullptr);
        return r;
    }
};

// ============================================================
// PDF Generator: one font per file, two variants (full/subset)
// ============================================================
class FontPdfGenerator
{
public:
    static bool Generate(const string& outputDir, const FontInfo& font)
    {
        string catDir = outputDir + "\\" + GetPdfSpecFontCategoryName(font.SpecCategory);

        string safeName = SanitizeFileName(font.FontName.empty() ?
            GetFileName(font.FilePath) : font.FontName);

        try
        {
            fs::create_directories(catDir);
        }
        catch (...)
        {
            cerr << "  Cannot create directory: " << catDir << endl;
            return false;
        }

        cout << "  [" << GetPdfSpecFontCategoryName(font.SpecCategory) << "] "
             << font.FontName << endl;

        // Full-embed version (DontSubset)
        string fullPath = catDir + "\\" + safeName + "_full.pdf";
        bool fullOK = false;
        try
        {
            fullOK = GenerateSinglePdf(fullPath, font, PdfFontCreateFlags::DontSubset);
            cout << "    Full embed:  " << (fullOK ? "OK" : "FAILED") << endl;
        }
        catch (exception& e)
        {
            cout << "    Full embed:  FAILED (" << e.what() << ")" << endl;
        }

        // Subset-embed version (default = subset enabled)
        string subsetPath = catDir + "\\" + safeName + "_subset.pdf";
        bool subsetOK = false;
        try
        {
            subsetOK = GenerateSinglePdf(subsetPath, font, PdfFontCreateFlags::None);
            cout << "    Subset embed: " << (subsetOK ? "OK" : "FAILED") << endl;
        }
        catch (exception& e)
        {
            cout << "    Subset embed: FAILED (" << e.what() << ")" << endl;
        }

        return fullOK || subsetOK;
    }

private:
    static bool GenerateSinglePdf(const string& outputPath, const FontInfo& font,
                                  PdfFontCreateFlags embedFlags)
    {
        PdfMemDocument doc;

        PdfFontCreateParams createParams;
        createParams.Flags = embedFlags;

        PdfFont* pdfFont = nullptr;
        string fontName = font.FontName.empty() ? GetFileName(font.FilePath) : font.FontName;

        // ---- Font Loading ----
        // Strategy 1: SearchFont via name (enables proper font embedding)
        {
            PdfFontSearchParams sp;
            sp.MatchBehavior = PdfFontMatchBehaviorFlags::NormalizePattern;
            try { pdfFont = doc.GetFonts().SearchFont(fontName, sp, createParams); }
            catch (...) { pdfFont = nullptr; }
        }

        // Strategy 2: Fallback to direct file path
        if (pdfFont == nullptr)
        {
            try
            {
                pdfFont = &doc.GetFonts().GetOrCreateFont(
                    font.FilePath, font.FaceIndex, createParams);
            }
            catch (PdfError& e)
            {
                // Strategy 3: Try with PreferNonCID for variable/CFF fonts
                PdfFontCreateParams fallbackParams;
                fallbackParams.Flags = static_cast<PdfFontCreateFlags>(
                    static_cast<int>(embedFlags) |
                    static_cast<int>(PdfFontCreateFlags::PreferNonCID));
                try
                {
                    pdfFont = &doc.GetFonts().GetOrCreateFont(
                        font.FilePath, font.FaceIndex, fallbackParams);
                }
                catch (...)
                {
                    cerr << "    Load error: " << (int)e.GetCode() << endl;
                    return false;
                }
            }
            catch (exception& e)
            {
                cerr << "    Load error: " << e.what() << endl;
                return false;
            }
        }

        // ---- PDF Page Setup ----
        auto& page = doc.GetPages().CreatePage(PdfPageSize::A4);
        PdfPainter painter;
        painter.SetCanvas(page);

        const double margin = 56.69;
        double y = page.GetRect().Height - margin;
        auto& metrics = pdfFont->GetMetrics();
        double tx = margin;

        // ---- Sample Text Drawing (resilient to encoding errors) ----
        bool sampleTextDrawn = false;
        try
        {
            painter.TextState.SetFont(*pdfFont, 12.0);

            string line;
            line = "Uppercase: " + BuildSafeString(*pdfFont, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
            painter.DrawText(line, tx, y); y -= 18;

            line = "Lowercase: " + BuildSafeString(*pdfFont, "abcdefghijklmnopqrstuvwxyz");
            painter.DrawText(line, tx, y); y -= 18;

            line = "Symbols:   " + BuildSafeString(*pdfFont, "0123456789!@#$%&*()[]{}");
            painter.DrawText(line, tx, y); y -= 22;

            sampleTextDrawn = true;
        }
        catch (PdfError&)
        {
            // Sample text not supported by this font (e.g. symbol/variable fonts)
            // Proceed with metadata only
        }
        catch (...) { }

        if (!sampleTextDrawn)
            y -= 22;

        // ---- Font Metadata Label ----
        PdfFont* labelFont = nullptr;
        try
        {
            labelFont = &doc.GetFonts().GetStandard14Font(PdfStandard14FontType::Helvetica);
            painter.TextState.SetFont(*labelFont, 9.0);
        }
        catch (...) { }

        // Draw metadata even if sample text failed
        string embedStr = (embedFlags & PdfFontCreateFlags::DontSubset) == PdfFontCreateFlags::DontSubset
            ? "Full Embedding" : "Subset Embedding";

        if (labelFont && !sampleTextDrawn)
        {
            painter.DrawText("[Sample text not supported by this font type]", tx, y);
            y -= 14;
        }

        y -= 14;
        if (labelFont)
        {
            painter.DrawText(string("Font Name: ") + string(metrics.GetFontName()), tx, y); y -= 14;
            painter.DrawText(string("Family:    ") + string(metrics.GetFontFamilyName()), tx, y); y -= 14;
            painter.DrawText(string("Type:      ") + GetPdfSpecFontCategoryName(font.SpecCategory), tx, y); y -= 14;
            painter.DrawText(string("Embedding: ") + embedStr, tx, y); y -= 14;
            painter.DrawText(string("File:      ") + font.Extension, tx, y); y -= 22;

            painter.TextState.SetFont(*labelFont, 8.0);
            painter.DrawText("--- PDF Spec Classification ---", tx, y); y -= 14;
            painter.DrawText(GetPdfSpecFontCategoryDescription(font.SpecCategory), tx, y); y -= 14;

            string embedKey = GetEmbeddingKey(font.FileType);
            painter.DrawText(string("Embedding key: ") + embedKey, tx, y); y -= 14;

            string ffType = GetFontFileTypeInfo(font.FileType);
            if (!ffType.empty())
                painter.DrawText(ffType, tx, y);
        }

        painter.FinishDrawing();

        // ---- Embed Font & Save ----
        doc.GetFonts().EmbedFonts();
        doc.Save(outputPath);

        return true;
    }

    static string BuildSafeString(PdfFont& font, const string& text)
    {
        string result;
        for (size_t i = 0; i < text.size(); )
        {
            if ((unsigned char)text[i] < 0x80)
            {
                char32_t cp = (unsigned char)text[i++];
                unsigned gid;
                if (font.TryGetGID(cp, PdfGlyphAccess::FontProgram, gid))
                    result += (char)cp;
            }
            else
            {
                result += text[i++];
            }
        }
        return result;
    }

    static string SanitizeFileName(const string& name)
    {
        string r;
        r.reserve(name.size());
        for (char c : name)
        {
            if (c == '/' || c == '\\' || c == ':' || c == '*' ||
                c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
                r += '_';
            else
                r += c;
        }
        return r;
    }

    static string GetFileName(const string& path)
    {
        size_t s = path.rfind('\\');
        size_t d = path.rfind('.');
        string fn = (s != string::npos) ? path.substr(s + 1, d - s - 1) : path.substr(0, d);
        return fn;
    }

    static string GetEmbeddingKey(PdfFontFileType ft)
    {
        switch (ft)
        {
        case PdfFontFileType::Type1:       return "/FontFile (Length1-3)";
        case PdfFontFileType::Type1CFF:    return "/FontFile3 /Subtype /Type1C";
        case PdfFontFileType::CIDKeyedCFF: return "/FontFile3 /Subtype /CIDFontType0C";
        case PdfFontFileType::TrueType:    return "/FontFile2 (Length1)";
        case PdfFontFileType::OpenTypeCFF: return "/FontFile3 /Subtype /OpenType";
        default:                           return "N/A";
        }
    }

    static string GetFontFileTypeInfo(PdfFontFileType ft)
    {
        switch (ft)
        {
        case PdfFontFileType::Type1:
            return "Per PDF Spec Table 126: Type 1 program in /FontFile";
        case PdfFontFileType::TrueType:
            return "Per PDF Spec Table 126: TrueType program in /FontFile2";
        case PdfFontFileType::OpenTypeCFF:
            return "Per PDF Spec Table 126: OpenType CFF in /FontFile3 /Subtype /OpenType";
        default:
            return "";
        }
    }
};

// ============================================================
// Main
// ============================================================
void PrintUsage(const char* prog)
{
    cout << "PoDoFo PDF Font Spec Classification Demo" << endl;
    cout << "Usage: " << prog << " [options] <output_dir>" << endl;
    cout << "  --dir <path>    Font directory (default: C:\\Windows\\Fonts)" << endl;
    cout << "  --max <N>       Max fonts to process (default: all)" << endl;
    cout << "  --type <type>   Filter: truetype, opentype, type1, all" << endl;
    cout << "  --help          Show help" << endl;
    cout << endl;
    cout << "Output:" << endl;
    cout << "  <output_dir>/TrueType/<font>_full.pdf   (full embedding)" << endl;
    cout << "  <output_dir>/TrueType/<font>_subset.pdf (subset embedding)" << endl;
    cout << endl;
}

int main(int argc, char* argv[])
{
    string outputDir;
    string fontDir = "C:\\Windows\\Fonts";
    string filterType = "all";
    unsigned maxFonts = UINT_MAX;

    for (int i = 1; i < argc; i++)
    {
        string a = argv[i];
        if (a == "--help") { PrintUsage(argv[0]); return 0; }
        else if (a == "--dir" && i + 1 < argc) fontDir = argv[++i];
        else if (a == "--max" && i + 1 < argc) maxFonts = (unsigned)max(1, atoi(argv[++i]));
        else if (a == "--type" && i + 1 < argc)
        {
            filterType = argv[++i];
            for (auto& c : filterType) c = (char)tolower((unsigned char)c);
        }
        else outputDir = a;
    }

    if (outputDir.empty())
    {
        cerr << "ERROR: No output directory specified." << endl;
        PrintUsage(argv[0]);
        return 1;
    }

    try
    {
        auto fonts = WindowsFontScanner::ScanAndAnalyze(fontDir);
        if (fonts.empty())
        {
            cerr << "No fonts found." << endl;
            return 1;
        }

        cout << "\n=== PDF SPEC FONT TYPE CLASSIFICATION REPORT ===" << endl;
        map<PdfSpecFontCategory, unsigned> counts;
        for (auto& f : fonts)
            if (f.LoadSuccess)
                counts[f.SpecCategory]++;

        for (auto& [cat, count] : counts)
        {
            cout << "  " << GetPdfSpecFontCategoryName(cat) << ": "
                 << count << " fonts" << endl;
            cout << "    " << GetPdfSpecFontCategoryDescription(cat) << endl;
        }

        vector<FontInfo> selected;
        for (auto& f : fonts)
        {
            if (!f.LoadSuccess) continue;
            if (filterType == "truetype" && f.SpecCategory != PdfSpecFontCategory::TrueType) continue;
            if (filterType == "opentype" && f.SpecCategory != PdfSpecFontCategory::OpenTypeCFF) continue;
            if (filterType == "type1" && f.SpecCategory != PdfSpecFontCategory::Type1
                && f.SpecCategory != PdfSpecFontCategory::Type1CFF) continue;
            selected.push_back(f);
        }

        if (selected.empty())
        {
            cerr << "No fonts match the filter." << endl;
            return 1;
        }

        cout << "\nProcessing " << min((size_t)maxFonts, selected.size())
             << " fonts..." << endl;

        unsigned ok = 0, fail = 0;
        unsigned processed = 0;
        for (auto& font : selected)
        {
            if (processed >= maxFonts) break;
            if (FontPdfGenerator::Generate(outputDir, font))
                ok++;
            else
                fail++;
            processed++;
        }

        cout << "\n=== DONE ===" << endl;
        cout << "  Processed: " << processed << endl;
        cout << "  Success:   " << ok << endl;
        cout << "  Failed:    " << fail << endl;
        cout << "  Output:    " << outputDir << endl;
    }
    catch (PdfError& err)
    {
        cerr << "\nFATAL PoDoFo Error: " << (int)err.GetCode() << endl;
        err.PrintErrorMsg();
        return (int)err.GetCode();
    }
    catch (exception& e)
    {
        cerr << "\nFATAL Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
