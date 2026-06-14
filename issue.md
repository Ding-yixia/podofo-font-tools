# Issue: `PdfFontManager::EmbedFonts()` Only Iterates `m_cachedQueries` — `m_cachedPaths` Becomes Dangling

## Summary

`PdfFontManager::EmbedFonts()` iterates only one of the two font caches (`m_cachedQueries`), ignoring `m_cachedPaths`. After `EmbedFonts()` clears `m_cachedQueries`, the pointers in `m_cachedPaths` become **dangling** but are never cleared — leading to **use-after-free** if `GetOrCreateFont()` is called again with the same file path.

## Environment

- **PoDoFo version**: 1.1.0+ (commit `6018f85ce`)
- **Platform**: Windows 11 / MSVC 2022
- **Build config**: Static library, C++17

## Background: The Two Font Caches

`PdfFontManager` maintains three collections for font management:

| Collection | Key | Value | Purpose |
|---|---|---|---|
| `m_cachedQueries` | `Descriptor` (font name, encoding, style) | `std::vector<PdfFont*>` | Maps font names/style to fonts (from `SearchFont()` or fallback) |
| `m_cachedPaths` | `PathDescriptor` (file path, face index) | `PdfFont*` (raw pointer) | Fast re-lookup by file path (from `GetOrCreateFont()`) |
| `m_fonts` | `PdfReference` | `Storage` (with `unique_ptr<PdfFont>`) | **Ownership** — all fonts' memory is owned here |

## Root Cause

### `EmbedFonts()` Only Processes `m_cachedQueries`

File: [`src/podofo/main/PdfFontManager.cpp`](extern/podofo/src/podofo/main/PdfFontManager.cpp), lines ~406-423:

```cpp
void PdfFontManager::EmbedFonts()
{
    // Collect fonts to embed from cached queries
    set<PdfReference> fontToEmbeds;
    for (auto& pair : m_cachedQueries)          // <--- ONLY iterates m_cachedQueries
    {
        for (auto& font : pair.second)
            fontToEmbeds.insert(font->GetObject().GetIndirectReference());
    }

    for (auto& ref : fontToEmbeds)
        m_fonts[ref].Font->EmbedFont();

    m_cachedQueries.clear();                     // <--- clears m_cachedQueries
    // BUG: m_cachedPaths is NOT cleared here!   // <--- leaves dangling pointers
}
```

`m_cachedPaths` is **never iterated** by `EmbedFonts()`, and **never cleared** after embedding.

### `addImported()` Writes to Both Caches

Every font creation path ultimately calls `addImported()` (line 100-106):

```cpp
PdfFont* PdfFontManager::addImported(vector<PdfFont*>& fonts, unique_ptr<PdfFont>&& font)
{
    auto fontPtr = font.get();
    fonts.push_back(fontPtr);                       // writes to m_cachedQueries[...]
    m_fonts.insert({ fontPtr->GetObject().GetIndirectReference(),
                     Storage{ false, std::move(font) } });  // writes to m_fonts
    return fontPtr;
}
```

### `GetOrCreateFont()` Also Writes to `m_cachedPaths`

```cpp
PdfFont& PdfFontManager::GetOrCreateFont(...)
{
    auto found = m_cachedPaths.find(descriptor);
    if (found != m_cachedPaths.end())
        return *found->second;       // <--- returns cached pointer (DANGLING after EmbedFonts!)

    auto metrics = PdfFontMetrics::Create(fontPath, faceIndex);
    auto& ret = getOrCreateFontHashed(std::move(metrics), params);
    m_cachedPaths[descriptor] = &ret;   // <--- stores raw pointer in path cache
    return ret;
}
```

## The Dangling Pointer Problem (Use-After-Free)

### Timeline

```
1. GetOrCreateFont("arial.ttf", ...)     → added to BOTH caches + m_fonts
2. EmbedFonts()                           → iterates m_cachedQueries ✓
                                           → embeds via m_fonts[ref].Font ✓
                                           → clears m_cachedQueries ✓
                                           → m_cachedPaths NOT cleared  ← BUG!
3. PdfDocument destructor (or Clear())    → frees unique_ptr in m_fonts
                                           → m_cachedPaths pointers now DANGLING
4. GetOrCreateFont("arial.ttf", ...)      → finds old entry in m_cachedPaths
                                           → returns dangling pointer    ← CRASH
```

### Minimal Reproduction

