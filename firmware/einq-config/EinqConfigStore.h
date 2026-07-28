#pragma once

#include <cstddef>
#include <string>

namespace EinqConfigStore {

constexpr size_t kMaximumJsonBytes = 4096;

/** Load canonical configuration JSON, returning defaults when none is stored. */
std::string load();

/**
 * Validate and save configuration JSON.
 * On success, canonicalJson receives the normalized document returned by GET.
 */
bool save(const char* json, size_t length, std::string& canonicalJson, std::string& error);

void clear();

}  // namespace EinqConfigStore
