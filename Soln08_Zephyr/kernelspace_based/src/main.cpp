#include <atomic>
#include <cstring>
#include <iostream>
#include <string>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

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

static k_tid_t thread_0_tid;

K_THREAD_STACK_DEFINE(stack_thread_0, kThreadStackSize);

constexpr int thread_0_prio = 10;

constexpr const device *uart_port = (DEVICE_DT_GET(DT_ALIAS(usercom0)));

UartPolling user_com_port{uart_port};

constexpr const pwm_dt_spec pwm_led = PWM_DT_SPEC_GET(DT_ALIAS(user_pwm_led));

using pwm_type = uint32_t;
// Inspired from sample codes under Zephyr repo
constexpr pwm_type kMinPWMPeriod = PWM_SEC(0U);

#if CONFIG_BOARD_ESP322
constexpr pwm_type kMaxPWMPeriod = PWM_MSEC(0.5f);
#else
constexpr pwm_type kMaxPWMPeriod = PWM_MSEC(10U); // 20U
#endif

constexpr pwm_type kBrightnessLevels = 100U;

static pwm_type max_period = kMaxPWMPeriod;

// timer
k_timer led_off_timer;

void led_off_work_handler(struct k_work *work);
void led_off_timer_expiry_handler(k_timer *id);

k_work led_off_work = {
    .handler = led_off_work_handler,
};

void led_off_work_handler(struct k_work *work) {

  int ret;

  static pwm_type pulse_width;
  pulse_width = max_period;
  pwm_type level = kBrightnessLevels;

  pwm_set_dt(&pwm_led, max_period, max_period);

  while (level) {

    pulse_width = ((--level) * max_period) / kBrightnessLevels;

    // ret = pwm_set_dt(&pwm_led, max_period, pulse_width);
    ret = pwm_set_pulse_dt(&pwm_led, pulse_width);

    if (ret) {
      LOG_ERR("Error setting pulse width: %d\n", ret);
      LOG_ERR("pulse_width %u, level %u, max: %u: \n", pulse_width, level,
              max_period);
      break;
    }

    LOG_DBG("pulse_width %u, level %u, max: %u: \n", pulse_width, level,
            max_period);
    k_msleep(10U); // 10ms
  }
  pwm_set_dt(&pwm_led, max_period, 0); // turn off the led completely
}

void led_off_timer_expiry_handler(k_timer *id) {
  // handle timer expiry
  k_work_submit(&led_off_work);
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
        // k_msleep(2.5 * UART_DELAY);
      }
    } while (!ret);

    if (index) {
      int ret = pwm_set_dt(&pwm_led, max_period, max_period);
      if (ret) {
        LOG_ERR("Error setting pulse width: %d\n", ret);
        LOG_ERR("pulse_width %d, max: %u: \n", max_period, max_period);
      }
      k_timer_start(&led_off_timer, K_MSEC(5000), K_NO_WAIT);
    }

    read_buff[index] = '\0';
    memset(read_buff, 0, index + 1); // clear the local buffer
    index = 0;
    k_msleep(UART_DELAY);
  }
}

extern "C" int main(void) {

  if (!device_is_ready(pwm_led.dev)) {
    LOG_ERR("PWM led %s is not ready", pwm_led.dev->name);
    return 0;
  }
  LOG_INF("Calibrating for PWM channel %d...", pwm_led.channel);

  while (pwm_set_dt(&pwm_led, max_period, max_period / 2U)) {
    max_period /= 2U;
    if (max_period < (4U * kMinPWMPeriod)) {
      LOG_ERR("PWM led: min period %u not supported", 4U * kMinPWMPeriod);
      return 0;
    }
  }
  // Turn off the led
  pwm_set_dt(&pwm_led, max_period, 0);

  LOG_INF("PWM led: Done calibrating; maximum/minimum periods %u/%u nsec",
          max_period, kMinPWMPeriod);

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

  while (true) {
    // Do nothing
    k_msleep(2 * LED_DELAY_DEF);
  }

  return 0;
}
