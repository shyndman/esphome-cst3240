#pragma once

#include "../touchscreen/cst3240_touchscreen.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace cst3240 {

class CST3240Button : public binary_sensor::BinarySensor,
                      public Component,
                      public CST3240ButtonListener,
                      public Parented<CST3240Touchscreen> {
public:
  void setup() override;
  void dump_config() override;
  void update_button(bool state) override;
};

} // namespace cst3240
} // namespace esphome
