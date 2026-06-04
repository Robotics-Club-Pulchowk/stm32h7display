/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body — three-section touch UI
  *
 * Layout (landscape):
 *   +----------+---------------------+
 *   |    B     |                     |
 *   | Red Blue |      A (3×4)        |
 *   +----------+  Green grid 1–12    |
 *   |    C     |                     |
 *   |  Reset   |                     |
 *   +----------+---------------------+
 *
 * Touching any cell/button updates UI state; UART sends periodic state frames.
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
#define GRID_CELL_COUNT            12u
#define UART_FRAME_MAX_LEN         41u  /* team(2) + 12*(space+2 bits) + CRLF(2) + NUL(1) */
#define APP_MODE_TX                 0u
#define APP_MODE_RX                 1u
#define UART_RX_LINE_MAX           96u  /* fits the RX area while keeping RAM usage small */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t  g_team_sel          = 0;      /* 0:none, 1:red(A), 2:blue(B) */
static uint8_t  g_matrix_state[GRID_CELL_COUNT]; /* each cell: 0=digit, 1=AR, 2=MR, 3=FAKE */
static uint8_t  g_app_mode          = APP_MODE_TX;
static uint8_t  g_reset_armed       = 0;
static uint32_t g_reset_arm_start   = 0;      /* ms tick for first reset press */
static uint32_t g_last_uart_sent_ms = 0;
static char     g_rx_line[UART_RX_LINE_MAX];
static uint8_t  g_rx_line_len       = 0;
/*
 * UI storage index order follows on-screen scan order: [12,11,10,9,8,7,6,5,4,3,2,1].
 * g_uart_cell_order[n-1] gives the storage index used for UART transmit position n (n=1..12).
 * Example: transmit cell 1 uses index 11; transmit cell 12 uses index 0.
 */
static const uint8_t g_uart_cell_order[GRID_CELL_COUNT] = {11u, 10u, 9u, 8u, 7u, 6u, 5u, 4u, 3u, 2u, 1u, 0u}; /* 1->11,2->10,3->9,4->8,5->7,6->6,7->5,8->4,9->3,10->2,11->1,12->0 */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void reset_all_state(void)
{
    g_team_sel        = 0;
    memset(g_matrix_state, 0, sizeof(g_matrix_state));
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

static uint8_t frame_append_char(char *frame, size_t frame_len, size_t *off, char ch)
{
    if (*off >= (frame_len - 1u))
    {
        return 0u;
    }

    frame[*off] = ch;
    (*off)++;
    frame[*off] = '\0';
    return 1u;
}

static uint8_t frame_append_bits(char *frame, size_t frame_len, size_t *off, const char *bits)
{
    if (bits == NULL)
    {
        return 0u;
    }

    return (uint8_t)(frame_append_char(frame, frame_len, off, bits[0]) &&
                     frame_append_char(frame, frame_len, off, bits[1]));
}

static void send_uart_frame(void)
{
    const char *team_bits = "00";
    char frame[UART_FRAME_MAX_LEN];
    size_t off = 0;

    if (g_team_sel == 1)
    {
        team_bits = "01";
    }
    else if (g_team_sel == 2)
    {
        team_bits = "10";
    }

    frame[0] = '\0';
    if (!frame_append_bits(frame, sizeof(frame), &off, team_bits))
    {
        return;
    }

    for (uint8_t i = 0; i < GRID_CELL_COUNT; i++)
    {
        uint8_t cell_idx = g_uart_cell_order[i];
        const char *cell_bits = "00";
        uint8_t state = (uint8_t)(g_matrix_state[cell_idx] & 0x3u);

        if (state == 1u)
        {
            cell_bits = "01";
        }
        else if (state == 2u)
        {
            cell_bits = "10";
        }
        else if (state == 3u)
        {
            cell_bits = "11";
        }

        if (!frame_append_char(frame, sizeof(frame), &off, ' ') ||
            !frame_append_bits(frame, sizeof(frame), &off, cell_bits))
        {
            return;
        }
    }

    if (!frame_append_char(frame, sizeof(frame), &off, '\r') ||
        !frame_append_char(frame, sizeof(frame), &off, '\n'))
    {
        return;
    }

    printf("%s", frame);
}

static void flush_rx_line_to_ui(void)
{
    g_rx_line[g_rx_line_len] = '\0';
    display_ui_set_rx_text(g_rx_line);
}

static void process_rx_data(void)
{
    uint8_t rx_buf[32];
    uint16_t rx_len = usart1_rx_dma_read(rx_buf, sizeof(rx_buf));

    for (uint16_t i = 0; i < rx_len; i++)
    {
        char ch = (char)rx_buf[i];

        if (ch == '\r')
        {
            continue;
        }

        if (ch == '\n')
        {
            flush_rx_line_to_ui();
            g_rx_line_len = 0;
            continue;
        }

        if ((ch < 32) || (ch > 126))
        {
            continue;
        }

        if (g_rx_line_len >= (UART_RX_LINE_MAX - 1u))
        {
            flush_rx_line_to_ui();
            g_rx_line_len = 0;
        }

        g_rx_line[g_rx_line_len++] = ch;
    }
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

      if ((g_app_mode == APP_MODE_TX) &&
          (elapsed_ms(now_ms, g_last_uart_sent_ms) >= UART_FRAME_INTERVAL_MS))
      {
          send_uart_frame();
          g_last_uart_sent_ms = now_ms;
      }

      if (g_app_mode == APP_MODE_RX)
      {
          process_rx_data();
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

              if (id == UI_TOUCH_MODE_TOGGLE)
              {
                  g_app_mode = (g_app_mode == APP_MODE_TX) ? APP_MODE_RX : APP_MODE_TX;
                  g_reset_armed = 0;
                  g_last_uart_sent_ms = now_ms;
                  g_rx_line_len = 0;
                  g_rx_line[0] = '\0';
                  display_ui_set_mode(g_app_mode);
                  if (g_app_mode == APP_MODE_RX)
                  {
                      display_ui_set_rx_text("Waiting for UART data...");
                  }
              }
              else if ((g_app_mode == APP_MODE_TX) && (id >= UI_TOUCH_GRID_A && id <= UI_TOUCH_GRID_L))
              {
                  uint8_t idx = (uint8_t)(id - UI_TOUCH_GRID_A);
                  g_matrix_state[idx] = (uint8_t)((g_matrix_state[idx] + 1u) & 0x3u);
                  display_ui_set_grid_state(idx, g_matrix_state[idx]);
              }
              else if ((g_app_mode == APP_MODE_TX) && (id == UI_TOUCH_TEAM_RED))
              {
                  if (g_team_sel == 0)
                  {
                      g_team_sel = 1;
                      display_ui_set_team_selection(1);
                  }
              }
              else if ((g_app_mode == APP_MODE_TX) && (id == UI_TOUCH_TEAM_BLUE))
              {
                  if (g_team_sel == 0)
                  {
                      g_team_sel = 2;
                      display_ui_set_team_selection(2);
                  }
              }
              else if ((g_app_mode == APP_MODE_TX) && (id == UI_TOUCH_RESET))
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
