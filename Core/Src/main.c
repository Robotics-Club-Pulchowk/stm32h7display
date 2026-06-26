/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    main.c
 * @brief   Main program body — TX UI with CAM/SCR, TX enable, and swipe pages
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
#define UART_FRAME_MAX_LEN 100u

#define MAX_AR_CELLS 4u
#define MAX_MR_CELLS 3u
#define MAX_FAKE_CELLS 1u

#define DISPLAY_UI_CAM 0u
#define DISPLAY_UI_SCREEN 1u

/*
 * Swipe detection
 * ──────────────────────────────────────────────────────────────────────────
 * A gesture is classified as a swipe when the finger lifts after travelling
 * at least SWIPE_THRESHOLD pixels in X while staying within SWIPE_Y_DEADZONE
 * pixels in Y.  This prevents accidental page flips when the user taps a
 * button near an edge of the screen.
 */
#define SWIPE_THRESHOLD  50u   /* minimum horizontal pixel travel  */
#define SWIPE_Y_DEADZONE 80u   /* maximum vertical drift allowed   */
/* USER CODE END PD */

/* USER CODE BEGIN PV */
static uint8_t g_team_sel   = DISPLAY_UI_TEAM_RED;
static uint8_t g_scroll_mode = DISPLAY_UI_SCROLL_AR;
static uint8_t g_matrix_state[GRID_CELL_COUNT];

static uint32_t g_last_uart_sent_ms = 0u;

static const uint8_t g_uart_cell_order[GRID_CELL_COUNT] =
{
    11u, 10u, 9u, 8u, 7u, 6u,
     5u,  4u, 3u, 2u, 1u, 0u
};

/* ── Touch tracking ─────────────────────────────────────────────────────── */
static uint8_t  g_touch_down   = 0u;
static uint16_t g_touch_x      = 0u;   /* current / last X              */
static uint16_t g_touch_y      = 0u;   /* current / last Y              */
static uint16_t g_touch_down_x = 0u;   /* X where the finger first landed */
static uint16_t g_touch_down_y = 0u;   /* Y where the finger first landed */

static uint8_t g_cam_screen   = 0u;
static uint8_t uart_send_flag = 0u;

/* ── Motor states (page 2) ──────────────────────────────────────────────── */
#define MOTOR_COUNT 6u
static uint8_t g_motor_state[MOTOR_COUNT];
static uint8_t init_all    = 0u;
static uint8_t start_tree  = 0u;
/* USER CODE END PV */

/* USER CODE BEGIN 0 */

/* ── Utility ────────────────────────────────────────────────────────────── */

static uint32_t elapsed_ms(uint32_t now, uint32_t then)
{
    return now - then;
}

uint8_t calculate_cr8x_fast(uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++)
        crc = crc8x_table[data[i] ^ crc];
    return crc;
}

/* ── Page 1 helpers ─────────────────────────────────────────────────────── */

static uint8_t count_cells_in_mode(uint8_t mode)
{
    uint8_t n = 0u;
    for (uint8_t i = 0u; i < GRID_CELL_COUNT; i++)
        if (g_matrix_state[i] == mode) n++;
    return n;
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
    return (count_cells_in_mode(mode) < mode_limit(mode));
}

static void reset_all_state(void)
{
    g_team_sel     = DISPLAY_UI_TEAM_RED;
    g_scroll_mode  = DISPLAY_UI_SCROLL_AR;
    g_cam_screen   = 0u;
    uart_send_flag = 0u;

    memset(g_matrix_state, 0, sizeof(g_matrix_state));
    memset(g_motor_state,  0, sizeof(g_motor_state));

    display_ui_reset_visual_state();
    display_ui_draw();
}

/* ── UART frame builder ─────────────────────────────────────────────────── */

static void send_uart_frame(void)
{
    uint8_t pkt[17] = {0};

    pkt[0] = 0xA5;

    /*
     * Page-1 fields only carry real data while TX is held.
     * Page-2 fields always reflect live state.
     */
    if (uart_send_flag != 0u)
    {
        pkt[1] = (g_team_sel == DISPLAY_UI_TEAM_BLUE) ? 1u : 0u;
        pkt[2] = g_cam_screen;
        for (uint8_t i = 0u; i < GRID_CELL_COUNT; i++)
            pkt[3 + i] = g_matrix_state[i];
    }

    pkt[15] = 0u;
    for (uint8_t i = 0u; i < MOTOR_COUNT; i++)
        pkt[15] |= (g_motor_state[i] & 1u) << i;
    pkt[15] |= (init_all   & 1u) << 6u;
    pkt[15] |= (start_tree & 1u) << 7u;

    pkt[16] = calculate_cr8x_fast(&pkt[1], 15u);

    HAL_UART_Transmit(&huart1, pkt, 17u, 100u);
}

