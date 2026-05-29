#pragma once

#include <cstddef>
#include <cstdint>

/** OTA manifest from einq.castalia.institute (GitHub Pages). */
namespace EinqOta {

constexpr char kManifestUrl[] = "https://einq.castalia.institute/firmware.json";
constexpr char kDefaultFirmwareUrl[] = "https://einq.castalia.institute/firmware.bin";

enum class Result : uint8_t { Ok, NoUpdate, HttpError, ParseError, InstallError };

bool isNewerThanRunning(const char* latestVersion);

/** Fetch firmware.json; fills version, firmware URL, and optional size. */
Result fetchManifest(char* versionOut, size_t versionLen, char* urlOut, size_t urlLen, size_t* sizeOut);

/** Download and install firmware from url. Reboots on success (does not return). */
Result installFromUrl(const char* url);

/**
 * If manifest version is newer than CROSSPOINT_VERSION, install and reboot.
 * Returns NoUpdate when already current; Failed variants otherwise.
 */
Result checkAndInstallIfNewer();

}  // namespace EinqOta
