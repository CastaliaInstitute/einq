#include "EinqGlyph.h"

#include <GfxRenderer.h>

#include <cstring>

namespace EinqGlyph {

namespace {

void drawPerson(GfxRenderer& renderer, int cx, int cy, int size) {
  const int headR = size / 5;
  const int bodyW = size / 2;
  const int bodyH = size / 3;
  renderer.drawArc(headR, cx, cy - size / 6, 1, 1, 2, true);
  renderer.drawRoundedRect(cx - bodyW / 2, cy - size / 12, bodyW, bodyH, 2, size / 10, true);
}

void drawPlace(GfxRenderer& renderer, int cx, int cy, int size) {
  const int pinH = size / 2;
  const int pinW = size / 3;
  const int topY = cy - pinH / 2;
  renderer.drawArc(pinW / 2, cx, topY + pinW / 4, 1, 1, 2, true);
  const int tipY = topY + pinH;
  const int leftX = cx - pinW / 2;
  const int rightX = cx + pinW / 2;
  const int midY = topY + pinW / 2;
  renderer.drawLine(leftX, midY, cx, tipY, 2, true);
  renderer.drawLine(rightX, midY, cx, tipY, 2, true);
  renderer.drawLine(leftX, midY, rightX, midY, 2, true);
}

void drawThing(GfxRenderer& renderer, int cx, int cy, int size) {
  const int bulbR = size / 5;
  const int stemW = size / 8;
  const int stemH = size / 5;
  renderer.drawArc(bulbR, cx, cy - size / 8, 1, 1, 2, true);
  renderer.fillRect(cx - stemW / 2, cy, stemW, stemH, true);
  renderer.drawLine(cx - bulbR - 4, cy - size / 8, cx - bulbR - size / 8, cy - size / 4, 2, true);
  renderer.drawLine(cx + bulbR + 4, cy - size / 8, cx + bulbR + size / 8, cy - size / 4, 2, true);
}

}  // namespace

Kind kindForDayOfYear(int yday) {
  const int normalized = ((yday % kKindCount) + kKindCount) % kKindCount;
  return static_cast<Kind>(normalized);
}

Kind kindForDomain(const char* domain) {
  if (domain == nullptr || domain[0] == '\0') {
    return Kind::Thing;
  }
  if (strcmp(domain, "people") == 0) {
    return Kind::Person;
  }
  if (strcmp(domain, "locations") == 0) {
    return Kind::Place;
  }
  return Kind::Thing;
}

const char* kindLabel(Kind kind) {
  switch (kind) {
    case Kind::Person:
      return "person";
    case Kind::Place:
      return "place";
    case Kind::Thing:
      return "thing";
  }
  return "thing";
}

Kind draw(GfxRenderer& renderer, int centerX, int centerY, int size, Kind kind) {
  switch (kind) {
    case Kind::Person:
      drawPerson(renderer, centerX, centerY, size);
      break;
    case Kind::Place:
      drawPlace(renderer, centerX, centerY, size);
      break;
    case Kind::Thing:
      drawThing(renderer, centerX, centerY, size);
      break;
  }
  return kind;
}

}  // namespace EinqGlyph
