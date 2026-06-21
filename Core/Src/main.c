/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body — three-section touch UI
  *
 * Layout (landscape):
 *   +-----------+---------------------+
 *   |     B     |                     |
 *   | Team Mode |      A (3×4)        |
 *   +-----------+  Green grid 1–12    |
 *   |     C     |  (cells recolor to  |
 *   |   Reset   |   AR/MR/FAKE)       |
 *   +-----------+---------------------+
 *
 * Section B: left button toggles Red/Blue team (locks after first touch
 * until Reset); right button (the "scroll" button) cycles AR -> MR -> FAKE
 * and sets which state any touched Section-A cell will take on.
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
#define UART_FRAME_MAX_LEN         42u  /* team(1) + space(1) + screen_cam(1) + 12*(space+2 bits) + CRLF(2) + NUL(1) */
#define UART_RX_LINE_MAX           96u  /* includes NUL terminator; max payload is 95 bytes */
#define UART_RX_PAYLOAD_MAX        (UART_RX_LINE_MAX - 1u)

/* Maximum number of Section-A cells allowed to hold each mode at once.
 * A tap that would exceed a mode's limit is ignored; free a cell of that
 * mode first (revert it via double-tap, or change it to another mode). */
#define MAX_AR_CELLS               4u
#define MAX_MR_CELLS               3u
#define MAX_FAKE_CELLS             1u

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint8_t  g_team_sel          = DISPLAY_UI_TEAM_RED; /* 0:red (default), 1:blue */
static uint8_t  g_team_locked       = 0;      /* set on first touch of the team button; cleared on reset */
static uint8_t  g_scroll_mode       = DISPLAY_UI_SCROLL_AR; /* 1:AR, 2:MR, 3:FAKE — applied to grid cells on touch */
static uint8_t  g_screen_cam_sel    = 0;      /* 0:camera (default), 1:screen */
static uint8_t  g_matrix_state[GRID_CELL_COUNT]; /* each cell: 0=digit, 1=AR, 2=MR, 3=FAKE */
static uint8_t  g_app_mode          = DISPLAY_UI_MODE_TX;
static uint8_t  g_reset_armed       = 0;
static uint32_t g_reset_arm_start   = 0;      /* ms tick for first reset press */
static uint8_t  g_cell_armed[GRID_CELL_COUNT];       /* per-cell double-tap arm flag */
static uint32_t g_cell_arm_start[GRID_CELL_COUNT];   /* ms tick for first tap on that cell */
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
    g_team_sel        = DISPLAY_UI_TEAM_RED;
    g_team_locked     = 0;
    g_scroll_mode     = DISPLAY_UI_SCROLL_AR;
    g_screen_cam_sel  = 0;
    memset(g_matrix_state, 0, sizeof(g_matrix_state));
    memset(g_cell_armed, 0, sizeof(g_cell_armed));
    memset(g_cell_arm_start, 0, sizeof(g_cell_arm_start));
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

static uint8_t count_cells_in_mode(uint8_t mode)
{
    uint8_t count = 0;

    for (uint8_t i = 0; i < GRID_CELL_COUNT; i++)
    {
        if (g_matrix_state[i] == mode)
        {
            count++;
        }
    }

    return count;
}

static uint8_t mode_limit(uint8_t mode)
{
    if (mode == DISPLAY_UI_SCROLL_AR)   return MAX_AR_CELLS;
    if (mode == DISPLAY_UI_SCROLL_MR)   return MAX_MR_CELLS;
    if (mode == DISPLAY_UI_SCROLL_FAKE) return MAX_FAKE_CELLS;
    return 0u;
}

