#include <iostream>

#include "led.h"

Led::Led(const gpio_dt_spec &user_pin = m_def_pin) : m_pin(user_pin) {}

int Led::Init() {
  if (!gpio_is_ready_dt(&m_pin)) {
    std::cout << "Led_init: not ready.." << std::endl;
    return -1;
  }

  error_t ret = gpio_pin_configure_dt(&m_pin, GPIO_OUTPUT_INACTIVE);

  if (ret < 0) {
    std::cout << "Led_init: error configuring Led gpio" << std::endl;
    return ret;
  }
  return 0;
}

int Led::DeInit() { return gpio_pin_configure_dt(&m_pin, GPIO_DISCONNECTED); }

int Led::Toggle() { return gpio_pin_toggle_dt(&m_pin); }

int Led::On() { return gpio_pin_set_dt(&m_pin, true); }

int Led::Off() { return gpio_pin_set_dt(&m_pin, false); }