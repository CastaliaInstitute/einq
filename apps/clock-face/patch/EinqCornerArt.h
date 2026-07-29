#pragma once

#include <GfxRenderer.h>

namespace EinqCornerArt {

// Select today's eligible seasonal theme. A theme may use one mirrored master,
// four independent ornaments, or a mixture of master plus corner overrides.
void drawFourCorners(GfxRenderer& renderer, int pageWidth, int pageHeight);
// Horizontal space occupied by today's ornaments, including visual breathing room.
void contentInsets(bool top, int& left, int& right);
const char* currentThemeId();

}  // namespace EinqCornerArt