/* ── Page 1: finger-down handler ────────────────────────────────────────────
 *
 * Called exactly once per touch, on the very first scan where the finger
 * is detected (the leading edge of g_touch_down going 0→1).
 *
 * Behaviour:
 *   - Determines which page-1 element (x,y) hit at the moment of touch.
 *   - Fires the action for that element immediately.
 *   - After this call the finger position is ignored until it lifts;
 *     sliding elsewhere activates nothing.
 *   - TX button is deliberately excluded here — it has its own press/release
 *     handling in the main loop because it must stay ON while held and turn
 *     OFF only when the finger lifts (anywhere on screen).
 * ──────────────────────────────────────────────────────────────────────────── */
static void handle_page1_finger_down(uint16_t x, uint16_t y)
{
    ui_touch_id_t id = display_ui_get_touch_id(x, y);

    /* Grid cell tap-toggle */
    if (id >= UI_TOUCH_GRID_A && id <= UI_TOUCH_GRID_L)
    {
        uint8_t idx = (uint8_t)(id - UI_TOUCH_GRID_A);
        if (g_matrix_state[idx] != 0u)
        {
            g_matrix_state[idx] = 0u;
            display_ui_set_grid_state(idx, 0u);
        }
        else if (mode_slot_available(g_scroll_mode))
        {
            g_matrix_state[idx] = g_scroll_mode;
            display_ui_set_grid_state(idx, g_scroll_mode);
        }
        return;
    }

    if (id == UI_TOUCH_TEAM_TOGGLE)
    {
        g_team_sel = (g_team_sel == DISPLAY_UI_TEAM_RED)
                         ? DISPLAY_UI_TEAM_BLUE
                         : DISPLAY_UI_TEAM_RED;
        display_ui_set_team_selection(g_team_sel);
        return;
    }

    if (id == UI_TOUCH_SCROLL_MODE)
    {
        if      (g_scroll_mode == DISPLAY_UI_SCROLL_AR)   g_scroll_mode = DISPLAY_UI_SCROLL_MR;
        else if (g_scroll_mode == DISPLAY_UI_SCROLL_MR)   g_scroll_mode = DISPLAY_UI_SCROLL_FAKE;
        else                                               g_scroll_mode = DISPLAY_UI_SCROLL_AR;
        display_ui_set_scroll_mode(g_scroll_mode);
        return;
    }

    if (id == UI_TOUCH_CAM_SCREEN)
    {
        g_cam_screen ^= 1u;
        display_ui_set_cam_screen(g_cam_screen);
        return;
    }

    if (id == UI_TOUCH_RESET)
    {
        reset_all_state();
        return;
    }

    /* UI_TOUCH_UART_SEND is handled in the main loop, not here */
}

/* ── Page 2: hold-to-send scan handler ──────────────────────────────────────
 *
 * Called every scan loop.  A control's transmitted state is 1 only while
 * the finger is physically over that button; the instant it moves away or
 * lifts, the state goes back to 0.  Redraws are edge-triggered to avoid
 * flicker.
 * ──────────────────────────────────────────────────────────────────────────── */
static void update_page2_hold_buttons(uint8_t is_down, uint16_t x, uint16_t y)
{
    ui_touch_id_t id = is_down ? display_ui_get_touch_id(x, y) : UI_TOUCH_NONE;

    for (uint8_t i = 0u; i < MOTOR_COUNT; i++)
    {
        uint8_t now = (id == (ui_touch_id_t)(UI_TOUCH_MOTOR_1 + i)) ? 1u : 0u;
        if (now != g_motor_state[i])
        {
            g_motor_state[i] = now;
            display_ui_set_motor_state(i, now);
        }
    }

    uint8_t now_init = (id == UI_TOUCH_INIT_ALL) ? 1u : 0u;
    if (now_init != init_all)
    {
        init_all = now_init;
        display_ui_set_init_all_state(now_init);
    }

    uint8_t now_tree = (id == UI_TOUCH_START_TREE) ? 1u : 0u;
    if (now_tree != start_tree)
    {
        start_tree = now_tree;
        display_ui_set_start_tree_state(now_tree);
    }
}

