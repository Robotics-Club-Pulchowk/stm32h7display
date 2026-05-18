/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body — three-section touch UI
  *
  * Layout (landscape):
  *   +----------+---------------------+
  *   |    B     |                     |
  *   | Red Blue |       A (4×3)       |
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
#include <string.h>
#include <stdio.h>
#include "usart.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* One cell in the Section-A grid */
typedef struct
{
    uint16_t x1, y1, x2, y2;
    char     label[4];   /* up to 3 chars + NUL */
} grid_cell_t;

/* Generic labelled button used for sections B and C */
typedef struct
{
    uint16_t    x1, y1, x2, y2;
    uint32_t    fill_color;
    uint32_t    text_color;
    const char *label;
} button_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* Appearance */
#define BORDER_W    2    /* width of the white section-divider lines (px) */
#define CELL_PAD    3    /* black gap around each cell / button (px) */
#define FONT_SIZE   24   /* character height used throughout */

/* Section-A grid dimensions */
#define GRID_COLS   4
#define GRID_ROWS   3
#define GRID_CELLS  (GRID_COLS * GRID_ROWS)   /* 12 */

/* Section-B / C button counts */
#define SEC_B_CNT   2
#define SEC_C_CNT   3

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

static grid_cell_t grid[GRID_CELLS];
static button_t    sec_b[SEC_B_CNT];
static button_t    sec_c[SEC_C_CNT];

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

static void        init_layout(void);
static void        draw_interface(void);
static void        draw_cell(const grid_cell_t *c);
static void        draw_button(const button_t *b);
static const char *get_touch_msg(uint16_t x, uint16_t y);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
  * @brief  Pre-compute pixel coordinates for all cells and buttons.
  *         Must be called after lcd_init() so lcddev.width/height are valid.
  */
static void init_layout(void)
{
    uint16_t w    = lcddev.width;
    uint16_t h    = lcddev.height;
    uint16_t midx = (uint16_t)(w / 2);
    uint16_t midy = (uint16_t)(h / 2);

    /* ── Section A  (right half) ──────────────────────────────────────── */
    uint16_t ax1 = (uint16_t)(midx + BORDER_W);
    uint16_t aw  = (uint16_t)(w - ax1);
    uint16_t ah  = h;

    uint16_t cw  = (uint16_t)(aw / GRID_COLS);
    uint16_t ch  = (uint16_t)(ah / GRID_ROWS);

    for (uint8_t r = 0; r < GRID_ROWS; r++)
    {
        for (uint8_t c = 0; c < GRID_COLS; c++)
        {
            uint8_t idx = (uint8_t)(r * GRID_COLS + c);

            grid[idx].x1 = (uint16_t)(ax1 + c * cw       + CELL_PAD);
            grid[idx].y1 = (uint16_t)(      r * ch        + CELL_PAD);
            grid[idx].x2 = (uint16_t)(ax1 + (c + 1) * cw - 1 - CELL_PAD);
            grid[idx].y2 = (uint16_t)(      (r + 1) * ch  - 1 - CELL_PAD);

            grid[idx].label[0] = (char)('A' + idx);
            grid[idx].label[1] = '\0';
        }
    }

    /* ── Section B  (top-left quarter) ───────────────────────────────── */
    /*
     * Two square buttons placed side by side, centred vertically.
     * Square side = min(half_width, quarter_height) - 2*CELL_PAD
     */
    uint16_t bw     = midx;
    uint16_t bh     = midy;
    uint16_t half_w = (uint16_t)(bw / 2);
    uint16_t sq     = (uint16_t)((half_w < bh ? half_w : bh) - 2 * CELL_PAD);
    uint16_t bvy    = (uint16_t)((bh - sq) / 2);  /* vertical offset to centre */

    sec_b[0].x1         = (uint16_t)(0 + CELL_PAD);
    sec_b[0].y1         = bvy;
    sec_b[0].x2         = (uint16_t)(half_w - 1 - CELL_PAD);
    sec_b[0].y2         = (uint16_t)(bvy + sq - 1);
    sec_b[0].fill_color = RED;
    sec_b[0].text_color = WHITE;
    sec_b[0].label      = "Red";

    sec_b[1].x1         = (uint16_t)(half_w + CELL_PAD);
    sec_b[1].y1         = bvy;
    sec_b[1].x2         = (uint16_t)(bw - 1 - CELL_PAD);
    sec_b[1].y2         = (uint16_t)(bvy + sq - 1);
    sec_b[1].fill_color = BLUE;
    sec_b[1].text_color = WHITE;
    sec_b[1].label      = "Blue";

    /* ── Section C  (bottom-left quarter) ────────────────────────────── */
    /*
     * Three equal-height buttons stacked vertically.
     * Colors chosen to be clearly distinct from red/green/blue.
     */
    uint16_t cy1 = (uint16_t)(midy + BORDER_W);
    uint16_t cw2 = midx;
    uint16_t ch2 = (uint16_t)(h - cy1);
    uint16_t gap = (uint16_t)(2 * CELL_PAD);

    /* Divide available height into SEC_C_CNT equal slots */
    uint16_t slot_h = (uint16_t)(ch2 / SEC_C_CNT);
    uint16_t btn_h  = (uint16_t)(slot_h - gap);

    static const uint32_t c_fill[SEC_C_CNT] = { YELLOW,  MAGENTA, CYAN  };
    static const uint32_t c_text[SEC_C_CNT] = { BLACK,   WHITE,   BLACK };
    static const char    *c_lbl [SEC_C_CNT] = { "Start", "Retry1", "Retry2" };

    for (uint8_t i = 0; i < SEC_C_CNT; i++)
    {
        sec_c[i].x1         = (uint16_t)(CELL_PAD);
        sec_c[i].y1         = (uint16_t)(cy1 + i * slot_h + CELL_PAD);
        sec_c[i].x2         = (uint16_t)(cw2 - 1 - CELL_PAD);
        sec_c[i].y2         = (uint16_t)(sec_c[i].y1 + btn_h - 1);
        sec_c[i].fill_color = c_fill[i];
        sec_c[i].text_color = c_text[i];
        sec_c[i].label      = c_lbl[i];
    }
}

