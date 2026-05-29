#include "EinqSchedule.h"

#include <InputManager.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include <algorithm>

namespace EinqSchedule {

uint32_t secondsUntilNextMinute(const struct tm& localTime) {
  const uint32_t sec = static_cast<uint32_t>(localTime.tm_sec);
  return sec == 0 ? 60U : (60U - sec);
}

uint32_t secondsUntilMidnight(const struct tm& localTime) {
  const uint32_t hour = static_cast<uint32_t>(localTime.tm_hour);
  const uint32_t min = static_cast<uint32_t>(localTime.tm_min);
  const uint32_t sec = static_cast<uint32_t>(localTime.tm_sec);
  const uint32_t sinceMidnight = hour * 3600U + min * 60U + sec;
  return sinceMidnight == 0 ? 86400U : (86400U - sinceMidnight);
}

uint32_t nextWakeSeconds(const struct tm& localTime) {
  return std::min(secondsUntilNextMinute(localTime), secondsUntilMidnight(localTime));
}

int lightSleepForSeconds(uint32_t seconds) {
  if (seconds == 0) {
    return ESP_SLEEP_WAKEUP_UNDEFINED;
  }

  const uint64_t wakeUs = static_cast<uint64_t>(seconds) * 1000000ULL;
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_enable_timer_wakeup(wakeUs);

  const gpio_num_t powerPin = static_cast<gpio_num_t>(InputManager::POWER_BUTTON_PIN);
  gpio_wakeup_enable(powerPin, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  return static_cast<int>(esp_light_sleep_start());
}

}  // namespace EinqSchedule
