
/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    main.c
 * @brief   Main program body — TX UI with CAM/SCR and TX enable
 ******************************************************************************
 */
/* USER CODE END Header */

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

/* USER CODE BEGIN PD */
#define UART_FRAME_INTERVAL_MS 100u
#define GRID_CELL_COUNT 12u
#define UART_FRAME_MAX_LEN 42u

#define MAX_AR_CELLS 4u
#define MAX_MR_CELLS 3u
#define MAX_FAKE_CELLS 1u

#define DISPLAY_UI_CAM 0u
#define DISPLAY_UI_SCREEN 1u
/* USER CODE END PD */

/* USER CODE BEGIN PV */
static uint8_t g_team_sel = DISPLAY_UI_TEAM_RED;
static uint8_t g_scroll_mode = DISPLAY_UI_SCROLL_AR;
static uint8_t g_matrix_state[GRID_CELL_COUNT];

static uint32_t g_last_uart_sent_ms = 0u;

static const uint8_t g_uart_cell_order[GRID_CELL_COUNT] =
    {
        11u, 10u, 9u, 8u, 7u, 6u,
        5u, 4u, 3u, 2u, 1u, 0u};

static uint8_t g_touch_down = 0u;
static uint16_t g_touch_x = 0u;
static uint16_t g_touch_y = 0u;

static uint8_t g_cam_screen = 0u;
static uint8_t uart_send_flag = 0u;
/* USER CODE END PV */

/* USER CODE BEGIN 0 */

static uint32_t elapsed_ms(uint32_t now, uint32_t then)
{
    return now - then;
}

static uint8_t count_cells_in_mode(uint8_t mode)
{
    uint8_t n = 0u;

    for (uint8_t i = 0u; i < GRID_CELL_COUNT; i++)
    {
        if (g_matrix_state[i] == mode)
        {
            n++;
        }
    }

    return n;
}

static uint8_t mode_limit(uint8_t mode)
{
    if (mode == DISPLAY_UI_SCROLL_AR)
        return MAX_AR_CELLS;

    if (mode == DISPLAY_UI_SCROLL_MR)
        return MAX_MR_CELLS;

    if (mode == DISPLAY_UI_SCROLL_FAKE)
        return MAX_FAKE_CELLS;

    return 0u;
}

static uint8_t mode_slot_available(uint8_t mode)
{
    return (count_cells_in_mode(mode) < mode_limit(mode));
}

static void reset_all_state(void)
{
    g_team_sel = DISPLAY_UI_TEAM_RED;
    g_scroll_mode = DISPLAY_UI_SCROLL_AR;
    g_cam_screen = 0u;
    uart_send_flag = 0u;

    memset(g_matrix_state, 0, sizeof(g_matrix_state));

    display_ui_reset_visual_state();
    display_ui_draw();
}

static uint8_t frame_append_char(
    char *frame,
    size_t len,
    size_t *off,
    char ch)
{
    if (*off >= (len - 1u))
    {
        return 0u;
    }

    frame[(*off)++] = ch;
    frame[*off] = '\0';

    return 1u;
}

static uint8_t frame_append_bits(
    char *frame,
    size_t len,
    size_t *off,
    const char *bits)
{
    if (!bits)
    {
        return 0u;
    }

    return (uint8_t)(frame_append_char(frame, len, off, bits[0]) &&
                     frame_append_char(frame, len, off, bits[1]));
}

static void send_uart_frame(void)
{
    char frame[UART_FRAME_MAX_LEN];
    size_t off = 0u;

    frame[0] = '\0';

    frame_append_char(
        frame,
        sizeof(frame),
        &off,
        (g_team_sel == DISPLAY_UI_TEAM_BLUE) ? '1' : '0');

    frame_append_char(frame, sizeof(frame), &off, ' ');

    frame_append_char(
        frame,
        sizeof(frame),
        &off,
        g_cam_screen ? '1' : '0');

    for (uint8_t i = 0u; i < GRID_CELL_COUNT; i++)
    {
        uint8_t ci = g_uart_cell_order[i];
        uint8_t st = (uint8_t)(g_matrix_state[ci] & 0x03u);

        const char *bits = "00";

        if (st == 1u)
        {
            bits = "01";
        }
        else if (st == 2u)
        {
            bits = "10";
        }
        else if (st == 3u)
        {
            bits = "11";
        }

        frame_append_char(frame, sizeof(frame), &off, ' ');
        frame_append_bits(frame, sizeof(frame), &off, bits);
    }

    frame_append_char(frame, sizeof(frame), &off, '\r');
    frame_append_char(frame, sizeof(frame), &off, '\n');

    printf("%s", frame);
}

