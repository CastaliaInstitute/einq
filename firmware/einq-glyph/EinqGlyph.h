#pragma once

#include <cstdint>

class GfxRenderer;

/** Monochrome inquiry glyphs (person / place / thing) for the Einq face. */
namespace EinqGlyph {

enum class Kind : uint8_t { Person = 0, Place = 1, Thing = 2 };

constexpr int kKindCount = 3;

Kind kindForDayOfYear(int yday);
Kind kindForDomain(const char* domain);
const char* kindLabel(Kind kind);

/** Draw a large centered glyph; returns the kind rendered. */
Kind draw(GfxRenderer& renderer, int centerX, int centerY, int size, Kind kind);

}  // namespace EinqGlyph
