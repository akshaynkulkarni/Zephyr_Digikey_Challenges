#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/sys/mutex.h>
#include <zephyr/sys/sem.h>

#include "app.h"

#define LED_DELAY1_DEF (300U)
#define LED_DELAY2_DEF (500U)
#define LED_DELAY3_DEF (234U)
#define UART_DELAY (100U)

LOG_MODULE_REGISTER(user_space, CONFIG_LOG_DEFAULT_LEVEL);

// Memory Domain stuff
K_HEAP_DEFINE(user_resource_pool,
              10 * 1024); // pool of resouces allocated by kernel , syscalls

K_APPMEM_PARTITION_DEFINE(
    user_partition); // all the user space threads and objects live here

static struct k_mem_domain user_domain;

#define BUF_SIZE 5                          // Size of buffer array
USER_DATA const uint8_t num_prod_tasks = 5; // Number of producer tasks
USER_DATA const uint8_t num_cons_tasks = 2; // Number of consumer tasks
USER_DATA const uint8_t num_writes = 3; // Num times each producer writes to buf

// Userspace Application Globals
USER_BSS uint8_t buf[BUF_SIZE];                     // Shared buffer
USER_DATA uint8_t head = 0;                         // Writing index to buffer
USER_DATA uint8_t tail = 0;                         // Reading index to buffer
USER_BSS int8_t parameter1, parameter2, parameter3; // send params to tasks
USER_BSS sys_sem bin_sem; // Waits for parameter to be read

USER_BSS sys_sem
    bin_sem1; // Waits for new produced buff element, in consumer task
USER_BSS sys_sem
    bin_sem2; // Waits for the buff element to be consumed, in producer task
USER_BSS sys_mutex mutex; // To access the shared buffer

// producer:
static void app_thread_producer(void *param1, void *param2, void *param3);
// consumer:
static void app_thread_consumer(void *param1, void *param2, void *param3);

// Threads
#if CONFIG_BOARD_ESP
constexpr size_t kThreadStackSize = 4 * 1024;
#else
constexpr size_t kThreadStackSize = 2 * 1024;
#endif

// TIDs
static k_tid_t app_consumer_thread_tid[num_cons_tasks];
static k_tid_t app_producer_thread_tid[num_prod_tasks];
static k_tid_t app_init_thread_tid;

// TCBs
static k_thread app_init_thread;
static k_thread app_consumer_thread[num_cons_tasks];
static k_thread app_producer_thread[num_prod_tasks];

// Thread Stacks

K_THREAD_STACK_DEFINE(stack_app_init_thread, kThreadStackSize);

K_THREAD_STACK_DEFINE(stack_app_consumer_thread0, kThreadStackSize);
K_THREAD_STACK_DEFINE(stack_app_consumer_thread1, kThreadStackSize);

K_THREAD_STACK_DEFINE(stack_app_producer_thread0, kThreadStackSize);
K_THREAD_STACK_DEFINE(stack_app_producer_thread1, kThreadStackSize);
K_THREAD_STACK_DEFINE(stack_app_producer_thread2, kThreadStackSize);
K_THREAD_STACK_DEFINE(stack_app_producer_thread3, kThreadStackSize);
K_THREAD_STACK_DEFINE(stack_app_producer_thread4, kThreadStackSize);

constexpr static int thread_priority = K_PRIO_PREEMPT(-1);

// Producer: write a given number of times to shared buffer
static void app_thread_producer(void *param1, void *param2, void *param3) {

  ARG_UNUSED(param2);
  ARG_UNUSED(param3);

  // Copy the parameters into a local variable
  uint8_t num = *(uint8_t *)param1;
  // Release the binary semaphore
  sys_sem_give(&bin_sem);

  // Fill shared buffer with task number
  for (uint8_t i = 0; i < num_writes; i++) {
    sys_sem_take(&bin_sem2, K_FOREVER); // Empty buffer?
    // Critical section (accessing shared buffer)
    sys_mutex_lock(&mutex, K_FOREVER); // Take muex lock to access buffer
    LOG_INF("producer thread[%d] %d", num, num);
    buf[head] = num;
    head = (head + 1) % BUF_SIZE;
    sys_mutex_unlock(&mutex); // Release mutex lock of buffer
    sys_sem_give(
        &bin_sem1); // Signal the consumer that new queue item is available
  }
}

