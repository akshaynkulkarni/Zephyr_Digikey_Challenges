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
static k_thread thread_1;
static k_thread thread_0;

static k_tid_t thread_1_tid;
static k_tid_t thread_0_tid;

K_THREAD_STACK_DEFINE(stack_thread_1, kThreadStackSize);
K_THREAD_STACK_DEFINE(stack_thread_0, kThreadStackSize);

constexpr int thread_1_prio = 1;
constexpr int thread_0_prio = 1;

// In the corresponding DT overlays, we defined same gpio pin with same alias
constexpr gpio_dt_spec led_pin = GPIO_DT_SPEC_GET(DT_ALIAS(userled0), gpios);

Led led{led_pin};

static void led_toggle_thread(void *param1, void *param2, void *param3) {
  const uint8_t thread_no = *((const uint8_t *)param1);
  const uint32_t led_delay = thread_no ? DELAY2 : DELAY1;

  while (true) {
    std::cout << "led_toggle_thread " << unsigned(thread_no) << std::endl;
    if (led.Toggle()) {
      std::cout << "Thread " << unsigned(thread_no) << ": led toggle error"
                << std::endl;
    }
    k_msleep(led_delay);
  }
}

extern "C" int main(void) {

  static uint8_t const thread0_param = 0;
  static uint8_t const thread1_param = 1;

  if (led.Init()) {
    return 0;
  }

  std::cout << "Starting Thread 0 ..." << std::endl;

  thread_0_tid = k_thread_create(&thread_0, stack_thread_0,
                                 K_THREAD_STACK_SIZEOF(stack_thread_0),
                                 led_toggle_thread, (void *)&thread0_param,
                                 NULL, NULL, thread_0_prio, K_USER, K_NO_WAIT);

  std::cout << "Starting Thread 1 ..." << std::endl;

  thread_1_tid = k_thread_create(&thread_1, stack_thread_1,
                                 K_THREAD_STACK_SIZEOF(stack_thread_1),
                                 led_toggle_thread, (void *)&thread1_param,
                                 NULL, NULL, thread_1_prio, K_USER, K_NO_WAIT);

  while (true) {
    // Do nothing
    k_msleep(2 * DELAY2);
  }

  return 0;
}