```cpp
#include <podofo/podofo.h>
using namespace PoDoFo;

void TestDanglingPathCache()
{
    PdfMemDocument doc;

    // Step 1: Create a font from file path
    auto& font1 = doc.GetFonts().GetOrCreateFont(
        "C:\\Windows\\Fonts\\arial.ttf", 0,
        PdfFontCreateParams{});

    // Step 2: Embed it (works fine)
    doc.GetFonts().EmbedFonts();

    // Step 3: Clear the document (frees all font unique_ptrs)
    // This happens automatically when PdfMemDocument goes out of scope,
    // or can be triggered explicitly.

    // Step 4: Try to create the SAME font again — returns DANGLING pointer!
    auto& font2 = doc.GetFonts().GetOrCreateFont(
        "C:\\Windows\\Fonts\\arial.ttf", 0,
        PdfFontCreateParams{});

    // font2 is a dangling pointer — use-after-free!
    font2.GetMetrics().GetFontName();  // UNDEFINED BEHAVIOR
}
```

### Expected Behavior

`EmbedFonts()` should either:
1. **Also iterate** `m_cachedPaths` to embed fonts that might have been added by path only, OR
2. **Also clear** `m_cachedPaths` after embedding (alongside `m_cachedQueries.clear()`) to prevent dangling pointers, OR
3. The ownership model should be redesigned so that `m_cachedPaths` stores `weak_ptr` or uses a different mechanism to detect validity.

### Actual Behavior (Current)

```cpp
m_cachedQueries.clear();   // line 422
// m_cachedPaths NOT cleared — pointers become dangling after m_fonts cleanup
// m_cachedPaths still holds old PdfFont* addresses that are now freed
```

## Workaround

When working with fonts loaded by **file path**, always use `SearchFont()` instead of `GetOrCreateFont()`:

```cpp
// ✅ CORRECT: Use SearchFont() for fonts that need embedding
PdfFontSearchParams searchParams;
searchParams.MatchBehavior = PdfFontMatchBehaviorFlags::NormalizePattern;
PdfFont* font = doc.GetFonts().SearchFont("Arial", searchParams, createParams);
if (font != nullptr) {
    // Draw something...
    doc.GetFonts().EmbedFonts();  // ✅ This font WILL be embedded
}

// ❌ PROBLEMATIC: GetOrCreateFont() by file path
auto& font = doc.GetFonts().GetOrCreateFont("C:\\Windows\\Fonts\\arial.ttf", 0, params);
doc.GetFonts().EmbedFonts();  // ⚠️ This font happens to work (addImported writes to
                               //    both caches), but the dangling path pointer remains
```

Also, avoid re-creating fonts by the same file path after `EmbedFonts()`:

```cpp
// After EmbedFonts(), do NOT call GetOrCreateFont() with the same path
doc.GetFonts().EmbedFonts();
// ...
auto& font = doc.GetFonts().GetOrCreateFont("arial.ttf", 0, params);  // DANGER
```

## Is This a Bug?

**Yes, this is a bug** — but with nuance:

| Aspect | Assessment |
|---|---|
| **Dangling pointer in `m_cachedPaths`** | **Bug** — use-after-free risk. `m_cachedPaths` should be cleared after `EmbedFonts()`, just like `m_cachedQueries` is. |
| **`EmbedFonts()` only iterates one cache** | **Design choice** — but undocumented. Since `addImported()` writes to both caches, fonts created by `GetOrCreateFont()` DO get embedded (via `m_cachedQueries`). The design works in the common case but is fragile. |
| **Missing `m_cachedPaths.clear()`** | **Bug** — the TODO comment at line 423 (`// TODO: Don't clean standard14 and full embedded fonts`) acknowledges incomplete cleanup logic. |

The code already has a **TODO comment** acknowledging incomplete cache management:

```cpp
// TODO: Don't clean standard14 and full embedded fonts
m_cachedQueries.clear();
```

A simple fix would be to add `m_cachedPaths.clear()` right after `m_cachedQueries.clear()` in `EmbedFonts()`:

```cpp
// After embedding
m_cachedQueries.clear();
m_cachedPaths.clear();  // <--- FIX: prevent dangling pointers
```

Or, more thoroughly, the `m_cachedPaths` cache could store `std::weak_ptr` instead of raw pointers, so callers can detect when a font has been evicted.

## Additional Code References

- `PdfFontManager.h`: Cache declarations (lines 183-220) — [`extern/podofo/src/podofo/main/PdfFontManager.h`](extern/podofo/src/podofo/main/PdfFontManager.h)
- `PdfFontManager.cpp::EmbedFonts()` (lines 406-423) — [`extern/podofo/src/podofo/main/PdfFontManager.cpp`](extern/podofo/src/podofo/main/PdfFontManager.cpp)
- `PdfFontManager.cpp::addImported()` (lines 100-106) — same file
- `PdfFontManager.cpp::GetOrCreateFont()` (lines 206-268) — same file
- `PdfFontManager.cpp::SearchFont()` (lines 166-183) — same file
- `PdfFontCreateFlags` enum — [`extern/podofo/src/podofo/main/PdfDeclarations.h`](extern/podofo/src/podofo/main/PdfDeclarations.h)
