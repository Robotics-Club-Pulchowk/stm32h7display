/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "sys.h"
#include "delay.h"
#include "sdram.h"
#include "lcd.h"
#include "mpu.h"
#include "touch.h"
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define UART_BAUDRATE       115200U
#define PROMPT_TEXT         "click anywhere"
#define PROMPT_FONT_SIZE    32U
#define FONT_WIDTH_DIVISOR  2U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

void uart1_init(uint32_t baudrate);
int __io_putchar(int ch);
int __io_getchar(void);
void draw_touch_prompt(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void uart1_init(uint32_t baudrate)
{
    uint32_t usartdiv;

    /* USART1 TX pin: PA9 (AF7) -> connect this pin to USB-UART RX */
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

    sys_gpio_set(GPIOA, SYS_GPIO_PIN9, SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);
    sys_gpio_af_set(GPIOA, SYS_GPIO_PIN9, 7);

    USART1->CR1 = 0;
    USART1->CR2 = 0;
    USART1->CR3 = 0;

    usartdiv = (HAL_RCC_GetPCLK2Freq() + (baudrate / 2U)) / baudrate;
    USART1->BRR = usartdiv;
    USART1->CR1 |= USART_CR1_TE;
    USART1->CR1 |= USART_CR1_UE;
}

int __io_putchar(int ch)
{
    while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0U)
    {
    }

    USART1->TDR = (uint8_t)ch;
    return ch;
}

int __io_getchar(void)
{
    return -1;
}

void draw_touch_prompt(void)
{
    uint16_t text_width = (strlen(PROMPT_TEXT) * PROMPT_FONT_SIZE) / FONT_WIDTH_DIVISOR;
    uint16_t x = (lcddev.width > text_width) ? (uint16_t)((lcddev.width - text_width) / 2U) : 0U;
    uint16_t y = (lcddev.height > PROMPT_FONT_SIZE) ? (uint16_t)((lcddev.height - PROMPT_FONT_SIZE) / 2U) : 0U;

    lcd_clear(BLACK);
    g_back_color = BLACK;
    lcd_show_string(x, y, text_width, PROMPT_FONT_SIZE, PROMPT_FONT_SIZE, PROMPT_TEXT, WHITE);
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  /* USER CODE BEGIN SysInit */
  sys_stm32_clock_init(192, 5, 2, 4);
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();

  /* USER CODE BEGIN 2 */

  mpu_memory_protection();

  delay_init(480);

  sdram_init();

  lcd_init();

  uart1_init(UART_BAUDRATE);

  tp_dev.init();

  draw_touch_prompt();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
      tp_dev.scan(0);

      if (tp_dev.sta & TP_PRES_DOWN)
      {
          uint16_t x = tp_dev.x[0];
          uint16_t y = tp_dev.y[0];

          printf("touch x=%u y=%u\r\n", x, y);

          while (tp_dev.sta & TP_PRES_DOWN)
          {
              tp_dev.scan(0);
              delay_ms(10);
          }
      }

      delay_ms(10);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }

  /* USER CODE END 3 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */

  __disable_irq();

  while (1)
  {
  }

  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */

  /* USER CODE END 6 */
}

#endif /* USE_FULL_ASSERT */
