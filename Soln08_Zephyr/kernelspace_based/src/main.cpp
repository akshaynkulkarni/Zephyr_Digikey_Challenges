#include <atomic>
#include <cstring>
#include <iostream>
#include <string>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "led.h"
#include "uartpolling.h"

#define LED_DELAY_DEF (500U)
#define UART_DELAY (100U)

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

// Threads
#if CONFIG_BOARD_ESP
constexpr size_t kThreadStackSize = 4 * 1024;
#else
constexpr size_t kThreadStackSize = 2 * 1024;
#endif

static k_thread thread_0;
static k_thread thread_1;

static k_tid_t thread_0_tid;
static k_tid_t thread_1_tid;

K_THREAD_STACK_DEFINE(stack_thread_0, kThreadStackSize);
K_THREAD_STACK_DEFINE(stack_thread_1, kThreadStackSize);

constexpr int thread_0_prio = 10;
constexpr int thread_1_prio = 10;

constexpr const gpio_dt_spec led_pin =
    GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

constexpr const device *uart_port = (DEVICE_DT_GET(DT_ALIAS(usercom0)));

Led led{led_pin};

UartPolling user_com_port{uart_port};

//timer
k_timer led_off_timer;

void led_off_work_handler(struct k_work *work);
void led_off_timer_expiry_handler(k_timer* id);

k_work led_off_work = {
    .handler = led_off_work_handler,
};

void led_off_work_handler(struct k_work *work) {
  /* do the processing that needs to be done periodically */
        ARG_UNUSED(led.Off());
}

void led_off_timer_expiry_handler(k_timer* id) {
  // handle timer expiry
  k_work_submit(&led_off_work);
}

/* static void led_off_thread(void *param1, void *param2, void *param3) {


  while (true) {
    led.Off();

    k_msleep(LED_DELAY_DEF);
  }
} */

static void uart_read_thread(void *param1, void *param2, void *param3) {
  int ret = 0;
  size_t index = 0;
  size_t read_buff_size = 100;
  unsigned char read_buff[read_buff_size] = {'0'};

  while (true) {

    do {
      while (!(ret = user_com_port.Read(read_buff[index]))) {
        user_com_port.Write(read_buff[index++]);

        if (index > read_buff_size - 1) {
          index = read_buff_size - 1;
          ret = 1;
          break;
        }
        // k_msleep(2.5 * UART_DELAY);
      }
    } while (!ret);

    if (index) {
      ARG_UNUSED(led.On());

      if (k_timer_status_get(&led_off_timer) == 0) {
        LOG_ERR("led_off_timer ==== 0");
        k_timer_start(&led_off_timer, K_MSEC(5000), K_MSEC(5000));
      } else {
        LOG_ERR("led_off_timer ==== 1");
        k_timer_start(&led_off_timer, K_MSEC(5000), K_MSEC(5000));
      }
    }
    read_buff[index] = '\0';
    memset(read_buff, 0, index + 1); // clear the local buffer
    index = 0;
    k_msleep(UART_DELAY);
  }
}

extern "C" int main(void) {


  if (led.Init()) {
    return 0;
  }

  if (!user_com_port.Init()) {
    LOG_ERR("uart config failed ...");
    return 0;
  }

  if (!user_com_port.IsReady()) {
    LOG_ERR("uart port not found...");
    return 0;
  }

  k_timer_init(&led_off_timer, led_off_timer_expiry_handler, nullptr);


  LOG_INF("Starting uart Thread ...");

  thread_0_tid = k_thread_create(
      &thread_0, stack_thread_0, K_THREAD_STACK_SIZEOF(stack_thread_0),
      uart_read_thread, NULL, NULL, NULL, thread_0_prio, K_USER, K_NO_WAIT);

/*   LOG_INF("Starting led Thread ...");

  thread_1_tid = k_thread_create(
      &thread_1, stack_thread_1, K_THREAD_STACK_SIZEOF(stack_thread_1),
      led_off_thread, NULL, NULL, NULL, thread_1_prio, K_USER, K_NO_WAIT); */

  while (true) {
    // Do nothing
    k_msleep(2 * LED_DELAY_DEF);
  }

  return 0;
}
