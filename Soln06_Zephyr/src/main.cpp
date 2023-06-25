#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "led.h"

#define LED_DELAY1_DEF (300U)
#define LED_DELAY2_DEF (500U)
#define LED_DELAY3_DEF (234U)
#define UART_DELAY (100U)

// Threads
#if CONFIG_BOARD_ESP
constexpr size_t kThreadStackSize = 4 * 1024;
#else
constexpr size_t kThreadStackSize = 2 * 1024;
#endif

static k_thread thread_0;
static k_thread thread_1;
static k_thread thread_2;

static k_tid_t thread_0_tid;
static k_tid_t thread_1_tid;
static k_tid_t thread_2_tid;

K_KERNEL_STACK_DEFINE(stack_thread_0, kThreadStackSize);
K_THREAD_STACK_DEFINE(stack_thread_1, kThreadStackSize);
K_THREAD_STACK_DEFINE(stack_thread_2, kThreadStackSize);

constexpr int thread_0_prio = 10;
constexpr int thread_1_prio = 10;
constexpr int thread_2_prio = 10;

// Semaphore, Mutex etc

static k_sem param_read_k_semaphore = {NULL};

static k_mutex led_access_mutex = {NULL};

constexpr const gpio_dt_spec thread_led_pin =
    GPIO_DT_SPEC_GET(DT_ALIAS(userled0), gpios);
constexpr const gpio_dt_spec main_led_pin =
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

// logging
LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);
LED_LOGGER_DEFINE(userled0);
LED_LOGGER_DEFINE(led0);

Led thread_led{thread_led_pin, userled0};
Led main_led{main_led_pin, led0};

using led_task_parm_st = struct led_param {
  uint8_t task_no;
  uint32_t blink_rate;
};

static uint32_t blink_count = 0;
static void led_blink_thread(void *param1, void *param2, void *param3) {

  led_task_parm_st const led_params = *((const led_task_parm_st *)param1);
  if (led_params.task_no == 1) {
    k_sem_give(&param_read_k_semaphore);
  }

  LOG_INF("blink rate = %u", led_params.blink_rate);

  while (true) {
    k_mutex_lock(&led_access_mutex, K_FOREVER);
    thread_led.Toggle();
    LOG_INF("Blink count = %u", ++blink_count);
    k_mutex_unlock(&led_access_mutex);
    k_msleep(led_params.blink_rate);
  }
}

extern "C" int main(void) {

  led_task_parm_st led1_params = {.task_no = 1, .blink_rate = LED_DELAY1_DEF};
  led_task_parm_st led2_params = {.task_no = 2, .blink_rate = LED_DELAY2_DEF};
  led_task_parm_st led3_params = {.task_no = 3, .blink_rate = LED_DELAY3_DEF};

  k_sem_init(&param_read_k_semaphore, 1, 1); // Binary Semaphore
  k_mutex_init(&led_access_mutex);           // Mutex Init

  if (thread_led.Init()) {
    return 0;
  }
  if (main_led.Init()) {
    return 0;
  }
  LOG_INF("Starting led Thread 0...");

  thread_0_tid = k_thread_create(
      &thread_0, stack_thread_0, K_KERNEL_STACK_SIZEOF(stack_thread_0),
      led_blink_thread, (void *)&led1_params, nullptr,
      nullptr, thread_0_prio, K_ESSENTIAL, K_NO_WAIT);
  k_sem_take(&param_read_k_semaphore, K_FOREVER);

  LOG_INF("Starting led Thread 1...");

  thread_1_tid = k_thread_create(
      &thread_1, stack_thread_1, K_THREAD_STACK_SIZEOF(stack_thread_1),
      led_blink_thread, (void *)&led2_params, nullptr, nullptr, thread_1_prio,
      K_INHERIT_PERMS, K_NO_WAIT);

  LOG_INF("Starting led Thread 2...");

  thread_2_tid = k_thread_create(
      &thread_2, stack_thread_2, K_THREAD_STACK_SIZEOF(stack_thread_2),
      led_blink_thread, (void *)&led3_params, nullptr, nullptr, thread_2_prio,
      K_INHERIT_PERMS, K_NO_WAIT);

  uint32_t main_count = 0;

  while (true) {
    main_led.Toggle();
    LOG_INF("Blink Led: %u", ++main_count);
    k_msleep(LED_DELAY1_DEF);
  }

  return 0;
}
