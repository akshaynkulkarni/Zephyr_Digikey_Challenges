
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Register the module "main" against logger */
LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

/*
 * For same module, to use log from different part of code,
 * declare it through :
 * LOG_MODULE_DECLARE(main , <LOG_LEVEL>);
 */

constexpr uint32_t delay = 1000U; // 1000 ms

extern "C" int main(void) {

  LOG_ERR("This is an example error log");
  LOG_WRN("This is an example warn log");
  LOG_INF("This is an example info log");
  LOG_DBG("This is an example debug log"); // Won't print, as log level is Info.

  while (true) {
    printk("Hello from Zephyr OS!\n");
    k_msleep(delay);
  }

  return 0;
}
