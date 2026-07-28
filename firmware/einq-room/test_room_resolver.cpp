#include "EinqRoomResolver.h"

#include <cassert>
#include <cstring>

int main() {
  EinqRoom::Config config;
  config.smoothing = 1.0F;
  config.switchWins = 2;
  config.minimumLead = 5;
  config.sampleTtlMs = 1000;

  EinqRoom::Resolver resolver(config);
  assert(resolver.addBeacon("kitchen-beacon", "Kitchen"));
  assert(resolver.addBeacon("studio-beacon", "Studio"));
  assert(!resolver.observe("unknown", -40, 10));

  resolver.observe("kitchen-beacon", -48, 10);
  resolver.observe("studio-beacon", -70, 10);
  assert(!resolver.resolve(10).known);
  const auto kitchen = resolver.resolve(20);
  assert(kitchen.known);
  assert(kitchen.changed);
  assert(std::strcmp(kitchen.room, "Kitchen") == 0);

  resolver.observe("kitchen-beacon", -72, 30);
  resolver.observe("studio-beacon", -45, 30);
  const auto pending = resolver.resolve(30);
  assert(std::strcmp(pending.room, "Kitchen") == 0);
  const auto studio = resolver.resolve(40);
  assert(studio.changed);
  assert(std::strcmp(studio.room, "Studio") == 0);

  const auto expired = resolver.resolve(2000);
  assert(!expired.known);
  return 0;
}
