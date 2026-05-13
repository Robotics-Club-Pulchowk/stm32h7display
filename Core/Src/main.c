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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define BUTTON_WIDTH     160
#define BUTTON_HEIGHT    80
#define BUTTON_TEXT      "CLICK ME"
#define BUTTON_FONT_SIZE 24

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

uint8_t color_state = 0;
uint16_t button_x1 = 0;
uint16_t button_y1 = 0;
uint16_t button_x2 = 0;
uint16_t button_y2 = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

void draw_interface(void);
void change_background(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief Draw UI
  */
void draw_interface(void)
{
    uint16_t btn_w = BUTTON_WIDTH;
    uint16_t btn_h = BUTTON_HEIGHT;
    uint16_t text_len = (uint16_t)strlen(BUTTON_TEXT);
    uint16_t text_w = text_len * (BUTTON_FONT_SIZE / 2);
    uint16_t text_h = BUTTON_FONT_SIZE;
    uint16_t text_x;
    uint16_t text_y;

    if (btn_w > lcddev.width) btn_w = lcddev.width;
    if (btn_h > lcddev.height) btn_h = lcddev.height;

    button_x1 = (lcddev.width - btn_w) / 2;
    button_y1 = (lcddev.height - btn_h) / 2;
    button_x2 = button_x1 + btn_w - 1;
    button_y2 = button_y1 + btn_h - 1;

    lcd_fill(button_x1, button_y1, button_x2, button_y2, GREEN);

    /* Match text background with button */
    g_back_color = GREEN;

    if (text_w > btn_w) text_w = btn_w;
    if (text_h > btn_h) text_h = btn_h;

    text_x = button_x1 + (btn_w - text_w) / 2;
    text_y = button_y1 + (btn_h - text_h) / 2;

    lcd_show_string(
        text_x,
        text_y,
        text_w,
        text_h,
        BUTTON_FONT_SIZE,
        BUTTON_TEXT,
        WHITE
    );
}

/**
  * @brief Change background color
  */
void change_background(void)
{
    color_state++;

    if (color_state >= 3)
    {
        color_state = 0;
    }

    switch (color_state)
    {
        case 0:
            lcd_clear(BLUE);
            break;

        case 1:
            lcd_clear(RED);
            break;

        case 2:
            lcd_clear(BLACK);
            break;
    }

    draw_interface();
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

  tp_dev.init();

  /* Initial screen color */
  lcd_clear(BLUE);

  /* Draw button */
  draw_interface();

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

          /* Check if touch is inside button */
          if ((x >= button_x1) && (x <= button_x2) &&
              (y >= button_y1) && (y <= button_y2))
          {
              change_background();

              /* Wait for touch release */
              while (tp_dev.sta & TP_PRES_DOWN)
              {
                  tp_dev.scan(0);
                  delay_ms(10);
              }
          }
      }

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