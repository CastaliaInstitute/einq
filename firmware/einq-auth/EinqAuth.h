#pragma once

#include <cstddef>

struct EinqPairing {
  char pairId[64] {};
  char pairSecret[96] {};
  char signInUrl[384] {};
  bool valid = false;
};

namespace EinqAuth {

enum class PollResult { Pending, Ready, Expired, Error };

bool hasSession();
bool startPairing(EinqPairing& out);
PollResult pollPairing(const EinqPairing& pairing);
bool refreshIfNeeded();
bool clearSession();

}  // namespace EinqAuth