static uint8_t mode_slot_available(uint8_t mode)
{
    return (count_cells_in_mode(mode) < mode_limit(mode)) ? 1u : 0u;
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
    char frame[UART_FRAME_MAX_LEN];
    size_t off = 0;
    char team_bit = (g_team_sel == DISPLAY_UI_TEAM_BLUE) ? '1' : '0';

    frame[0] = '\0';
    if (!frame_append_char(frame, sizeof(frame), &off, team_bit))
    {
        return;
    }

    if (!frame_append_char(frame, sizeof(frame), &off, ' ') ||
        !frame_append_char(frame, sizeof(frame), &off, g_screen_cam_sel ? '1' : '0'))
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

        if (g_rx_line_len >= UART_RX_PAYLOAD_MAX)
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
  printf("hello\n");
  g_last_uart_sent_ms = HAL_GetTick();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
      uint32_t now_ms = HAL_GetTick();

      if ((g_app_mode == DISPLAY_UI_MODE_TX) &&
          (elapsed_ms(now_ms, g_last_uart_sent_ms) >= UART_FRAME_INTERVAL_MS))
      {
          send_uart_frame();
          g_last_uart_sent_ms = now_ms;
      }

      if (g_app_mode == DISPLAY_UI_MODE_RX)
      {
          process_rx_data();
      }

      if (g_reset_armed && elapsed_ms(now_ms, g_reset_arm_start) > RESET_ARM_TIMEOUT_MS)
      {
          g_reset_armed = 0;
      }

      for (uint8_t ci = 0; ci < GRID_CELL_COUNT; ci++)
      {
          if (g_cell_armed[ci] && elapsed_ms(now_ms, g_cell_arm_start[ci]) > RESET_ARM_TIMEOUT_MS)
          {
              g_cell_armed[ci] = 0;
          }
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
                  g_app_mode = (g_app_mode == DISPLAY_UI_MODE_TX) ? DISPLAY_UI_MODE_RX : DISPLAY_UI_MODE_TX;
                  g_reset_armed = 0;
                  g_last_uart_sent_ms = now_ms;
                  g_rx_line_len = 0;
                  g_rx_line[0] = '\0';
                  display_ui_set_mode(g_app_mode);
                  if (g_app_mode == DISPLAY_UI_MODE_RX)
                  {
                      display_ui_set_rx_text("Waiting for UART data...");
                  }
              }
              else if ((g_app_mode == DISPLAY_UI_MODE_TX) && (id >= UI_TOUCH_GRID_A && id <= UI_TOUCH_GRID_L))
              {
                  uint8_t idx = (uint8_t)(id - UI_TOUCH_GRID_A);

                  if (g_matrix_state[idx] != 0u)
                  {
                      /* Cell already AR/MR/FAKE: a quick second tap reverts it to its number;
                       * otherwise this tap just arms the revert window. */
                      if (g_cell_armed[idx] && elapsed_ms(now_ms, g_cell_arm_start[idx]) <= RESET_ARM_TIMEOUT_MS)
                      {
                          g_matrix_state[idx] = 0u;
                          g_cell_armed[idx] = 0;
                          display_ui_set_grid_state(idx, g_matrix_state[idx]);
                      }
                      else
                      {
                          g_cell_armed[idx] = 1;
                          g_cell_arm_start[idx] = now_ms;
                      }
                  }
                  else
                  {
                      /* Plain numbered cell: a single tap sets it to the current scroll mode,
                       * but only if that mode still has a free slot. */
                      if (mode_slot_available(g_scroll_mode))
                      {
                          g_matrix_state[idx] = g_scroll_mode;
                          g_cell_armed[idx] = 0;
                          display_ui_set_grid_state(idx, g_matrix_state[idx]);
                      }
                  }
              }
              else if ((g_app_mode == DISPLAY_UI_MODE_TX) && (id == UI_TOUCH_TEAM_TOGGLE))
              {
                  if (!g_team_locked)
                  {
                      g_team_sel = (g_team_sel == DISPLAY_UI_TEAM_RED) ? DISPLAY_UI_TEAM_BLUE : DISPLAY_UI_TEAM_RED;
                      g_team_locked = 1;
                      display_ui_set_team_selection(g_team_sel);
                  }
              }
              else if ((g_app_mode == DISPLAY_UI_MODE_TX) && (id == UI_TOUCH_SCROLL_MODE))
              {
                  if (g_scroll_mode == DISPLAY_UI_SCROLL_AR)
                  {
                      g_scroll_mode = DISPLAY_UI_SCROLL_MR;
                  }
                  else if (g_scroll_mode == DISPLAY_UI_SCROLL_MR)
                  {
                      g_scroll_mode = DISPLAY_UI_SCROLL_FAKE;
                  }
                  else
                  {
                      g_scroll_mode = DISPLAY_UI_SCROLL_AR;
                  }
                  display_ui_set_scroll_mode(g_scroll_mode);
              }
              else if ((g_app_mode == DISPLAY_UI_MODE_TX) && (id == UI_TOUCH_SCREEN_CAM))
              {
                  g_screen_cam_sel = (uint8_t)(g_screen_cam_sel ^ 1u);
                  display_ui_set_screen_cam_selection(g_screen_cam_sel);
              }
              else if ((g_app_mode == DISPLAY_UI_MODE_TX) && (id == UI_TOUCH_RESET))
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