// Consumer: continuously read from shared buffer
static void app_thread_consumer(void *param1, void *param2, void *param3) {

  ARG_UNUSED(param2);
  ARG_UNUSED(param3);

  uint8_t val = 0;
  uint8_t num = *(uint8_t *)param1;
  // Release the binary semaphore
  // sys_sem_give(&bin_sem);
  // Read from buffer
  while (1) {

    // Critical section (accessing shared buffer and Serial)
    sys_sem_take(&bin_sem1,
                 K_FOREVER); // wait till new buff is produced in the queue

    sys_mutex_lock(&mutex,
                   K_FOREVER); // mutex to access the shared buff: acquire lock

    val = buf[tail];
    tail = (tail + 1) % BUF_SIZE;
    LOG_INF("consumer thread[%d] %d", num, val);

    sys_mutex_unlock(&mutex); // mutex to access the shared buff: release lock
    sys_sem_give(&bin_sem2);  // signal the buff value consumption
    k_msleep(1);
  }
}

USER_DATA k_thread_stack_t *stack_app_consumer_thread_ptr[] = {
    stack_app_consumer_thread0, stack_app_consumer_thread1};

USER_DATA k_thread_stack_t *stack_app_producer_thread_ptr[] = {
    stack_app_producer_thread0, stack_app_producer_thread1,
    stack_app_producer_thread2, stack_app_producer_thread3,
    stack_app_producer_thread4};

static void app_thread_init(void *param1, void *param2, void *param3) {
  ARG_UNUSED(param1);
  ARG_UNUSED(param2);
  ARG_UNUSED(param3);
  LOG_INF("Semaphore and mutex init...");
  // Create mutexes and semaphores before starting tasks

  sys_sem_init(&bin_sem, 0, 1);
  sys_sem_init(&bin_sem1, 0, num_prod_tasks); // incrementing semaphore
  sys_sem_init(&bin_sem2, num_cons_tasks,
               num_cons_tasks); // decrementing semaphore
  sys_mutex_init(&mutex);
}

// Entry Point(EP) to userspace: create all the user space threads in this
// thread that are part of this application: producer, consumer threads. This
// thread runs in privileged mode
void userspace_thread_init(void *param1, void *param2, void *param3) {

  struct k_mem_partition *partitions[] = {&user_partition};
  int ret = k_mem_domain_init(&user_domain, ARRAY_SIZE(partitions), partitions);

  __ASSERT(ret == 0, "k_mem_domain_init failed %d", ret);

  // Add this thread to user memory domain
  k_mem_domain_add_thread(&user_domain, k_current_get());

  // k_thread_heap_assign(k_current_get(), &user_resource_pool);
  // k_thread_access_grant(k_current_get(), &bin_sem, &mutex, &bin_sem2,
  // &bin_sem1);

  app_init_thread_tid = k_thread_create(
      &app_init_thread, stack_app_init_thread,
      K_THREAD_STACK_SIZEOF(stack_app_init_thread), app_thread_init, nullptr,
      nullptr, nullptr, thread_priority, K_USER, K_FOREVER);

  //  k_thread_access_grant(&app_init_thread, &bin_sem, &mutex, &bin_sem2,
  //  &bin_sem1);
  LOG_INF("Starting app_init_thread....");
  k_thread_start(&app_init_thread);

  k_msleep(50); // to avoid LOGs dropping
  // Wait untill all semaphore and mutex init is complete
  k_thread_join(app_init_thread_tid, K_FOREVER);

  LOG_INF("Starting app_thread_producer(s)....");
  // Start producer tasks (wait for each to read argument)
  for (uint8_t i = 0; i < num_prod_tasks; i++) {

    parameter1 = i;
    app_producer_thread_tid[i] = k_thread_create(
        &app_producer_thread[i], stack_app_producer_thread_ptr[i],
        K_THREAD_STACK_SIZEOF(stack_app_producer_thread0), app_thread_producer,
        (void *)&parameter1, nullptr, nullptr, thread_priority, K_USER,
        K_FOREVER);
    // k_thread_access_grant(&app_producer_thread[i], &bin_sem, &mutex,
    // &bin_sem2, &bin_sem1);
    k_thread_start(app_producer_thread_tid[i]);
    sys_sem_take(&bin_sem, K_FOREVER);
  }
  // k_msleep(1);
  LOG_INF(" Starting app_thread_consumer(s)....");
  for (uint8_t i = 0; i < num_cons_tasks; i++) {
    parameter1 = i;
    app_consumer_thread_tid[i] = k_thread_create(
        &app_consumer_thread[i], stack_app_consumer_thread_ptr[i],
        K_THREAD_STACK_SIZEOF(stack_app_consumer_thread0), app_thread_consumer,
        (void *)&parameter1, nullptr, nullptr, thread_priority, K_USER,
        K_FOREVER);
    // k_thread_access_grant(&app_consumer_thread[i], &bin_sem, &mutex,
    // &bin_sem2, &bin_sem1);
    k_thread_start(app_consumer_thread_tid[i]);
    sys_sem_take(&bin_sem, K_NO_WAIT);
    // k_msleep(100);
  }

  LOG_INF("All tasks created");
  k_thread_join(app_consumer_thread_tid[num_cons_tasks - 1], K_FOREVER);
  // end of thread...
}