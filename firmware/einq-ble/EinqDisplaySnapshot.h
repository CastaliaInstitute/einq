#pragma once

#include <cstddef>

/** JSON-serializable view of what the Einq face is showing (BLE read / notify). */
struct EinqDisplaySnapshot {
  static constexpr size_t kModeLen = 16;
  static constexpr size_t kTitleLen = 32;
  static constexpr size_t kLineLen = 96;
  static constexpr size_t kTimeLen = 8;
  static constexpr size_t kDayLen = 32;
  static constexpr size_t kDateLen = 16;
  static constexpr size_t kGlyphLen = 16;
  static constexpr size_t kThemeLen = 32;
  static constexpr size_t kSsidLen = 64;
  static constexpr size_t kPasswordLen = 96;

  char mode[kModeLen] = "clock";
  char title[kTitleLen] = "Einq";
  char line1[kLineLen] = {};
  char line2[kLineLen] = {};
  char line3[kLineLen] = {};
  char time[kTimeLen] = {};
  char day[kDayLen] = {};
  char date[kDateLen] = {};
  char glyph[kGlyphLen] = {};
  char theme[kThemeLen] = {};
};

/** Parsed display command from BLE write (show message or return to clock). */
struct EinqDisplayCommand {
  bool valid = false;
  char mode[EinqDisplaySnapshot::kModeLen] = "clock";
  char title[EinqDisplaySnapshot::kTitleLen] = "Einq";
  char line1[EinqDisplaySnapshot::kLineLen] = {};
  char line2[EinqDisplaySnapshot::kLineLen] = {};
  char line3[EinqDisplaySnapshot::kLineLen] = {};
  char ssid[EinqDisplaySnapshot::kSsidLen] = {};
  char password[EinqDisplaySnapshot::kPasswordLen] = {};
};