static void handle_touch_page1(uint16_t x, uint16_t y)
{
    ui_touch_id_t id = display_ui_get_touch_id(x, y);

    if (id == UI_TOUCH_NONE)
    {
        return;
    }

    if (id >= UI_TOUCH_GRID_A &&
        id <= UI_TOUCH_GRID_L)
    {
        uint8_t idx = (uint8_t)(id - UI_TOUCH_GRID_A);

        if (g_matrix_state[idx] != 0u)
        {
            g_matrix_state[idx] = 0u;
            display_ui_set_grid_state(idx, 0u);
        }
        else
        {
            if (mode_slot_available(g_scroll_mode))
            {
                g_matrix_state[idx] = g_scroll_mode;
                display_ui_set_grid_state(idx, g_scroll_mode);
            }
        }

        return;
    }

    if (id == UI_TOUCH_TEAM_TOGGLE)
    {
        g_team_sel =
            (g_team_sel == DISPLAY_UI_TEAM_RED) ? DISPLAY_UI_TEAM_BLUE : DISPLAY_UI_TEAM_RED;

        display_ui_set_team_selection(g_team_sel);

        return;
    }

    if (id == UI_TOUCH_SCROLL_MODE)
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

        return;
    }

    if (id == UI_TOUCH_RESET)
    {
        reset_all_state();
        return;
    }

    if (id == UI_TOUCH_CAM_SCREEN)
    {
        g_cam_screen ^= 1u;

        display_ui_set_cam_screen(g_cam_screen);

        return;
    }
}

/* USER CODE END 0 */

int main(void)
{
    HAL_Init();

    /* USER CODE BEGIN SysInit */
    sys_stm32_clock_init(192, 5, 2, 4);
    /* USER CODE END SysInit */

    MX_GPIO_Init();

    /* USER CODE BEGIN 2 */
    mpu_memory_protection();
    delay_init(480);

    sdram_init();
    lcd_init();

    usart1_init(115200);

    tp_dev.init();

    display_ui_init();
    display_ui_draw();

    g_last_uart_sent_ms = HAL_GetTick();
    /* USER CODE END 2 */

    while (1)
    {
        uint32_t now_ms = HAL_GetTick();

        if ((elapsed_ms(now_ms, g_last_uart_sent_ms) >= UART_FRAME_INTERVAL_MS) && uart_send_flag)
        {
            send_uart_frame();
            g_last_uart_sent_ms = now_ms;
        }

        tp_dev.scan(0);

        if (tp_dev.sta & TP_PRES_DOWN)
        {
            if (!g_touch_down)
            {
                g_touch_down = 1u;
            }

            g_touch_x = tp_dev.x[0];
            g_touch_y = tp_dev.y[0];
            // Check if touch happened on uart button, if so, update uart_send_flag and redraw button immediately
            ui_touch_id_t touch_id = display_ui_get_touch_id(g_touch_x, g_touch_y);
            if (touch_id == UI_TOUCH_UART_SEND)
            {
                uart_send_flag = 1u;
                display_ui_set_uart_send(1u);
            }
            else
                uart_send_flag = 0u; // If touch is not on uart button, disable uart sending to prevent accidental sends
            display_ui_set_uart_send(0u);
        }
        else
        {
            if (g_touch_down)
            {
                g_touch_down = 0u;

                handle_touch_page1(
                    g_touch_x,
                    g_touch_y);
            }
        }
    }
}

/* ── Error handler ───────────────────────────────────────────── */

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}

#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}

#endif
