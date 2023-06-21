#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

class UartPolling {
private:
  using ptr_device_const = const device *;
  constexpr static ptr_device_const p_def_port =
      (DEVICE_DT_GET(DT_ALIAS(usercom0)));
  ptr_device_const m_port;

  constexpr static uart_config def_config = {.baudrate = 115200U,
                                             .parity = UART_CFG_PARITY_NONE,
                                             .stop_bits = UART_CFG_STOP_BITS_1,
                                             .data_bits = UART_CFG_DATA_BITS_8,
                                             .flow_ctrl =
                                                 UART_CFG_FLOW_CTRL_NONE};
  uart_config config;

public:
  UartPolling(const ptr_device_const &p_user_port);
  UartPolling(const ptr_device_const &p_user_port,
              const uart_config &user_config);
  int Init();
  int IsReady();
  int DeInit();
  void Write(const uint16_t &buffer);
  int Read(uint16_t &buffer);
  void Write(const unsigned char &buffer);
  int Read(unsigned char &buffer);

  ~UartPolling() = default;
};