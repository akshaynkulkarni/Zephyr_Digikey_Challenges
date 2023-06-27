#ifndef APP_H
#define APP_H

#include <zephyr/kernel.h>
#include <zephyr/app_memory/app_memdomain.h>

void user_thread_init(void *p1, void *p2, void *p3);

extern struct k_mem_partition user_partition;
#define USER_DATA	K_APP_DMEM(user_partition)
#define USER_BSS	K_APP_BMEM(user_partition)

#endif /* APP_H */