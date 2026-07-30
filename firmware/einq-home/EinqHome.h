#pragma once

#include <cstddef>

struct EinqHomeEvent {
  char title[64] {};
  char start[32] {};
  char end[32] {};
  bool allDay = false;
  bool valid = false;
};

struct EinqHomeReading {
  char title[48] {};
  char summary[160] {};
  bool valid = false;
};

struct EinqHomeAttribution {
  char title[64] {};
  char summary[192] {};
  char byline[64] {};
  char source[64] {};
  char assetUrl[192] {};
  char sha256[65] {};
  bool valid = false;
};

struct EinqHomeTask {
  char title[64] {};
  char due[32] {};
  bool completed = false;
  bool valid = false;
};

struct EinqHomeCodexTask {
  char id[49] {};
  char host[29] {};
  char title[65] {};
  char model[17] {};
  char speed[17] {};
  char status[24] {};
  unsigned int totalTokens = 0;
  unsigned int rateTokensPerMinute = 0;
  bool pinned = false;
  bool valid = false;
};

struct EinqHomeCodex {
  EinqHomeCodexTask tasks[12] {};
  size_t taskCount = 0;
  size_t selectedIndex = 0;
  unsigned int revision = 0;
  bool valid = false;
};

struct EinqHomeLibrary {
  char catalogUrl[192] {};
  char revision[65] {};
  unsigned int bookCount = 0;
  unsigned int changedCount = 0;
  bool valid = false;
};

struct EinqHomeWeather {
  char condition[48] {};
  char temperature[24] {};
  char summary[128] {};
  bool valid = false;
};

struct EinqHomeFamilyMember {
  char name[32] {};
  char status[96] {};
  bool valid = false;
};

struct EinqHomeCard {
  char title[48] {};
  char summary[160] {};
  char domain[32] {};
  bool valid = false;
};

struct EinqHomeSpotify {
  char track[64] {};
  char artist[64] {};
  char device[48] {};
  int volume = 0;
  bool connected = false;
  bool playing = false;
};

struct EinqHomeLights {
  char room[32] {};
  char scene[48] {};
  int brightness = 0;
  bool available = false;
  bool on = false;
};

struct EinqHomePermissions {
  bool calendar = true;
  bool astrology = true;
  bool fortune = true;
  bool cards = true;
  bool spotifyControl = false;
  bool lightControl = false;
  bool codexControl = false;
  bool administration = false;
};

struct EinqHomePayload {
  char schema[40] = "castalia.device.daily.v1";
  char date[16] {};
  char generatedAt[32] {};
  char profile[16] = "parent";
  EinqHomeEvent nextEvent {};
  EinqHomeWeather weather {};
  EinqHomeReading dayAphorism {};
  EinqHomeReading selfWeather {};
  EinqHomeReading synastryWeather {};
  EinqHomeFamilyMember family[6] {};
  size_t familyCount = 0;
  EinqHomeReading fortune {};
  EinqHomeCard card {};
  EinqHomeAttribution news {};
  EinqHomeAttribution art {};
  EinqHomeAttribution quote {};
  EinqHomeReading mindfulness {};
  EinqHomeTask tasks[6] {};
  size_t taskCount = 0;
  EinqHomeCodex codex {};
  EinqHomeLibrary library {};
  EinqHomeSpotify spotify {};
  EinqHomeLights lights {};
  EinqHomePermissions permissions {};
  bool valid = false;
  bool fromCache = false;
};

namespace EinqHome {

bool loadCached(EinqHomePayload& out);
bool fetchAndCache(const char* room, EinqHomePayload& out);
bool sync(const char* room, EinqHomePayload& out);
bool sendAction(const char* action, const char* room);
bool sendAction(const char* action, const char* room, const char* taskId);

}  // namespace EinqHome
