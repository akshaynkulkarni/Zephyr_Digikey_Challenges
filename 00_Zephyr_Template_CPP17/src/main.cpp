
#include <zephyr/kernel.h>

constexpr uint32_t delay = 1000U; // 1000 ms

extern "C" int main(void) {

  while (true) {
    printk("Hello from Zephyr OS!\n");
    k_msleep(delay);
  }

  return 0;
}