/* ── Swipe detection ─────────────────────────────────────────────────────────
 *
 * Called on every finger-lift.
 * Returns 1 if it was a swipe (page changed); 0 if it was a tap.
 *
 * dx > 0  →  finger moved right  →  next page
 * dx < 0  →  finger moved left   →  prev page
 * ──────────────────────────────────────────────────────────────────────────── */
static uint8_t check_swipe(uint16_t x_down, uint16_t y_down,
                            uint16_t x_up,   uint16_t y_up)
{
    int32_t  dx  = (int32_t)x_up - (int32_t)x_down;
    int32_t  dy  = (int32_t)y_up - (int32_t)y_down;
    uint32_t adx = (uint32_t)(dx < 0 ? -dx : dx);
    uint32_t ady = (uint32_t)(dy < 0 ? -dy : dy);

    if (adx < SWIPE_THRESHOLD || ady > SWIPE_Y_DEADZONE)
        return 0u;

    uint8_t current_page = display_ui_get_page();

    if (dx > 0)
    {
        if (current_page < DISPLAY_UI_PAGE_MOTOR)
            display_ui_set_page((uint8_t)(current_page + 1u));
    }
    else
    {
        if (current_page > DISPLAY_UI_PAGE_TX)
            display_ui_set_page((uint8_t)(current_page - 1u));
    }

    return 1u;
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

        /* ── Periodic UART transmit ───────────────────────────────────── */
        if (elapsed_ms(now_ms, g_last_uart_sent_ms) >= UART_FRAME_INTERVAL_MS)
        {
            send_uart_frame();
            g_last_uart_sent_ms = now_ms;
        }

        /* ── Touch scan ───────────────────────────────────────────────── */
        tp_dev.scan(0);

        uint8_t touch_now_down = (tp_dev.sta & TP_PRES_DOWN) ? 1u : 0u;

        /*
         * Page-2 hold-to-send controls: evaluated every scan so that
         * sliding between buttons while held works correctly.
         */
        update_page2_hold_buttons(touch_now_down,
                                   tp_dev.x[0], tp_dev.y[0]);

        if (touch_now_down)
        {
            g_touch_x = tp_dev.x[0];
            g_touch_y = tp_dev.y[0];

            if (!g_touch_down)
            {
                /*
                 * ── Finger just landed (leading edge) ─────────────────
                 * Record origin for swipe detection.
                 * Fire page-1 button action (except TX) immediately.
                 * Fire TX-on for the TX button immediately.
                 * After this, finger position is ignored until lift.
                 */
                g_touch_down   = 1u;
                g_touch_down_x = g_touch_x;
                g_touch_down_y = g_touch_y;

                if (display_ui_get_page() == DISPLAY_UI_PAGE_TX)
                {
                    ui_touch_id_t id =
                        display_ui_get_touch_id(g_touch_x, g_touch_y);

                    if (id == UI_TOUCH_UART_SEND)
                    {
                        /* TX button: turn on now, release on any finger-lift */
                        uart_send_flag = 1u;
                        display_ui_set_uart_send(1u);
                    }
                    else
                    {
                        /* All other page-1 buttons: fire once on touch-down */
                        handle_page1_finger_down(g_touch_x, g_touch_y);
                    }
                }
            }
            /* else: finger is still down and sliding — do nothing for page 1 */
        }
        else
        {
            if (g_touch_down)
            {
                /*
                 * ── Finger lifted ─────────────────────────────────────
                 * Evaluate swipe vs tap for page navigation.
                 * Release TX unconditionally (it was held while finger
                 * was down; it goes off regardless of where the finger
                 * lifted).
                 */
                g_touch_down = 0u;

                /* Always release TX on any finger-lift */
                if (uart_send_flag)
                {
                    uart_send_flag = 0u;
                    display_ui_set_uart_send(0u);
                }

                check_swipe(g_touch_down_x, g_touch_down_y,
                            g_touch_x,      g_touch_y);

                /*
                 * Note: check_swipe handles page navigation internally.
                 * No page-1 button actions happen on finger-lift — they
                 * all fired on finger-down already.
                 */
            }
        }
    }
}

/* ── Error handler ───────────────────────────────────────────────────────── */

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}
#endif