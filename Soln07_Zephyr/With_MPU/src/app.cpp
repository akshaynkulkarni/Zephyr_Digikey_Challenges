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

//Memory Domain stuff
K_HEAP_DEFINE(user_resource_pool, 256 * 5 + 128);
K_APPMEM_PARTITION_DEFINE(user_partition);
static struct k_mem_domain user_domain;

#define BUF_SIZE 5                   // Size of buffer array
USER_DATA const uint8_t num_prod_tasks = 5; // Number of producer tasks
USER_DATA const uint8_t num_cons_tasks = 2; // Number of consumer tasks
USER_DATA const uint8_t num_writes = 3; // Num times each producer writes to buf

// Globals
USER_BSS uint8_t buf[BUF_SIZE]; // Shared buffer
USER_DATA uint8_t head = 0;      // Writing index to buffer
USER_DATA uint8_t tail = 0;      // Reading index to buffer
sys_sem bin_sem;       // Waits for parameter to be read

sys_sem
    bin_sem1; // Waits for new produced buff element, in consumer task
sys_sem
    bin_sem2; // Waits for the buff element to be consumed, in producer task
sys_mutex mutex; // To access the shared buffer

// producer:
void producer(void *param1, void *param2, void *param3);
// producer:
void consumer(void *param1, void *param2, void *param3);

// Threads
#if CONFIG_BOARD_ESP
USER_DATA constexpr size_t kThreadStackSize = 4 * 1024;
#else
USER_DATA constexpr size_t kThreadStackSize = 2 * 1024;
#endif

static k_thread thread_consumer[num_cons_tasks];
static k_thread thread_producer[num_prod_tasks];


static k_tid_t thread_consumer_tid[num_cons_tasks];
static k_tid_t thread_producer_tid[num_prod_tasks];


K_THREAD_STACK_DEFINE(stack_thread_consumer0, kThreadStackSize);
K_THREAD_STACK_DEFINE(stack_thread_consumer1, kThreadStackSize);

K_THREAD_STACK_DEFINE(stack_thread_producer0, kThreadStackSize);
K_THREAD_STACK_DEFINE(stack_thread_producer1, kThreadStackSize);
K_THREAD_STACK_DEFINE(stack_thread_producer2, kThreadStackSize);
K_THREAD_STACK_DEFINE(stack_thread_producer3, kThreadStackSize);
K_THREAD_STACK_DEFINE(stack_thread_producer4, kThreadStackSize);

k_thread_stack_t *stack_thread_consumer_ptr[num_cons_tasks] = {
    stack_thread_consumer0, stack_thread_consumer1};
k_thread_stack_t *stack_thread_producer_ptr[num_prod_tasks] = {
    stack_thread_producer0, stack_thread_producer1, stack_thread_producer2,
    stack_thread_producer3, stack_thread_producer4};

USER_DATA constexpr int thread_priority = K_PRIO_PREEMPT(10);

//*****************************************************************************
// Tasks

// Producer: write a given number of times to shared buffer
void producer(void *param1, void *param2, void *param3) {

  // Copy the parameters into a local variable
  uint8_t num = *(uint8_t *)param1;

  // Release the binary semaphore
  sys_sem_give(&bin_sem);

  // Fill shared buffer with task number
  for (uint8_t i = 0; i < num_writes; i++) {
    sys_sem_take(&bin_sem2, K_FOREVER); // Empty buffer?
    // Critical section (accessing shared buffer)
    sys_mutex_lock(&mutex, K_FOREVER); // Take muex lock to access buffer
    buf[head] = num;
    head = (head + 1) % BUF_SIZE;
    sys_mutex_unlock(&mutex); // Release mutex lock of buffer
    sys_sem_give(
        &bin_sem1); // Signal the consumer that new queue item is available
  }
  // just self exit
}

// Consumer: continuously read from shared buffer
void consumer(void *param1, void *param2, void *param3) {

  uint8_t val;
  uint8_t num = *(uint8_t *)param1;
  // Release the binary semaphore
  sys_sem_give(&bin_sem);
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

void user_thread_init(void *param1, void *param2, void *param3) {

	int ret;
	struct k_mem_partition *partitions[] = {
		&user_partition
	};
	ret = k_mem_domain_init(&user_domain, ARRAY_SIZE(partitions), partitions);
	__ASSERT(ret == 0, "k_mem_domain_init failed %d", ret);
	ARG_UNUSED(ret);

    k_mem_domain_add_thread(&user_domain, k_current_get());

	k_thread_heap_assign(k_current_get(), &user_resource_pool);

  LOG_INF("Creating tasks and mutex init");
  // Create mutexes and semaphores before starting tasks
  sys_sem_init(&bin_sem, 0, 1);
  sys_sem_init(&bin_sem1, 0, num_prod_tasks); // incrementing semaphore
  sys_sem_init(&bin_sem2, num_cons_tasks,
               num_cons_tasks); // decrementing semaphore
  sys_mutex_init(&mutex);
    LOG_INF("Creating tasks and mutex init");
  // Start producer tasks (wait for each to read argument)
  for (uint8_t i = 0; i < num_prod_tasks; i++) {

    k_thread_access_grant(thread_producer_tid[i], &bin_sem, &mutex, &bin_sem2, &bin_sem1);
/*
    k_thread_access_grant(thread_producer_tid[i], &head);
    k_thread_access_grant(thread_producer_tid[i], &tail);
    k_thread_access_grant(thread_producer_tid[i], &buf);
    k_thread_access_grant(thread_producer_tid[i], &num_prod_tasks);
    k_thread_access_grant(thread_producer_tid[i], &num_cons_tasks);
    k_thread_access_grant(thread_producer_tid[i], &num_writes);
*/
    thread_producer_tid[i] =
        k_thread_create(&thread_producer[i], stack_thread_producer_ptr[i],
                        K_THREAD_STACK_SIZEOF(stack_thread_producer_ptr[i]),
                        producer, (void *)&i, nullptr, nullptr, thread_priority,
                        K_USER, K_NO_WAIT);

    sys_sem_take(&bin_sem, K_FOREVER);
  }

  for (uint8_t i = 0; i < num_cons_tasks; i++) {

    k_thread_access_grant(thread_consumer_tid[i], &bin_sem, &mutex, &bin_sem2, &bin_sem1);

    thread_consumer_tid[i] =
        k_thread_create(&thread_consumer[i], stack_thread_consumer_ptr[i],
                        K_THREAD_STACK_SIZEOF(stack_thread_consumer_ptr[i]),
                        consumer, (void *)&i, nullptr, nullptr, thread_priority,
                        K_USER, K_NO_WAIT);
    sys_sem_take(&bin_sem, K_NO_WAIT);
  }

  LOG_INF("All tasks created");
}