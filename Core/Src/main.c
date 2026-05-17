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
#include "usart.h"

/* USER CODE BEGIN Includes */
#include "sys.h"
#include "delay.h"
#include "sdram.h"
#include "lcd.h"
#include "mpu.h"
#include "touch.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define BUTTON_GAP       24
#define BUTTON_MARGIN    20
#define BUTTON_FONT_SIZE 24
#define BUTTON_COUNT     4
#define BUTTON_INDEX_NONE 0xFF

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

typedef struct
{
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    uint16_t color;
    const char *label;
} color_button_t;

color_button_t color_buttons[BUTTON_COUNT] =
{
    {0, 0, 0, 0, RED,   "RED"},
    {0, 0, 0, 0, GREEN, "GREEN"},
    {0, 0, 0, 0, BLUE,  "BLUE"},
    {0, 0, 0, 0, BLACK, "BLACK"}
};

static volatile uint8_t g_uart_tx_ready = 1;
static const uint8_t g_uart_heartbeat[] = "UART DMA heartbeat\r\n";

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

void draw_interface(void);
void set_background_color(uint16_t color);
uint8_t get_touched_button_index(uint16_t x, uint16_t y);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief Draw UI
  */
void draw_interface(void)
{
    uint16_t matrix_w;
    uint16_t matrix_h;
    uint16_t start_x;
    uint16_t start_y;
    uint16_t button_size;
    uint8_t i;

    uint16_t max_button_w = (lcddev.width - (2 * BUTTON_MARGIN) - BUTTON_GAP) / 2;
    uint16_t max_button_h = (lcddev.height - (2 * BUTTON_MARGIN) - BUTTON_GAP) / 2;

    button_size = (max_button_w < max_button_h) ? max_button_w : max_button_h;
    if (button_size == 0)
    {
        return;
    }

    matrix_w = (2 * button_size) + BUTTON_GAP;
    matrix_h = (2 * button_size) + BUTTON_GAP;
    start_x = (lcddev.width - matrix_w) / 2;
    start_y = (lcddev.height - matrix_h) / 2;

    color_buttons[0].x1 = start_x;
    color_buttons[0].y1 = start_y;
    color_buttons[0].x2 = start_x + button_size - 1;
    color_buttons[0].y2 = start_y + button_size - 1;

    color_buttons[1].x1 = start_x + button_size + BUTTON_GAP;
    color_buttons[1].y1 = start_y;
    color_buttons[1].x2 = color_buttons[1].x1 + button_size - 1;
    color_buttons[1].y2 = start_y + button_size - 1;

    color_buttons[2].x1 = start_x;
    color_buttons[2].y1 = start_y + button_size + BUTTON_GAP;
    color_buttons[2].x2 = start_x + button_size - 1;
    color_buttons[2].y2 = color_buttons[2].y1 + button_size - 1;

    color_buttons[3].x1 = start_x + button_size + BUTTON_GAP;
    color_buttons[3].y1 = start_y + button_size + BUTTON_GAP;
    color_buttons[3].x2 = color_buttons[3].x1 + button_size - 1;
    color_buttons[3].y2 = color_buttons[3].y1 + button_size - 1;

    for (i = 0; i < BUTTON_COUNT; i++)
    {
        uint16_t label_len;
        uint16_t text_w;
        uint16_t text_h;
        uint16_t text_x;
        uint16_t text_y;

        label_len = (uint16_t)strlen(color_buttons[i].label);
        text_w = label_len * (BUTTON_FONT_SIZE / 2);
        text_h = BUTTON_FONT_SIZE;
        if (text_w > button_size) text_w = button_size;
        if (text_h > button_size) text_h = button_size;
        text_x = color_buttons[i].x1 + ((button_size - text_w) / 2);
        text_y = color_buttons[i].y1 + ((button_size - text_h) / 2);

        lcd_fill(
            color_buttons[i].x1,
            color_buttons[i].y1,
            color_buttons[i].x2,
            color_buttons[i].y2,
            color_buttons[i].color
        );

        g_back_color = color_buttons[i].color;
        lcd_show_string(text_x, text_y, text_w, text_h, BUTTON_FONT_SIZE, color_buttons[i].label, WHITE);
    }
}

/**
  * @brief Set screen background and redraw color buttons
  */
void set_background_color(uint16_t color)
{
    lcd_clear(color);
    draw_interface();
}

/**
  * @brief Get touched button index
  */
uint8_t get_touched_button_index(uint16_t x, uint16_t y)
{
    uint8_t i;

    for (i = 0; i < BUTTON_COUNT; i++)
    {
        if ((x >= color_buttons[i].x1) && (x <= color_buttons[i].x2) &&
            (y >= color_buttons[i].y1) && (y <= color_buttons[i].y2))
        {
            return i;
        }
    }

    return BUTTON_INDEX_NONE;
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
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */

  mpu_memory_protection();

  delay_init(480);

  sdram_init();

  lcd_init();

  tp_dev.init();

  /* Initial screen color */
  set_background_color(BLUE);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
      uint32_t now = HAL_GetTick();
      static uint32_t last_uart_tick = 0;

      tp_dev.scan(0);

      if (tp_dev.sta & TP_PRES_DOWN)
      {
          uint16_t x = tp_dev.x[0];
          uint16_t y = tp_dev.y[0];

          uint8_t button_index = get_touched_button_index(x, y);

          if (button_index != BUTTON_INDEX_NONE)
          {
              set_background_color(color_buttons[button_index].color);

              /* Wait for touch release */
              while (tp_dev.sta & TP_PRES_DOWN)
              {
                  tp_dev.scan(0);
                  delay_ms(10);
              }
          }
      }

      if (g_uart_tx_ready && ((now - last_uart_tick) >= 1000U))
      {
          if (HAL_UART_Transmit_DMA(&huart1, (uint8_t *)g_uart_heartbeat, sizeof(g_uart_heartbeat) - 1U) == HAL_OK)
          {
              g_uart_tx_ready = 0;
              last_uart_tick = now;
          }
      }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }

  /* USER CODE END 3 */
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        g_uart_tx_ready = 1;
    }
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
