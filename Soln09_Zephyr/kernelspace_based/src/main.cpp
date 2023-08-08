#include <atomic>
#include <cstring>
#include <iostream>
#include <string>

#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "uartpolling.h"

#define LED_DELAY_DEF (500U)
#define UART_DELAY (100U)

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

// devices
constexpr const device *uart_port = (DEVICE_DT_GET(DT_ALIAS(usercom0)));

UartPolling user_com_port{uart_port};

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

// circular buffer

constexpr size_t buffer_len =
    4; // The ring buffer has 'n' buffers that can be configured. Min. 2
constexpr size_t buffer_mem_len = 10;

using buf = struct {
  uint16_t adc_val[buffer_mem_len];
};

volatile buf ring_buffer[buffer_len] = {0};
volatile uint8_t head = 0;
volatile uint8_t tail = 0;

std::atomic<float> avg_adc = {0.0f};

// syncs
k_sem signal_buff_full; // Signal a buffer is full
k_mutex avg_mutex;      // sync between the adc processor task and uart task

// Timer stuff

k_timer adc_read_timer;

void adc_read_work_handler(struct k_work *work);
void adc_read_timer_expiry_handler(k_timer *id);

k_work adc_read_work = {
    .handler = adc_read_work_handler,
};

// ADC:

static const struct adc_dt_spec adc_chan0 =
    ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);

void adc_read_timer_expiry_handler(k_timer *id) {
  // handle timer expiry
  // k_work_submit(&adc_read_work);

  if (((head + 1) % buffer_len) == tail) {
    // drop the elements, buffer full, sorry
    LOG_INF("ISR: Sorry! Buffer Full!! dropping adc_values....");
    return;
  }

  static size_t buffer_mem_count = 0;

  if (buffer_mem_count < buffer_mem_len) {
    size_t l_buffer_mem_count = buffer_mem_count;

    static uint16_t val_mv;
    static struct adc_sequence sequence = {
        .buffer = &val_mv,
        /* buffer size in bytes, not number of samples */
        .buffer_size = sizeof(val_mv),
    };
    (void)adc_sequence_init_dt(&adc_chan0, &sequence);
    int err = adc_read(adc_chan0.dev, &sequence);
    if (err < 0) {
      LOG_ERR("unable to read ADC channel 0(%d)\n", err);
      return;
    }

    ring_buffer[head].adc_val[buffer_mem_count] = val_mv;
    buffer_mem_count++;

#if DBG
    LOG_INF("ISR ring_buffer[%d].adc_val[%d] = %d", head, l_buffer_mem_count,
            ring_buffer[head].adc_val[l_buffer_mem_count]);
#endif
  }

  if (buffer_mem_count >= buffer_mem_len) {

    head = ((head + 1) % buffer_len); // let's fill the next buffer element
    buffer_mem_count = 0;             // reset the member buffer count

    // Signal a buffer is ready to be consumed
    k_sem_give(&signal_buff_full);
  }
}

static void adc_processing_thread(void *param1, void *param2, void *param3) {

  uint32_t adc_sum = 0;
  uint32_t l_tail = 0;

  while (true) {
    if (!k_sem_take(&signal_buff_full, K_FOREVER)) {
      adc_sum = 0;

      for (uint8_t i = 0; i < buffer_mem_len; i++) {
        adc_sum += ring_buffer[tail].adc_val[i];

        // LOG_INF("adc: ring_buffer[%d].adc_val[%d] = %d", tail, i,
        // ring_buffer[tail].adc_val[i]);
      }
      l_tail = tail;
      tail = ((tail + 1) % buffer_len);

#if DBG
      for (uint8_t i = 0; i < buffer_mem_len; i++) {
        LOG_INF("adc: ring_buffer[%d].adc_val[%d] = %d", l_tail, i,
                ring_buffer[l_tail].adc_val[i]);
      }
      LOG_INF("=========", "==========");
#endif
      if (!k_mutex_lock(&avg_mutex, K_FOREVER)) {
        avg_adc.store((float)(((float)adc_sum / (buffer_mem_len))));
        k_mutex_unlock(&avg_mutex);
      }
    }
    // k_msleep(10 * UART_DELAY); // only to observe buffer full
  }
}

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
        k_msleep(3 * UART_DELAY);
      }
    } while (!ret);

    read_buff[index] = '\0';

    if (index) {

      if (!strcmp("avg\r", (const char *)read_buff) ||
          !strcmp("avg\n", (const char *)read_buff)) {
        float l_avg = 0.0f;
        if (!k_mutex_lock(&avg_mutex, K_FOREVER)) {
          l_avg = avg_adc.load();
          k_mutex_unlock(&avg_mutex);
        }
        int32_t val_mv = (int32_t)l_avg;
        int err = adc_raw_to_millivolts_dt(&adc_chan0, &val_mv);
        if (err) {
          LOG_ERR("Error in adc_raw_to_millivolts_dt: %d", err);
          continue;
        }
        LOG_INF("Average is %f, Voltage at Pin = %u mV", l_avg, val_mv);
      }
    }

    memset(read_buff, 0, index + 1); // clear the local buffer
    index = 0;
    k_msleep(UART_DELAY);
  }
}

extern "C" int main(void) {

  // ADC ch 0 init
		if (!device_is_ready(adc_chan0.dev)) {
			LOG_ERR("ADC %s not ready\n", adc_chan0.dev->name);
			return 0;
		}

		int err = adc_channel_setup_dt(&adc_chan0);
		if (err < 0) {
			LOG_ERR("ADC channel 0 failed (%d)\n", err);
			return 0;
		}

  if (k_mutex_init(&avg_mutex)) {
    LOG_ERR("mutex avg_mutex init failed");
    return 0;
  }

  if (k_sem_init(&signal_buff_full, 0, buffer_len)) {
    LOG_ERR("semaphore signal_buff_full init failed");
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
      uart_read_thread, NULL, NULL, NULL, thread_0_prio, 0, K_NO_WAIT);

  LOG_INF("Starting adc processing Thread ...");

  thread_1_tid = k_thread_create(
      &thread_1, stack_thread_1, K_THREAD_STACK_SIZEOF(stack_thread_1),
      adc_processing_thread, NULL, NULL, NULL, thread_1_prio, 0, K_NO_WAIT);

  k_timer_init(&adc_read_timer, adc_read_timer_expiry_handler, NULL);
  LOG_INF("Starting ADC Timer ...");

  k_timer_start(&adc_read_timer, K_MSEC(100), K_MSEC(100));
  while (true) {
    // Do nothing
    k_msleep(2 * LED_DELAY_DEF);
  }

  return 0;
}
