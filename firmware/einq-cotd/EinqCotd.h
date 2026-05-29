#pragma once

#include <cstddef>

/** Card of the Day from cards.castalia.institute (7-year arc / Candlemas seasons). */
struct EinqCotdCard {
  static constexpr size_t kDateLen = 16;
  static constexpr size_t kTitleLen = 48;
  static constexpr size_t kTopicLen = 48;
  static constexpr size_t kSlugLen = 48;
  static constexpr size_t kDomainLen = 32;

  char date[kDateLen] = {};
  char title[kTitleLen] = {};
  char topic[kTopicLen] = {};
  char slug[kSlugLen] = {};
  char domain[kDomainLen] = {};
  int season = 0;
  int year = 0;
  bool valid = false;
};

namespace EinqCotd {

/** Load cached JSON from SD (/.einq/cotd/YYYY-MM-DD.json). */
bool loadCached(const char* dateYmd, EinqCotdCard& out);

/** Fetch JSON over WiFi and cache to SD. Requires WL_CONNECTED. */
bool fetchAndCache(const char* dateYmd, EinqCotdCard& out);

/** Fetch when online, else load cache. */
bool syncForDate(const char* dateYmd, EinqCotdCard& out);

}  // namespace EinqCotd
