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
  bool administration = false;
};

struct EinqHomePayload {
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

}  // namespace EinqHome
