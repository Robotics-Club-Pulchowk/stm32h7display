/**
 ****************************************************************************************************
 * @file        usart.h
 * @brief       USART1 driver using direct register access (no HAL UART module required).
 *              USART1 TX: PA9  (AF7)
 *              USART1 RX: PA10 (AF7)
 *
 *              These pins are typically routed to the on-board USB-to-UART bridge chip
 *              (e.g. CH340, CP2102) connected to one of the USB-C connectors.
 *
 *              IMPORTANT – "display turns off when opening minicom/picocom":
 *              Many STM32 development boards wire the USB-UART bridge's DTR output to
 *              the MCU's NRST pin so that IDEs can trigger a reset for firmware upload.
 *              When a terminal emulator opens the port it briefly toggles DTR, which
 *              resets the MCU and the display goes blank until the firmware restarts.
 *              Fix: open the port with DTR/RTS disabled:
 *                  picocom -b 115200 --noreset /dev/ttyUSB0
 *              or in minicom: Serial port setup → Hardware Flow Control → No.
 ****************************************************************************************************
 */

#ifndef __USART_H
#define __USART_H

#include "stm32h7xx.h"
#include <stdint.h>

/**
 * @brief  Initialise USART1 at the given baud rate (8N1, no hardware flow control).
 *         Call this after sys_stm32_clock_init() and delay_init().
 * @param  baud  Baud rate, e.g. 115200
 */
void usart1_init(uint32_t baud);

/**
 * @brief  Transmit a single character (blocking until the TX register is empty).
 */
void usart1_send_char(char c);

/**
 * @brief  Transmit a null-terminated string.
 */
void usart1_send_string(const char *str);

/**
 * @brief  Returns non-zero when a received byte is waiting in the RX register.
 */
int usart1_recv_ready(void);

/**
 * @brief  Read one received byte (blocking until data arrives).
 */
char usart1_recv_char(void);

#endif /* __USART_H */
