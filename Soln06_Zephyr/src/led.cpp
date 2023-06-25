#include "led.h"
#include <zephyr/logging/log.h>

LED_LOGGER_DEFINE(m_default_log_instance);

LOG_LEVEL_SET(CONFIG_LOG_DEFAULT_LEVEL);

Led::Led(const gpio_dt_spec &user_pin = m_def_pin,
         const led_instance &logger_instance = m_default_log_instance)
    : m_pin(user_pin), m_logger(logger_instance) {}

int Led::Init() {
  if (!gpio_is_ready_dt(&m_pin)) {
    LOG_INST_ERR(m_logger.log, "Not ready..");
    return -1;
  }

  error_t ret = gpio_pin_configure_dt(&m_pin, GPIO_OUTPUT_INACTIVE);

  if (ret < 0) {
    LOG_INST_ERR(m_logger.log, "Error configuring");
    return ret;
  }
  LOG_INST_INF(m_logger.log, "Initialized..");
  return 0;
}

int Led::DeInit() {
  LOG_INST_INF(m_logger.log, "DeInitialized..");
  return gpio_pin_configure_dt(&m_pin, GPIO_DISCONNECTED);
}

int Led::Toggle() {
  LOG_INST_INF(m_logger.log, "Toggle..");
  return gpio_pin_toggle_dt(&m_pin);
}

int Led::On() {
  LOG_INST_INF(m_logger.log, "Switching ON..");
  return gpio_pin_set_dt(&m_pin, true);
}

int Led::Off() {
  LOG_INST_INF(m_logger.log, "Switching OFF..");
  return gpio_pin_set_dt(&m_pin, false);
}