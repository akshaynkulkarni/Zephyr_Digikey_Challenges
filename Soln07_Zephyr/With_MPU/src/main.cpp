/**
 * FreeRTOS Counting Semaphore Challenge
 *
 * Code based on Digikey Challenge 7
 * (https://www.digikey.de/en/maker/projects/
 * introduction-to-rtos-solution-to-part-7-
 * freertos-semaphore-example/51aa8660524c4daba38cba7c2f5baba7)
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/sys/mutex.h>
#include <zephyr/sys/sem.h>

#include "app.h"

LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

// Threads
#if CONFIG_BOARD_ESP
constexpr size_t kThreadStackSize = 4 * 1024;
#else
constexpr size_t kThreadStackSize = 2 * 1024;
#endif

static k_thread userspace_thread;
static k_tid_t userspace_thread_tid;

K_THREAD_STACK_DEFINE(stack_userspace_thread, kThreadStackSize);

constexpr int thread_priority = K_PRIO_PREEMPT(-1);

extern "C" int main(void) {

  LOG_INF("---Zephyr RTOS Semaphore Alternate Solution---");
  // userspace threads: Create mutexes and semaphores in userspace
  LOG_INF("user space application @ %p (%zu)", (void *)user_partition.start,
          (size_t)user_partition.size);

  userspace_thread_tid = k_thread_create(
      &userspace_thread, stack_userspace_thread,
      K_THREAD_STACK_SIZEOF(stack_userspace_thread), userspace_thread_init,
      nullptr, nullptr, nullptr, thread_priority, K_INHERIT_PERMS, K_FOREVER);

  k_thread_start(userspace_thread_tid);

  k_thread_join(userspace_thread_tid, K_FOREVER);

  LOG_INF("---end of main---");
  return 0;
}
