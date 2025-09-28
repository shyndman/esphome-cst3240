#include "cst3240_touchscreen.h"

namespace esphome {
namespace cst3240 {

static const char *const TAG = "cst3240.touchscreen";

void CST3240Touchscreen::setup() {
  ESP_LOGCONFIG(TAG, "Setting up CST3240 touchscreen...");

  // Perform reset sequence if reset pin is provided
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
    delay(1); // 1ms delay before reset
    this->reset_pin_->digital_write(false);
    delay(1); // 1ms reset pulse (datasheet minimum 0.1ms)
    this->reset_pin_->digital_write(true);
    // Wait 400ms for chip initialization (datasheet requirement)
    this->set_timeout(400, [this] { this->continue_setup_(); });
  } else {
    // No reset pin - wait 400ms for potential power-on initialization
    this->set_timeout(400, [this] { this->continue_setup_(); });
  }
}

void CST3240Touchscreen::update_touches() {
  // Read 27 bytes of touch data starting from 0xD000 (TouchLib protocol)
  if (!this->read_register16_(CST3240_REG_TOUCH_START, this->touch_data_,
                              CST3240_TOUCH_DATA_LEN)) {
    this->status_set_warning();
    ESP_LOGW(TAG, "Failed to read touch data");
    return;
  }

  // Send sync signal (0xAB) to 0xD000 as required by TouchLib protocol
  if (!this->write_register16_(CST3240_REG_SYNC_SIGNAL, 0xAB)) {
    ESP_LOGW(TAG, "Failed to send sync signal");
  }

  // Check if touch data is valid: (raw_data[5] & 0x0f) != 0
  uint8_t touch_count = this->touch_data_[5] & 0x0F;
  if (touch_count == 0 || touch_count > 5) {
    this->status_clear_warning();
    return; // No valid touches
  }

  this->status_clear_warning();
  this->process_touch_data_();
}

bool CST3240Touchscreen::read_register16_(uint16_t reg, uint8_t *data,
                                          size_t len) {
  if (this->read_register16(reg, data, len) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Read from register 0x%04X failed", reg);
    return false;
  }
  return true;
}

bool CST3240Touchscreen::write_register16_(uint16_t reg, uint8_t value) {
  if (this->write_register16(reg, &value, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Write to register 0x%04X failed", reg);
    return false;
  }
  return true;
}

void CST3240Touchscreen::process_touch_data_() {
  uint8_t touch_count = this->touch_data_[5] & 0x0F;

  for (uint8_t i = 0; i < touch_count && i < 5; i++) {
    // Each touch point uses 5 bytes in the buffer, starting at different
    // offsets Touch 0: bytes 0-4, Touch 1: bytes 7-11, Touch 2: bytes 12-16,
    // etc.
    uint8_t offset;
    switch (i) {
    case 0:
      offset = 0;
      break; // 0xD000-0xD004
    case 1:
      offset = 7;
      break; // 0xD007-0xD00B
    case 2:
      offset = 12;
      break; // 0xD00C-0xD010
    case 3:
      offset = 17;
      break; // 0xD011-0xD015
    case 4:
      offset = 22;
      break; // 0xD016-0xD01A
    default:
      continue;
    }

    // Extract touch state (lower 4 bits of first byte)
    uint8_t touch_state = this->touch_data_[offset] & 0x0F;
    if (touch_state != 0x06) { // 0x06 = pressed state
      continue;                // Skip this touch point
    }

    // Extract 12-bit coordinates (8 high bits + 4 low bits)
    // X coordinate: byte[1] (high 8 bits) + upper 4 bits of byte[3]
    uint16_t x = (this->touch_data_[offset + 1] << 4) |
                 ((this->touch_data_[offset + 3] >> 4) & 0x0F);

    // Y coordinate: byte[2] (high 8 bits) + lower 4 bits of byte[3]
    uint16_t y = (this->touch_data_[offset + 2] << 4) |
                 (this->touch_data_[offset + 3] & 0x0F);

    // Pressure value from byte[4]
    uint8_t pressure = this->touch_data_[offset + 4];

    // Add the touch point
    this->add_raw_touch_position_(i, x, y, pressure);
    ESP_LOGV(TAG, "Touch %d: x=%d, y=%d, pressure=%d", i, x, y, pressure);
  }
}

void CST3240Touchscreen::continue_setup_() {
  ESP_LOGCONFIG(TAG, "Continuing CST3240 setup...");

  // Setup interrupt pin if provided
  if (this->interrupt_pin_ != nullptr) {
    this->interrupt_pin_->setup();
    this->attach_interrupt_(this->interrupt_pin_, gpio::INTERRUPT_FALLING_EDGE);
    ESP_LOGCONFIG(TAG, "Interrupt pin configured");
  }

  // Try to read chip information
  uint8_t buffer[8];
  if (this->read_register16_(0xD204, buffer, 4)) {
    uint16_t project_id = buffer[0] | (buffer[1] << 8);
    uint16_t chip_id = buffer[2] | (buffer[3] << 8);
    ESP_LOGCONFIG(TAG, "Chip ID: 0x%04X, Project ID: 0x%04X", chip_id,
                  project_id);
  } else {
    ESP_LOGW(TAG, "Failed to read chip identification");
  }

  // Read display resolution if not already set
  if (this->x_raw_max_ == 0 || this->y_raw_max_ == 0) {
    if (this->read_register16_(0xD1F8, buffer, 4)) {
      this->x_raw_max_ = buffer[0] | (buffer[1] << 8);
      this->y_raw_max_ = buffer[2] | (buffer[3] << 8);
      ESP_LOGCONFIG(TAG, "Touch resolution: %dx%d", this->x_raw_max_,
                    this->y_raw_max_);

      if (this->swap_x_y_) {
        std::swap(this->x_raw_max_, this->y_raw_max_);
        ESP_LOGCONFIG(TAG, "Coordinates swapped to: %dx%d", this->x_raw_max_,
                      this->y_raw_max_);
      }
    } else {
      // Fallback to display dimensions
      if (this->display_ != nullptr) {
        this->x_raw_max_ = this->display_->get_native_width();
        this->y_raw_max_ = this->display_->get_native_height();
        ESP_LOGCONFIG(TAG, "Using display dimensions: %dx%d", this->x_raw_max_,
                      this->y_raw_max_);
      } else {
        // Default dimensions for CST3240 (common 480x480)
        this->x_raw_max_ = 480;
        this->y_raw_max_ = 480;
        ESP_LOGW(TAG, "Using default dimensions: %dx%d", this->x_raw_max_,
                 this->y_raw_max_);
      }
    }
  }

  // Test touch data read to verify communication
  if (this->read_register16_(CST3240_REG_TOUCH_START, buffer, 8)) {
    ESP_LOGCONFIG(TAG, "Touch communication test successful");
  } else {
    ESP_LOGE(TAG, "Touch communication test failed");
    this->mark_failed();
    return;
  }

  this->setup_complete_ = true;
  ESP_LOGCONFIG(TAG, "CST3240 setup completed successfully");
}

void CST3240Touchscreen::update_button_state_(bool state) {
  if (this->button_touched_ == state)
    return;
  this->button_touched_ = state;
  for (auto *listener : this->button_listeners_)
    listener->update_button(state);
}

void CST3240Touchscreen::dump_config() {
  ESP_LOGCONFIG(TAG,
                "CST3240 Touchscreen:\n"
                "  Address: 0x%02X\n"
                "  Touch Resolution: %dx%d\n"
                "  Update Interval: %ums",
                this->address_, this->x_raw_max_, this->y_raw_max_,
                this->get_update_interval());
  LOG_PIN("  Interrupt Pin: ", this->interrupt_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
  LOG_I2C_DEVICE(this);
}

} // namespace cst3240
} // namespace esphome
