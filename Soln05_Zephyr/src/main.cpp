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
    GPIO_DT_SPEC_GET(DT_ALIAS(userled0), gpios);

constexpr const device *uart_port = (DEVICE_DT_GET(DT_ALIAS(usercom0)));

Led led{led_pin};

UartPolling user_com_port{uart_port};

// Queues:
using msg1_t = __attribute__((aligned(4))) uint32_t;
using msg2_t = struct msg2_st {
  std::string bmsg;
  uint32_t blinks;
} __attribute__((aligned(4)));
constexpr size_t kQueueLength = 10;
constexpr size_t kMsg1Size = sizeof(msg1_t);
constexpr size_t kMsg2Size = sizeof(msg2_t);

// k_msgq is a structure that maintains the respective buffer
k_msgq msg1_queue_handle = {NULL};
k_msgq msg2_queue_handle = {NULL};

// Create a queue buffer with word alignment.
char __aligned(4) msg1_buffer[kQueueLength * kMsg1Size];
char __aligned(4) msg2_buffer[kQueueLength * kMsg2Size];

// Static (Compile time) Queue creation: The param 4 -> __aligned(4)
// K_MSGQ_DEFINE(msg1_queue_handle, kMsg1Size, kQueueLength, 4);

static void led_blink_thread(void *param1, void *param2, void *param3) {

  static uint32_t led_blink_count = 0;
  static msg2_t msg = {.bmsg = std::string("Blink count is "),
                       .blinks = led_blink_count};
  static msg1_t msg_buff = LED_DELAY_DEF;

  while (true) {
    led.Toggle();
    led_blink_count++;
    if (led_blink_count % 100 == 0) {
      msg.blinks = led_blink_count;
      if (k_msgq_put(&msg2_queue_handle, reinterpret_cast<const void *>(&msg),
                     K_NO_WAIT)) {
        LOG_ERR("%s: Error Sending Queue for msg2", __func__);
      }
    }
    if (!k_msgq_get(&msg1_queue_handle, reinterpret_cast<void *>(&msg_buff),
                    K_NO_WAIT)) {
      LOG_INF("%s: msg1 recived: update delay time: %d", __func__, msg_buff);
      led_blink_count = 0;
    }
    k_msleep(msg_buff);
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
      static msg2_t msg_buff;
      if (!k_msgq_get(&msg2_queue_handle, &msg_buff, K_NO_WAIT))
        LOG_INF("%s: Message from msg2: %s %d", __func__, msg_buff.bmsg.data(),
                msg_buff.blinks);
      k_msleep(2.5 * UART_DELAY);
    } while (!ret);

    read_buff[index] = '\0';
    if (index) {
      static int delay_time = 0;
      const char *delay_str = "delay";
      char delay_scanned[strlen(delay_str)] = {'0'};

      sscanf(reinterpret_cast<const char *>(read_buff), "%s %d", delay_scanned,
             &delay_time);

      if (!strcmp(delay_str, delay_scanned)) {
        k_thread_suspend(
            thread_1_tid); // Immediate effect: Suspend the led task as it is
                           // currently blocked due to k_msleep()
        // insert the delay
        if (k_msgq_put(&msg1_queue_handle,
                       reinterpret_cast<const void *>(&delay_time), K_NO_WAIT))
          LOG_ERR("%s: Error sending msg1", __func__);

        k_thread_resume(thread_1_tid); // Immediate effect: Start blinking
        // with
        //  new delay time
      }
    }
    memset(read_buff, 0, index + 1); // clear the local buffer
    index = 0;
    k_msleep(UART_DELAY);
  }
}

extern "C" int main(void) {

  // Init the msgQ buffers
  k_msgq_init(&msg1_queue_handle, msg1_buffer, kMsg1Size, kQueueLength);
  k_msgq_init(&msg2_queue_handle, msg2_buffer, kMsg2Size, kQueueLength);

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

  LOG_INF("Starting uart Thread ...");

  thread_0_tid = k_thread_create(
      &thread_0, stack_thread_0, K_THREAD_STACK_SIZEOF(stack_thread_0),
      uart_read_thread, NULL, NULL, NULL, thread_0_prio, K_USER, K_NO_WAIT);

  LOG_INF("Starting led Thread ...");

  thread_1_tid = k_thread_create(
      &thread_1, stack_thread_1, K_THREAD_STACK_SIZEOF(stack_thread_1),
      led_blink_thread, NULL, NULL, NULL, thread_1_prio, K_USER, K_NO_WAIT);

  while (true) {
    // Do nothing
    k_msleep(2 * LED_DELAY_DEF);
  }

  return 0;
}
