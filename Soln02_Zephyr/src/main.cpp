#include <iostream>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "led.h"

#define DELAY1 (200U)
#define DELAY2 (500U)

// Threads
#if CONFIG_BOARD_ESP_WROVER_KIT
constexpr size_t kThreadStackSize = 4 * 1024;
#else
constexpr size_t kThreadStackSize = 2 * 1024;
#endif
struct k_thread thread_1;
struct k_thread thread_2;

k_tid_t thread_1_tid;
k_tid_t thread_2_tid;

K_THREAD_STACK_DEFINE(stack_thread_1, kThreadStackSize);
K_THREAD_STACK_DEFINE(stack_thread_2, kThreadStackSize);

#if CONFIG_BOARD_ESP_WROVER_KIT
constexpr gpio_dt_spec led_pin = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
#else
constexpr gpio_dt_spec led_pin = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
#endif

Led led{led_pin};

static void led_toggle_thread(void *param1, void *param2, void *param3) {
  const uint8_t task_no = *((const uint8_t *)param1);
  const uint32_t led_delay = (*((const uint8_t *)param1)) ? DELAY2 : DELAY1;

  while (true) {
    std::cout << "led_toggle_thread " << unsigned(task_no) << std::endl;
    led.Toggle();
    k_msleep(led_delay);
  }
}

extern "C" int main(void) {

  static uint8_t const task0_param = 0;
  static uint8_t const task1_param = 1;

  if (led.Init()) {
    return 0;
  }

  thread_1_tid = k_thread_create(
      &thread_1, stack_thread_1, K_THREAD_STACK_SIZEOF(stack_thread_1),
      led_toggle_thread, (void *)&task0_param, NULL, NULL, 5, 0, K_NO_WAIT);

  thread_2_tid = k_thread_create(
      &thread_2, stack_thread_2, K_THREAD_STACK_SIZEOF(stack_thread_2),
      led_toggle_thread, (void *)&task1_param, NULL, NULL, 5, 0, K_NO_WAIT);
  while (true) {
    // Do nothing
    k_msleep(2 * DELAY2);
  }

  return 0;
}
