#ifndef LED_H
#define LED_H

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_instance.h>

struct led_instance {
  LOG_INSTANCE_PTR_DECLARE(log);
  uint32_t id;
};

#define LED_LOGGER_DEFINE(_name)                                               \
  LOG_INSTANCE_REGISTER(led_instance, _name, CONFIG_LOG_DEFAULT_LEVEL)         \
  struct led_instance _name = {LOG_INSTANCE_PTR_INIT(log, led_instance, _name)};

class Led {

  constexpr static gpio_dt_spec m_def_pin =
      GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

  gpio_dt_spec m_pin;

  const led_instance& m_logger;

public:
  Led(const gpio_dt_spec &user_pin, const led_instance &logger_instance);
  int Init();
  int DeInit();
  int Toggle();
  int On();
  int Off();
  ~Led() = default;
};

#endif // LED_H