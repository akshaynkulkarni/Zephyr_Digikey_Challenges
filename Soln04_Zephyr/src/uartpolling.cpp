#include "uartpolling.h"

UartPolling::UartPolling(const ptr_device_const &p_user_port = p_def_port,
                         const uart_config &user_config = def_config)
    : m_port(p_user_port), config(user_config) {}

UartPolling::UartPolling(const ptr_device_const &p_user_port = p_def_port)
    : m_port(p_user_port) {}

int UartPolling::Init() { return uart_configure(m_port, &config); }

int UartPolling::IsReady() { return device_is_ready(m_port); }

int UartPolling::DeInit() { return 0; }

void UartPolling::Write(const unsigned char &buffer) {
  uart_poll_out(m_port, buffer);
}

void UartPolling::Write(const uint16_t &buffer) {
  uart_poll_out_u16(m_port, buffer);
}

int UartPolling::Read(uint16_t &buffer) {
  int ret = uart_poll_in_u16(m_port, &buffer);
  if (ret) {
    // std::cout << "uart: buffer empty" << std::endl;
    buffer = '\0';
  }
  return ret;
}

int UartPolling::Read(unsigned char &buffer) {
  int ret = uart_poll_in(m_port, &buffer);
  if (ret) {
    // std::cout << "uart: buffer empty" << std::endl;
    buffer = '\0';
  }
  return ret;
}