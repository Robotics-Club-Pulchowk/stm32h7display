/**
 ****************************************************************************************************
 * @file        usart.c
 * @brief       USART1 driver using direct register access (no HAL UART module required).
 *
 *              Pin mapping:
 *                PB6 → USART1_TX  (AF7, push-pull, high speed, pull-up)
 *                PB7 → USART1_RX  (AF7, push-pull, high speed, pull-up)
 *
 *              Clock source:
 *                USART1 lives on APB2 (rcc_pclk2).
 *                After sys_stm32_clock_init(192, 5, 2, 4):
 *                  HCLK  = 240 MHz, APB2 = HCLK/2 = 120 MHz  ← used for BRR calculation.
 ****************************************************************************************************
 */

#include "usart.h"
#include "sys.h"

/* USART1 kernel clock = rcc_pclk2 = APB2 = 120 MHz                          */
/* (D2PPRE2[2:0] = 4 → /2 divider, set in sys_clock_set inside sys.c)        */
#define USART1_CLK_HZ   120000000UL

/* -------------------------------------------------------------------------- */

void usart1_init(uint32_t baud)
{
    /* 1. Enable GPIOB peripheral clock */
    RCC->AHB4ENR |= (1U << 1);
    __DSB();    /* short barrier – let the clock enable settle before GPIO access */

    /* 2. PB6 → USART1_TX : alternate-function, push-pull, high speed, pull-up */
    sys_gpio_set(GPIOB, SYS_GPIO_PIN6,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP,
                 SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);
    sys_gpio_af_set(GPIOB, SYS_GPIO_PIN6, 7);   /* AF7 = USART1 */

    /* 3. PB7 → USART1_RX : alternate-function, push-pull, high speed, pull-up */
    sys_gpio_set(GPIOB, SYS_GPIO_PIN7,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP,
                 SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);
    sys_gpio_af_set(GPIOB, SYS_GPIO_PIN7, 7);  /* AF7 = USART1 */

    /* 4. Enable USART1 peripheral clock (APB2, bit 4) */
    RCC->APB2ENR |= (1U << 4);
    __DSB();

    /* 5. Reset all USART1 configuration registers */
    USART1->CR1 = 0;
    USART1->CR2 = 0;
    USART1->CR3 = 0;

    /* 6. Baud-rate register (OVER8=0 → 16× oversampling):
     *      BRR = fclk / baud_rate  (rounded to nearest integer)
     *      Caller must pass a non-zero baud rate.                            */
    if (baud == 0U) { return; }
    USART1->BRR = (uint32_t)((USART1_CLK_HZ + baud / 2U) / baud);

    /* 7. Enable transmitter (TE), receiver (RE), and the USART itself (UE).
     *    Frame format defaults: 8 data bits, 1 stop bit, no parity (8N1).    */
    USART1->CR1 = (1U << 3)   /* TE  – transmitter enable  */
                | (1U << 2)   /* RE  – receiver enable     */
                | (1U << 0);  /* UE  – USART enable        */
}

/* -------------------------------------------------------------------------- */

void usart1_send_char(char c)
{
    /* Wait until the transmit data register is empty (ISR bit 7 = TXE) */
    while (!(USART1->ISR & (1U << 7)));
    USART1->TDR = (uint8_t)c;
}

/* -------------------------------------------------------------------------- */

void usart1_send_string(const char *str)
{
    while (*str)
    {
        usart1_send_char(*str++);
    }
}

/* -------------------------------------------------------------------------- */

int usart1_recv_ready(void)
{
    /* RXNE (ISR bit 5) is set when a byte is waiting in the receive register */
    return (USART1->ISR & (1U << 5)) ? 1 : 0;
}

/* -------------------------------------------------------------------------- */

char usart1_recv_char(void)
{
    /* Wait until a byte has been received (RXNE) */
    while (!(USART1->ISR & (1U << 5)));
    return (char)(USART1->RDR & 0xFFU);
}

/* -------------------------------------------------------------------------- */

/**
 * @brief  Retarget printf / puts / putchar to USART1.
 *         syscalls.c declares __io_putchar as a weak symbol and calls it from
 *         the _write() syscall, so defining it here is all that is needed.
 */
int __io_putchar(int ch)
{
    /* Translate bare '\n' into '\r\n' so terminal emulators show line breaks */
    if (ch == '\n')
    {
        usart1_send_char('\r');
    }
    usart1_send_char((char)ch);
    return ch;
}
