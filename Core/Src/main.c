/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body — three-section touch UI
  *
  * Layout (landscape):
  *   +----------+---------------------+
  *   |    B     |                     |
 *   | Red Blue |       A (3×4)       |
  *   +----------+    Green grid A–L   |
  *   |    C     |                     |
  *   | Start    |                     |
  *   | Retry1   |                     |
  *   | Retry2   |                     |
  *   +----------+---------------------+
  *
  * Touching any cell/button sends its label string over USART1 (DMA).
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
#include <stdio.h>
#include <string.h>
#include "usart.h"
#include "display_ui.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define UART_FRAME_INTERVAL_MS   1000u
#define RESET_ARM_TIMEOUT_MS     1000u
#define CTRL_LOCK_DURATION_MS   10000u
#define MATRIX_BIT_COUNT           12u
#define UART_FRAME_MAX_LEN         24u  /* team + space + 3 bits + space + 12 bits + CRLF + NUL */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t  g_team_sel          = 0;      /* 0:none, 1:red(A), 2:blue(B) */
static uint16_t g_matrix_bits       = 0;      /* A..L in bits 0..11 */
static int8_t   g_ctrl_sel          = -1;     /* -1:none, 0:start, 1:retry1, 2:retry2 */
static uint32_t g_ctrl_lock_until   = 0;      /* ms tick when control section unlocks */
static uint8_t  g_reset_armed       = 0;
static uint32_t g_reset_arm_start   = 0;      /* ms tick for first reset press */
static uint32_t g_last_uart_sent_ms = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void reset_all_state(void)
{
    g_team_sel        = 0;
    g_matrix_bits     = 0;
    g_ctrl_sel        = -1;
    g_ctrl_lock_until = 0;
    g_reset_armed     = 0;
    g_reset_arm_start = 0;

    display_ui_reset_visual_state();
    display_ui_draw();
}

static uint32_t elapsed_ms(uint32_t now, uint32_t then)
{
    /* Unsigned subtraction is wrap-safe for HAL tick comparisons. */
    return now - then;
}

static void send_uart_frame(void)
{
    char team_hex = '0';
    char ctrl_bits[4];
    char matrix_bits[MATRIX_BIT_COUNT + 1];
    char frame[UART_FRAME_MAX_LEN];

    if (g_team_sel == 1)
    {
        team_hex = 'A';
    }
    else if (g_team_sel == 2)
    {
        team_hex = 'B';
    }

    ctrl_bits[0] = (g_ctrl_sel == 0) ? '1' : '0';
    ctrl_bits[1] = (g_ctrl_sel == 1) ? '1' : '0';
    ctrl_bits[2] = (g_ctrl_sel == 2) ? '1' : '0';
    ctrl_bits[3] = '\0';

    /* LSB-first mapping: bit0->A, bit1->B, ... bit11->L. */
    for (uint8_t i = 0; i < MATRIX_BIT_COUNT; i++)
    {
        matrix_bits[i] = ((g_matrix_bits >> i) & 0x1u) ? '1' : '0';
    }
    matrix_bits[MATRIX_BIT_COUNT] = '\0';

    snprintf(frame, sizeof(frame), "%c %s %s\r\n", team_hex, ctrl_bits, matrix_bits);
    printf("%s", frame);
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

  usart1_init(115200);

  tp_dev.init();

  /* Compute section layout and paint the UI */
  display_ui_init();
  display_ui_draw();
  g_last_uart_sent_ms = HAL_GetTick();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
      uint32_t now_ms = HAL_GetTick();

      if (g_ctrl_sel >= 0 && now_ms >= g_ctrl_lock_until)
      {
          g_ctrl_sel = -1;
          display_ui_set_ctrl_selection(-1);
      }

      if (elapsed_ms(now_ms, g_last_uart_sent_ms) >= UART_FRAME_INTERVAL_MS)
      {
          send_uart_frame();
          g_last_uart_sent_ms = now_ms;
      }

      if (g_reset_armed && elapsed_ms(now_ms, g_reset_arm_start) > RESET_ARM_TIMEOUT_MS)
      {
          g_reset_armed = 0;
      }

      tp_dev.scan(0);

      if (tp_dev.sta & TP_PRES_DOWN)
      {
          uint16_t      x  = tp_dev.x[0];
          uint16_t      y  = tp_dev.y[0];
          ui_touch_id_t id = display_ui_get_touch_id(x, y);

          if (id != UI_TOUCH_NONE)
          {
              now_ms = HAL_GetTick();

              if (id >= UI_TOUCH_GRID_A && id <= UI_TOUCH_GRID_L)
              {
                  uint8_t idx = (uint8_t)(id - UI_TOUCH_GRID_A);
                  uint16_t mask = (uint16_t)(1u << idx);
                  bool new_state = ((g_matrix_bits & mask) == 0u);

                  if (new_state)
                  {
                      g_matrix_bits |= mask;
                  }
                  else
                  {
                      g_matrix_bits &= (uint16_t)(~mask);
                  }

                  display_ui_set_grid_selected(idx, new_state);
              }
              else if (id == UI_TOUCH_TEAM_RED)
              {
                  if (g_team_sel == 0)
                  {
                      g_team_sel = 1;
                      display_ui_set_team_selection(1);
                  }
              }
              else if (id == UI_TOUCH_TEAM_BLUE)
              {
                  if (g_team_sel == 0)
                  {
                      g_team_sel = 2;
                      display_ui_set_team_selection(2);
                  }
              }
              else if (id >= UI_TOUCH_CTRL_START && id <= UI_TOUCH_CTRL_RETRY2)
              {
                  if (now_ms >= g_ctrl_lock_until)
                  {
                      g_ctrl_sel = (int8_t)(id - UI_TOUCH_CTRL_START);
                      g_ctrl_lock_until = now_ms + CTRL_LOCK_DURATION_MS;
                      display_ui_set_ctrl_selection(g_ctrl_sel);
                  }
              }
              else if (id == UI_TOUCH_RESET)
              {
                  if (g_reset_armed && elapsed_ms(now_ms, g_reset_arm_start) <= RESET_ARM_TIMEOUT_MS)
                  {
                      reset_all_state();
                  }
                  else
                  {
                      g_reset_armed = 1;
                      g_reset_arm_start = now_ms;
                  }
              }

              /* Wait for the finger to lift before accepting the next touch */
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
