#include <atomic>
#include <cstring>
#include <iostream>
#include <string>

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>

#include "uartpolling.h"

#define DELAY1 (200U)
#define DELAY2 (500U)
#define UART_DELAY (100U)
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

constexpr int thread_0_prio = 1;
constexpr int thread_1_prio = 1;

constexpr const device *uart_port = (DEVICE_DT_GET(DT_ALIAS(usercom1)));

// Led led{led_pin};
UartPolling user_com_port{uart_port};

// Shared buffer:
using shared_ptr = char *;
static std::atomic<shared_ptr> shared_data_ptr = NULL;

static void uart_write_thread(void *param1, void *param2, void *param3) {

  std::string prompt_string = "\n\rYou entered:\n\r";
  while (true) {
    shared_ptr temp = shared_data_ptr.load();

    if (temp != NULL) {
      std::string print_string = prompt_string + std::string(temp) + "\n\r";
      char *print = print_string.data();
      while (*print != '\0') {
        user_com_port.Write(static_cast<unsigned char>(*print++));
      }
      k_free(temp);
      temp = NULL;
      shared_data_ptr.store(temp);
    }
    k_msleep(UART_DELAY);
  }
}

static void uart_read_thread(void *param1, void *param2, void *param3) {
  int ret = 0;
  size_t index = 0;
  size_t read_buff_size = 100;
  unsigned char read_buff[read_buff_size] = {'0'};

  while (true) {

    do {
      if ((ret = user_com_port.Read(read_buff[index])) == 0) {
        user_com_port.Write(read_buff[index++]);
        if (index > read_buff_size - 1) {
          index = read_buff_size - 1;
          break;
        }
      }
      //k_msleep(2.5 * UART_DELAY);
    } while (!ret);

    read_buff[index] = '\0';

    if (read_buff[index - 1] == '\n' || read_buff[index - 1] == '\r') {
      shared_ptr temp = shared_data_ptr.load();
      if (temp == NULL) {
        temp = (char *)k_malloc((index + 1) * sizeof(char));
        memcpy(temp, read_buff, index + 1);
        shared_data_ptr.store(temp);
        index = 0;
      }
    } else if(index > read_buff_size - 2) {
      std::cout << "uart read buffer overflow, resetting..." << std::endl;
      index = 0;
    }
    k_msleep(UART_DELAY);
  }
}

extern "C" int main(void) {

  if (!user_com_port.Init()) {
    std::cout << "uart config failed ..." << std::endl;
    return 0;
  }

  if (!user_com_port.IsReady()) {
    std::cout << "uart port not found..." << std::endl;
    return 0;
  }

  std::cout << "Starting uart read Thread ..." << std::endl;

  thread_0_tid = k_thread_create(
      &thread_0, stack_thread_0, K_THREAD_STACK_SIZEOF(stack_thread_0),
      uart_read_thread, NULL, NULL, NULL, thread_0_prio, K_USER, K_NO_WAIT);

  std::cout << "Starting uart write Thread ..." << std::endl;

  thread_1_tid = k_thread_create(
      &thread_1, stack_thread_1, K_THREAD_STACK_SIZEOF(stack_thread_1),
      uart_write_thread, NULL, NULL, NULL, thread_1_prio, K_USER, K_NO_WAIT);

  while (true) {
    // Do nothing
    k_msleep(2 * DELAY2);
  }

  return 0;
}
