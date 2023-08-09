/**
 *
 * Based on https://www.digikey.com/en/maker/projects/introduction-to-
 * rtos-solution-to-part-10-deadlock-and-starvation/872c6a057901432e84594d79fcb2cc5d
 *
 * ESP32 Dining Philosophers: Port and solve challenge on Zephyr OS.
 *
 * The classic "Dining Philosophers" problem in FreeRTOS form.
 *
 * Based on http://www.cs.virginia.edu/luther/COA2/S2019/pa05-dp.html
 *
 * Date: August 9, 2023
 * Authors: Shawn Hymel, Akshay Narahari Kulkarni
 * License: 0BSD
 */
#include <atomic>
#include <cstring>
#include <iostream>
#include <string>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/spinlock.h>
#include <zephyr/sys/util_macro.h>

#define LED_DELAY_DEF (500U)

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

//DEMO_BLOCKED_THREAD: 0-> solution; 1-> problem
#define DEMO_BLOCKED_THREAD 0

// Threads
#if CONFIG_BOARD_ESP
constexpr size_t kThreadStackSize{4 * 1024};
#else
constexpr size_t kThreadStackSize{2 * 1024};
#endif

// Globals
// Settings
int64_t cs_wait{500 * 1L};   // Time spent in critical section (ms)
int64_t med_wait{10000 * 1L}; // Time medium task spends working (ms)

constexpr int kThreadLPriority{30}; //higher the number, lower the priority
constexpr int kThreadMPriority{20};
constexpr int kThreadHPriority{10};
constexpr int kThreadDelay{0};
constexpr int kThreadOptions{0};

#if DEMO_BLOCKED_THREAD
static k_sem lock;
#else
static k_spinlock spinlock;
static k_spinlock_key_t spinlock_key;
#endif
//*****************************************************************************//
// Tasks

// Task L (low priority)

constexpr std::string_view doTaskL_task_TAG = "doTaskL_task";

void doTaskL(void *params1, void *params2, void *params3) {

  int64_t timestamp;

  // Do forever
  while (1) {

    // Take lock
    LOG_INF("%s: Task L trying to take lock...", doTaskL_task_TAG.data());
    int64_t timestamp1{k_uptime_get()};

#if DEMO_BLOCKED_THREAD
    k_sem_take(&lock, K_FOREVER);
#else
    // Enter CS
    spinlock_key = k_spin_lock(&spinlock);
#endif
    timestamp = k_uptime_get();
    timestamp1 = timestamp - timestamp1;
    // Hog the processor for a while doing nothing
    while (k_uptime_get() - timestamp < cs_wait)
      ;
      // Release lock
#if DEMO_BLOCKED_THREAD
    k_sem_give(&lock);
#else
    k_spin_unlock(&spinlock, spinlock_key);
#endif
    // Say how long we spend waiting for a lock
    LOG_INF(
        "%s: Task L got lock. Spent %lld ms waiting for lock. Did some work "
        "and released lock... ",
        doTaskL_task_TAG.data(), timestamp1);

    // Go to sleep
    k_msleep(500);
  }
}

constexpr std::string_view doTaskM_task_TAG = "doTaskM_task";
// Task M (medium priority)
void doTaskM(void *params1, void *params2, void *params3) {

  int64_t timestamp;

  // Do forever
  while (1) {

    // Hog the processor for a while doing nothing
    LOG_INF("%s: Task M doing some work...", doTaskM_task_TAG.data());
    timestamp = k_uptime_get();
    while (k_uptime_get() - timestamp < med_wait)
      ;

    LOG_INF("%s: Task M done!", doTaskM_task_TAG.data());
    // Go to sleep
    k_msleep(500);
  }
}

constexpr std::string_view doTaskH_task_TAG = "doTaskH_task";

// Task H (high priority)
void doTaskH(void *params1, void *params2, void *params3) {

  int64_t timestamp;

  // Do forever
  while (1) {

    // Take lock
    LOG_INF("%s: Task H trying to take lock...", doTaskH_task_TAG.data());
    int64_t timestamp1{k_uptime_get()};
#if DEMO_BLOCKED_THREAD
    k_sem_take(&lock, K_FOREVER);
#else
    spinlock_key = k_spin_lock(&spinlock);
#endif
    timestamp = k_uptime_get();
    timestamp1 = timestamp - timestamp1;
    // Hog the processor for a while doing nothing
    while (k_uptime_get() - timestamp < cs_wait)
      ;
      // Release lock
#if DEMO_BLOCKED_THREAD
    k_sem_give(&lock);
#else
    k_spin_unlock(&spinlock, spinlock_key);
#endif
    // Say how long we spend waiting for a lock
    LOG_INF(
        "%s: Task H got lock. Spent %lld ms waiting for lock. Did some work "
        "and released lock...",
        doTaskH_task_TAG.data(), timestamp1);

    // Go to sleep
    k_msleep(500);
  }
}

K_THREAD_STACK_DEFINE(thread_L_stack_area, kThreadStackSize);
K_THREAD_STACK_DEFINE(thread_M_stack_area, kThreadStackSize);
K_THREAD_STACK_DEFINE(thread_H_stack_area, kThreadStackSize);
static struct k_thread thread_L;
static struct k_thread thread_M;
static struct k_thread thread_H;

//*****************************************************************************//

constexpr std::string_view app_main_task_TAG = "app_main_task";

extern "C" int main(void) {

  LOG_INF("%s: ---Zephyr RTOS Priority Inversion Demo---",
          app_main_task_TAG.data());
#if DEMO_BLOCKED_THREAD
  if(k_sem_init(&lock, 1, 1)) {
      LOG_ERR("Sem init failed..");
      return 0;
  }
#endif

  LOG_INF("Starting LP thread ...");
  k_thread_create(&thread_L, thread_L_stack_area,
                  K_THREAD_STACK_SIZEOF(thread_L_stack_area), doTaskL, NULL,
                  NULL, NULL, kThreadLPriority, 0, K_NO_WAIT);

  k_msleep(10); // sleep 10ms
  LOG_INF("Starting HP thread ...");
  k_thread_create(&thread_H, thread_H_stack_area,
                  K_THREAD_STACK_SIZEOF(thread_H_stack_area), doTaskH, NULL,
                  NULL, NULL, kThreadHPriority, 0, K_NO_WAIT);
  LOG_INF("Starting MP thread ...");
  k_thread_create(&thread_M, thread_M_stack_area,
                  K_THREAD_STACK_SIZEOF(thread_M_stack_area), doTaskM, NULL,
                  NULL, NULL, kThreadMPriority, 0, K_NO_WAIT);
  while (true) {
      // Do nothing
      k_msleep(2 * LED_DELAY_DEF);
  }

  return 0;
}
