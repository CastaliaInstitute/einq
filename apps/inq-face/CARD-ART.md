# Inq card art (v0)

## Rule

Use **monochrome Noto Emoji** glyphs only — one emoji per card, representing the inquiry subject (person, place, or thing).

- **Format:** 1-bit friendly (black ink on paper white)
- **Source:** [Noto Emoji](https://github.com/googlefonts/noto-emoji) (monochrome outlines; OFL license)
- **Not in v0:** photos, faculty busts, generated art, color emoji

## Layout (sketch)

```
┌─────────────────────┐
│                     │
│    [emoji large]    │  ← Noto Emoji glyph, centered
│                     │
│    Card title       │
│    One or two lines │
│                     │
└─────────────────────┘
```

## Mapping (placeholder)

Until content picks explicit codepoints, default by card `kind`:

| kind   | Example glyph (TBD) |
|--------|---------------------|
| person | e.g. U+1F464 bust   |
| place  | e.g. U+1F4CD pin  |
| thing  | e.g. U+1F4A1 bulb |

Per-card `emoji` field in JSON can override later.

## Firmware notes (later)

- Embed or load a Noto Emoji monochrome subset (SD or flash)
- Rasterize glyph at target size; optional partial refresh if only text changes
- CrossPoint’s text stack uses Noto *text* fonts; emoji is a separate raster path