/**
  * @brief  Draw a single Section-A grid cell (green, white label).
  */
static void draw_cell(const grid_cell_t *c)
{
    uint16_t bw     = (uint16_t)(c->x2 - c->x1 + 1);
    uint16_t bh     = (uint16_t)(c->y2 - c->y1 + 1);
    uint16_t llen   = (uint16_t)strlen(c->label);
    uint16_t text_w = (uint16_t)(llen * (FONT_SIZE / 2));
    uint16_t text_h = FONT_SIZE;

    if (text_w > bw) text_w = bw;
    if (text_h > bh) text_h = bh;

    uint16_t tx = (uint16_t)(c->x1 + (bw - text_w) / 2);
    uint16_t ty = (uint16_t)(c->y1 + (bh - text_h) / 2);

    lcd_fill(c->x1, c->y1, c->x2, c->y2, GREEN);
    g_back_color = GREEN;
    lcd_show_string(tx, ty, text_w, text_h, FONT_SIZE, (char *)c->label, WHITE);
}

/**
  * @brief  Draw a single Section-B or Section-C button.
  */
static void draw_button(const button_t *b)
{
    uint16_t bw     = (uint16_t)(b->x2 - b->x1 + 1);
    uint16_t bh     = (uint16_t)(b->y2 - b->y1 + 1);
    uint16_t llen   = (uint16_t)strlen(b->label);
    uint16_t text_w = (uint16_t)(llen * (FONT_SIZE / 2));
    uint16_t text_h = FONT_SIZE;

    if (text_w > bw) text_w = bw;
    if (text_h > bh) text_h = bh;

    uint16_t tx = (uint16_t)(b->x1 + (bw - text_w) / 2);
    uint16_t ty = (uint16_t)(b->y1 + (bh - text_h) / 2);

    lcd_fill(b->x1, b->y1, b->x2, b->y2, b->fill_color);
    g_back_color = b->fill_color;
    lcd_show_string(tx, ty, text_w, text_h, FONT_SIZE, (char *)b->label, b->text_color);
}

/**
  * @brief  Paint the complete three-section UI on a black background.
  */
static void draw_interface(void)
{
    uint16_t w    = lcddev.width;
    uint16_t h    = lcddev.height;
    uint16_t midx = (uint16_t)(w / 2);
    uint16_t midy = (uint16_t)(h / 2);

    /* Black background */
    lcd_clear(BLACK);

    /* White vertical border separating left and right halves */
    lcd_fill(midx, 0,
             (uint16_t)(midx + BORDER_W - 1),
             (uint16_t)(h - 1),
             WHITE);

    /* White horizontal border separating sections B and C (left half only) */
    lcd_fill(0, midy,
             (uint16_t)(midx - 1),
             (uint16_t)(midy + BORDER_W - 1),
             WHITE);

    /* Section A — 4×3 green grid */
    for (uint8_t i = 0; i < GRID_CELLS; i++)
    {
        draw_cell(&grid[i]);
    }

    /* Section B — Red and Blue square buttons */
    for (uint8_t i = 0; i < SEC_B_CNT; i++)
    {
        draw_button(&sec_b[i]);
    }

    /* Section C — Start / Retry1 / Retry2 buttons */
    for (uint8_t i = 0; i < SEC_C_CNT; i++)
    {
        draw_button(&sec_c[i]);
    }
}

/**
  * @brief  Return the UART message string for the cell/button at (x, y),
  *         or NULL if the touch is in an inactive area.
  */
static const char *get_touch_msg(uint16_t x, uint16_t y)
{
    uint8_t i;

    /* Section A */
    for (i = 0; i < GRID_CELLS; i++)
    {
        if (x >= grid[i].x1 && x <= grid[i].x2 &&
            y >= grid[i].y1 && y <= grid[i].y2)
        {
            return grid[i].label;
        }
    }

    /* Section B */
    for (i = 0; i < SEC_B_CNT; i++)
    {
        if (x >= sec_b[i].x1 && x <= sec_b[i].x2 &&
            y >= sec_b[i].y1 && y <= sec_b[i].y2)
        {
            return sec_b[i].label;
        }
    }

    /* Section C */
    for (i = 0; i < SEC_C_CNT; i++)
    {
        if (x >= sec_c[i].x1 && x <= sec_c[i].x2 &&
            y >= sec_c[i].y1 && y <= sec_c[i].y2)
        {
            return sec_c[i].label;
        }
    }

    return NULL;
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
  init_layout();
  draw_interface();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
      tp_dev.scan(0);

      if (tp_dev.sta & TP_PRES_DOWN)
      {
          uint16_t    x   = tp_dev.x[0];
          uint16_t    y   = tp_dev.y[0];
          const char *msg = get_touch_msg(x, y);

          if (msg != NULL)
          {
              printf("%s\n", msg);

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
