#include "hcpbridge.h"

namespace esphome {
namespace hcpbridge {

static const char *TAG = "hcpbridge";
void HCPBridge::setup() {
  this->engine = &HoermannGarageEngine::getInstance();
  // UART and Modbus server are now configured via YAML
  this->engine->setup(-1, -1, -1);
  ESP_LOGI(TAG, "HCPBridge component initialized");
}
void HCPBridge::add_on_state_callback(std::function<void()> &&callback, const char *tag) {
  auto wrapped_callback = [callback, tag]() {
    auto start = millis();
    callback();
    auto end = millis();
    ESP_LOGD(TAG, "Callback executed in %u ms [Tag: %s]", end - start, tag);
  };
  this->state_callback_.add(std::move(wrapped_callback));
}

void HCPBridge::update() {
  if (this->engine->state->changed) {
    this->engine->state->clearChanged();
    this->state_callback_.call();
  }
}
}  // namespace hcpbridge
}  // namespace esphome