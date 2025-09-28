#include "cst3240_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace cst3240 {

static const char *const TAG = "CST3240.binary_sensor";

void CST3240Button::setup() {
  this->parent_->register_button_listener(this);
  this->publish_initial_state(false);
}

void CST3240Button::dump_config() {
  LOG_BINARY_SENSOR("", "CST3240 Button", this);
}

void CST3240Button::update_button(bool state) { this->publish_state(state); }

} // namespace cst3240
} // namespace esphome
