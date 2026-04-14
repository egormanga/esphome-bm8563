#include "bm8563.h"
#include <esphome/core/log.h>

namespace esphome {
namespace bm8563 {

static const char *const TAG = "bm8563";

void BM8563Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BM8563...");
  this->write_byte_16(0, 0);
  if (!this->read_rtc_()) {
    this->mark_failed();
  }
}

void BM8563Component::update() { this->read_time(); }

void BM8563Component::dump_config() {
  ESP_LOGCONFIG(TAG, "BM8563:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with BM8563 failed!");
  }
}

float BM8563Component::get_setup_priority() const { return setup_priority::DATA; }

void BM8563Component::read_time() {
  if (!this->read_rtc_()) {
    return;
  }
  if (bm8563_.reg.vl) {
    ESP_LOGW(TAG, "RTC halted, not syncing to system clock.");
    return;
  }
  ESPTime rtc_time{
      .second = (uint8_t)(bm8563_.reg.second + bm8563_.reg.second_10*10),
      .minute = (uint8_t)(bm8563_.reg.minute + bm8563_.reg.minute_10*10),
      .hour = (uint8_t)(bm8563_.reg.hour + bm8563_.reg.hour_10*10),
      .day_of_week = bm8563_.reg.weekday,
      .day_of_month = (uint8_t)(bm8563_.reg.day + bm8563_.reg.day_10*10),
      .day_of_year = 1,  // ignored by recalc_timestamp_utc(false)
      .month = (uint8_t)(bm8563_.reg.month + bm8563_.reg.month_10*10),
      .year = (uint16_t)(bm8563_.reg.year + bm8563_.reg.year_10*10 + 1900 + 100*(bm8563_.reg.century)),
      .is_dst = false,   // not used
      .timestamp = 0,    // overwritten by recalc_timestamp_utc(false)
  };
  rtc_time.recalc_timestamp_utc(false);
  if (!rtc_time.is_valid()) {
    ESP_LOGE(TAG, "Invalid RTC time, not syncing to system clock.");
    return;
  }
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

void BM8563Component::write_time() {
  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }

  bm8563_.reg.year = (now.year % 2000) % 10;
  bm8563_.reg.year_10 = (now.year % 2000) / 10 % 10;
  bm8563_.reg.century = (now.year >= 2000);
  bm8563_.reg.month = now.month % 10;
  bm8563_.reg.month_10 = now.month / 10;
  bm8563_.reg.day = now.day_of_month % 10;
  bm8563_.reg.day_10 = now.day_of_month / 10;
  bm8563_.reg.weekday = now.day_of_week;
  bm8563_.reg.hour = now.hour % 10;
  bm8563_.reg.hour_10 = now.hour / 10;
  bm8563_.reg.minute = now.minute % 10;
  bm8563_.reg.minute_10 = now.minute / 10;
  bm8563_.reg.second = now.second % 10;
  bm8563_.reg.second_10 = now.second / 10;
  bm8563_.reg.vl = false;

  this->write_rtc_();
}

bool BM8563Component::read_rtc_() {
  if (!this->read_bytes(0x02, this->bm8563_.raw, sizeof(this->bm8563_.raw))) {
    ESP_LOGE(TAG, "Can't read I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Read  %0u%0u:%0u%0u:%0u%0u %0u%0u%0u-%0u%0u-%0u%0u  VL:%s",
           bm8563_.reg.hour_10, bm8563_.reg.hour,
           bm8563_.reg.minute_10, bm8563_.reg.minute,
           bm8563_.reg.second_10, bm8563_.reg.second,
           bm8563_.reg.century?20:19, bm8563_.reg.year_10, bm8563_.reg.year,
           bm8563_.reg.month_10, bm8563_.reg.month,
           bm8563_.reg.day_10, bm8563_.reg.day,
           ONOFF(bm8563_.reg.vl));
  return true;
}

bool BM8563Component::write_rtc_() {
  if (!this->write_bytes(0x02, this->bm8563_.raw, sizeof(this->bm8563_.raw))) {
    ESP_LOGE(TAG, "Can't write I2C data.");
    return false;
  }
  ESP_LOGD(TAG, "Write %0u%0u:%0u%0u:%0u%0u %0u%0u%0u-%0u%0u-%0u%0u  VL:%s",
           bm8563_.reg.hour_10, bm8563_.reg.hour,
           bm8563_.reg.minute_10, bm8563_.reg.minute,
           bm8563_.reg.second_10, bm8563_.reg.second,
           bm8563_.reg.century?20:19, bm8563_.reg.year_10, bm8563_.reg.year,
           bm8563_.reg.month_10, bm8563_.reg.month,
           bm8563_.reg.day_10, bm8563_.reg.day,
           ONOFF(bm8563_.reg.vl));
  return true;
}

}  // namespace bm8563
}  // namespace esphome
