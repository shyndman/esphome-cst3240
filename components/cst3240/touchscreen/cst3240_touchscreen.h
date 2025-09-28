#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cst3240 {

// CST3240 Register Map - based on TouchLib CSTMutualConstants.h
static const uint16_t CST3240_REG_TOUCH_START = 0xD000;
static const uint16_t CST3240_REG_TOUCH_COUNT = 0xD005;
static const uint16_t CST3240_REG_SYNC_SIGNAL = 0xD000;
static const uint16_t CST3240_REG_DEEP_SLEEP = 0xD105;

// Touch data buffer size (27 bytes for up to 5 touch points)
static const uint8_t CST3240_TOUCH_DATA_LEN = 27;

class CST3240ButtonListener {
public:
  virtual void update_button(bool state) = 0;
};

class CST3240Touchscreen : public touchscreen::Touchscreen,
                           public i2c::I2CDevice {
public:
  void setup() override;
  void update_touches() override;
  void dump_config() override;

  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }
  bool can_proceed() override {
    return this->setup_complete_ || this->is_failed();
  }
  void register_button_listener(CST3240ButtonListener *listener) {
    this->button_listeners_.push_back(listener);
  }

protected:
  bool read_register16_(uint16_t reg, uint8_t *data, size_t len);
  bool write_register16_(uint16_t reg, uint8_t value);
  void continue_setup_();
  void update_button_state_(bool state);
  void process_touch_data_();

  InternalGPIOPin *interrupt_pin_{};
  GPIOPin *reset_pin_{};
  uint8_t touch_data_[CST3240_TOUCH_DATA_LEN];
  bool setup_complete_{};
  std::vector<CST3240ButtonListener *> button_listeners_;
  bool button_touched_{};
};

} // namespace cst3240
} // namespace esphome
