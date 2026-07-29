/**
 ****************************************************************************************************
 * @file        usart.h
 * @brief       USART1 driver (PB6/PB7, AF7) with DMA TX support.
 ****************************************************************************************************
 */

#ifndef __USART_H
#define __USART_H

#include "stm32h7xx_hal.h"
#include <stdint.h>

extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart1_rx;

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
void usart1_send_bytes(const uint8_t *data, uint16_t len);
void usart1_start_rx_dma(void);
uint16_t usart1_rx_dma_read(uint8_t *data, uint16_t max_len);

/**
 * @brief  Count of USART1 RX errors (noise/framing/parity/overrun)
 *         recovered from since boot. Should stay at 0 on a healthy link.
 */
uint32_t usart1_get_rx_error_count(void);

/**
 * @brief  Returns non-zero when a received byte is waiting in the RX register.
 */
int usart1_recv_ready(void);

/**
 * @brief  Read one received byte (blocking until data arrives).
 */
char usart1_recv_char(void);

#endif /* __USART_H */