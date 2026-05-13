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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define BUTTON_X1   140
#define BUTTON_Y1   90
#define BUTTON_X2   340
#define BUTTON_Y2   170

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

uint8_t color_state = 0;

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
    /* Draw green button */
    lcd_fill(BUTTON_X1, BUTTON_Y1, BUTTON_X2, BUTTON_Y2, GREEN);

    /* Match text background to button color */
    g_back_color = GREEN;

    /* Draw text */
    lcd_show_string(175, 115, 140, 40, 24, "CLICK ME", WHITE);
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

  /* Initial screen */
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
        if ((x >= BUTTON_X1) && (x <= BUTTON_X2) &&
            (y >= BUTTON_Y1) && (y <= BUTTON_Y2))
        {
            change_background();

            /* Wait until finger released */
            while (tp_dev.sta & TP_PRES_DOWN)
            {
                tp_dev.scan(0);
                delay_ms(10);
            }
        }
    }
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