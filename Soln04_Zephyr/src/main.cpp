#include <atomic>
#include <cstring>
#include <iostream>
#include <string>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "uartpolling.h"

#include "led.h"

#define DELAY1 (200U)
#define DELAY2 (500U)
#define UART_DELAY (100U)
// Threads
#if CONFIG_BOARD_ESP_WROVER_KIT
constexpr const size_t kThreadStackSize = 4 * 1024;
#else
constexpr const size_t kThreadStackSize = 2 * 1024;
#endif

static k_thread thread_0;
static k_thread thread_1;

static k_tid_t thread_0_tid;
static k_tid_t thread_1_tid;

K_THREAD_STACK_DEFINE(stack_thread_0, kThreadStackSize);
K_THREAD_STACK_DEFINE(stack_thread_1, kThreadStackSize);

constexpr const int thread_0_prio = 1;
constexpr const int thread_1_prio = 1;

// In the corresponding DT overlays, we defined same gpio pin with same alias
constexpr const gpio_dt_spec led_pin =
    GPIO_DT_SPEC_GET(DT_ALIAS(userled0), gpios);

constexpr const device *uart_port = (DEVICE_DT_GET(DT_ALIAS(usercom1)));

static std::atomic<uint32_t> led_delay = DELAY2;

Led led{led_pin};
UartPolling user_com_port{uart_port};

static void led_toggle_thread(void *param1, void *param2, void *param3) {

  while (true) {
    // std::cout << "led_toggle_thread @" << led_delay.load() << "ms" <<
    // std::endl;
    if (led.Toggle()) {
      std::cout << "Thread "
                << ": led toggle error" << std::endl;
    }
    k_msleep(led_delay.load());
  }
}

static void uart_read_thread(void *param1, void *param2, void *param3) {
  int ret = 0;
  size_t index = 0;
  unsigned char read_buff[100] = {'0'};
  size_t read_buff_size = sizeof(read_buff);

  while (true) {

    do {
      if ((ret = user_com_port.Read(read_buff[index])) == 0) {
        user_com_port.Write(read_buff[index++]);
        if (index > read_buff_size - 1) {
          index = read_buff_size - 1;
          break;
        }
      }
      k_msleep(3 * UART_DELAY);
    } while (!ret);

    read_buff[index] = '\0';

    if (index) {
      sscanf((const char *)read_buff, "%[0-9]s", read_buff);

      if ((atoi((const char *)read_buff))) {
        k_thread_suspend(
            thread_1_tid); // Immediate effect: Suspend the led task as it is
                           // currently blocked due to k_msleep()
        led_delay.store((atoi((const char *)read_buff)));
        std::cout << "\n\rUpdate delay time to: "
                  << atoi((const char *)read_buff) << "ms" << std::endl;
        k_thread_resume(thread_1_tid); // Immediate effect: Start blinking
        // with
        //  new delay time
      } else {
        std::cout << "\n\rEnter only numbers[0-9], No delay time update!"
                  << std::endl;
      }
      memset(read_buff, 0, index); // Clear the buffer
      index = 0;
    }
    k_msleep(UART_DELAY);
  }
}

extern "C" int main(void) {

  if (led.Init()) {
    return 0;
  }

  while (!user_com_port.Init()) {
    std::cout << "Uart port is not ready... waiting.." << std::endl;
    k_msleep(2 * DELAY2);
  }

  std::cout << "Starting Uart Thread ..." << std::endl;

  thread_0_tid = k_thread_create(
      &thread_0, stack_thread_0, K_THREAD_STACK_SIZEOF(stack_thread_0),
      uart_read_thread, NULL, NULL, NULL, thread_0_prio, K_USER, K_NO_WAIT);

  std::cout << "Starting LED Thread ..." << std::endl;

  thread_1_tid = k_thread_create(
      &thread_1, stack_thread_1, K_THREAD_STACK_SIZEOF(stack_thread_1),
      led_toggle_thread, NULL, NULL, NULL, thread_1_prio, K_USER, K_NO_WAIT);

  while (true) {
    // Do nothing
    k_msleep(2 * DELAY2);
  }

  return 0;
}
