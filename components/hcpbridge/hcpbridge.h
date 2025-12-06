#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "hoermann.h"

namespace esphome {
namespace hcpbridge {

class HCPBridge : public PollingComponent {
 public:
  void setup() override;
  void update() override;
  HoermannGarageEngine *engine;
  void add_on_state_callback(std::function<void()> &&callback, const char *tag);
  void add_prio_callback(std::function<void()> &&callback, const char *tag);

 protected:
  CallbackManager<void()> state_callback_;
};
}  // namespace hcpbridge
}  // namespace esphome