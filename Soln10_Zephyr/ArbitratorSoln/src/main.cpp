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
#include <zephyr/sys/util_macro.h>

#define LED_DELAY_DEF (500U)
#define UART_DELAY (100U)

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

// Settings
enum { NUM_TASKS = 5 }; // Number of tasks (philosophers)

// Threads
#if CONFIG_BOARD_ESP
constexpr size_t kThreadStackSize = 4 * 1024;
#else
constexpr size_t kThreadStackSize = 2 * 1024;
#endif

// Globals
static k_sem bin_sem{NULL};            // Wait for parameters to be read
static k_sem done_sem{NULL};           // Notifies main task when done
static k_mutex arbitrator_mutex{NULL}; // arbitrator_mutex
static k_mutex chopstick[NUM_TASKS]{NULL};

constexpr std::string_view Philosopher_task_TAG = "Philosopher_task";
constexpr std::string_view app_main_task_TAG = "app_main_task";

static k_thread thread[NUM_TASKS];

static k_tid_t thread_tid[NUM_TASKS];

#define K_THREAD_STACK_DEFINE_GROUP(x)                                                      \
  K_THREAD_STACK_DEFINE(stack_thread_##x, kThreadStackSize);

FOR_EACH(K_THREAD_STACK_DEFINE_GROUP, (;), 0, 1, 2, 3, 4);

k_thread_stack_t *stack_eat_thread_ptr[] = {stack_thread_0, stack_thread_1,
                                            stack_thread_2, stack_thread_3,
                                            stack_thread_4};

constexpr int thread_prio[NUM_TASKS]{10};

// The only task: eating
void eat(void *param1, void *param2, void *param3) {

  uint8_t num = 0;

  // Copy parameter and increment semaphore count
  num = *(uint8_t *)param1;
  k_sem_give(&bin_sem);
  uint8_t left = num;
  uint8_t right = (num + 1) % NUM_TASKS;

  if (!k_mutex_lock(&arbitrator_mutex, K_FOREVER)) {
    // Take left chopstick
    k_mutex_lock(&chopstick[left], K_FOREVER);
    LOG_INF("%s: Philosopher %u took chopstick %u", Philosopher_task_TAG.data(),
            num, left);

    // Add some delay to force deadlock
    k_msleep(1);

    // Take right chopstick
    k_mutex_lock(&chopstick[right], K_FOREVER);
    LOG_INF("%s: Philosopher %u took chopstick %u", Philosopher_task_TAG.data(),
            num, right);

    // Do some eating
    LOG_INF("%s: Philosopher %u is eating", Philosopher_task_TAG.data(), num);
    k_msleep(100);

    // Put down right chopstick
    k_mutex_unlock(&chopstick[right]);
    LOG_INF("%s: Philosopher %u returned chopstick %u",
            Philosopher_task_TAG.data(), num, right);

    // Put down left chopstick
    k_mutex_unlock(&chopstick[left]);
    LOG_INF("%s: Philosopher %u returned chopstick %u",
            Philosopher_task_TAG.data(), num, left);
    k_mutex_unlock(&arbitrator_mutex);
  }
  // Notify main task and delete self
  k_sem_give(&done_sem);
  k_thread_abort(k_current_get());
}

extern "C" int main(void) {

  LOG_INF("%s: ---Zephyr RTOS Dining Philosophers Challenge---",
          app_main_task_TAG.data());

  // Create kernel objects before starting tasks

  if ((!k_sem_init(&bin_sem, 0, 1)) && (!k_sem_init(&done_sem, 0, NUM_TASKS)) &&
      (!k_mutex_init(&arbitrator_mutex))) {
    LOG_INF("Task Semaphore/Mutexes inited..");
  } else {
    LOG_ERR("Failed to init Semaphore/Mutexes..");
    return 0;
  }

  for (int i = 0; i < NUM_TASKS; i++) {
    if (k_mutex_init(&chopstick[i])) {
      LOG_ERR("Failed to init chopstick Mutexes..");
      return 0;
    }
  }
  LOG_INF("All Semaphore/Mutexes inited..");

  LOG_INF("Starting Threads ...");

  // Have the philosphers start eating
  for (uint8_t i = 0; i < NUM_TASKS; i++) {
    thread_tid[i] =
        k_thread_create(&thread[i], stack_eat_thread_ptr[i],
                        K_THREAD_STACK_SIZEOF(stack_thread_0), eat, (void *)&i,
                        NULL, NULL, thread_prio[i], 0, K_NO_WAIT);

    k_sem_take(&bin_sem, K_FOREVER);
  }

  // Wait until all the philosophers are done
  for (int i = 0; i < NUM_TASKS; i++) {
    k_sem_take(&done_sem, K_FOREVER);
  }

  // Say that we made it through without deadlock
  LOG_INF("%s: Done! No deadlock occurred!", app_main_task_TAG.data());

 // k_thread_abort(k_current_get());

  while (true) {
    // Do nothing
    k_msleep(2 * LED_DELAY_DEF);
  }

  return 0;
}
