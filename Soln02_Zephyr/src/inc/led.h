#include <zephyr/drivers/gpio.h>

class Led {

  constexpr static gpio_dt_spec m_def_pin =
      GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

  gpio_dt_spec m_pin;

public:
  Led(const gpio_dt_spec &user_pin);
  int Init();
  int DeInit();
  int Toggle();
  int On();
  int Off();
  ~Led() = default;
};