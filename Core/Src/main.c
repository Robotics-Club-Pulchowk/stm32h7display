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
#define UART_FRAME_INTERVAL_MS  100u
#define GRID_CELL_COUNT         12u
#define UART_FRAME_MAX_LEN      42u

#define MAX_AR_CELLS            4u
#define MAX_MR_CELLS            3u
#define MAX_FAKE_CELLS          1u

#define DISPLAY_UI_CAM          0u
#define DISPLAY_UI_SCREEN       1u

/*
 * Swipe detection
 * ──────────────────────────────────────────────────────────────────────────
 * A gesture is classified as a swipe when the finger lifts after travelling
 * at least SWIPE_THRESHOLD pixels in X while staying within SWIPE_Y_DEADZONE
 * pixels in Y.  This prevents accidental page flips when the user taps a
 * button near an edge of the screen.
 */
#define SWIPE_THRESHOLD   50u   /* minimum horizontal pixel travel  */
#define SWIPE_Y_DEADZONE  80u   /* maximum vertical drift allowed    */
/* USER CODE END PD */

/* USER CODE BEGIN PV */
static uint8_t  g_team_sel    = DISPLAY_UI_TEAM_RED;
static uint8_t  g_scroll_mode = DISPLAY_UI_SCROLL_AR;
static uint8_t  g_matrix_state[GRID_CELL_COUNT];

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

static uint8_t  g_cam_screen   = 0u;
static uint8_t  uart_send_flag = 0u;

/* ── Motor states (page 2) ──────────────────────────────────────────────────
 * Each motor button is MOMENTARY: it is active (1) only while the finger
 * is held down, and returns to inactive (0) on finger-up.
 *
 * g_motor_held  — bitmask of which motor buttons are currently pressed.
 *                 Bit N corresponds to motor index N.
 *
 * g_motor_state — last value sent to display_ui_set_motor_state().
 *                 Used as a change-guard so the UI function is called
 *                 only when the state actually transitions (0→1 or 1→0),
 *                 mirroring the uart_send_flag / display_ui_set_uart_send()
 *                 pattern used on page 1.
 * ──────────────────────────────────────────────────────────────────────── */
#define MOTOR_COUNT 6u
static uint8_t g_motor_held;              /* bitmask: bit N = motor N held */
static uint8_t g_motor_state[MOTOR_COUNT]; /* last value sent to UI layer   */
/* USER CODE END PV */

/* USER CODE BEGIN 0 */

/* ── Utility ────────────────────────────────────────────────────────────── */

static uint32_t elapsed_ms(uint32_t now, uint32_t then)
{
    return now - then;
}

/* ── Page 1 helpers ─────────────────────────────────────────────────────── */

static uint8_t count_cells_in_mode(uint8_t mode)
{
    uint8_t n = 0u;
    for (uint8_t i = 0u; i < GRID_CELL_COUNT; i++)
    {
        if (g_matrix_state[i] == mode)
            n++;
    }
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
    g_team_sel    = DISPLAY_UI_TEAM_RED;
    g_scroll_mode = DISPLAY_UI_SCROLL_AR;
    g_cam_screen  = 0u;
    uart_send_flag = 0u;

    memset(g_matrix_state, 0, sizeof(g_matrix_state));

    /* Release all held motors and zero the change-guard */
    g_motor_held = 0u;
    memset(g_motor_state,  0, sizeof(g_motor_state));

    display_ui_reset_visual_state();
    display_ui_draw();
}

/* ── UART frame builder ─────────────────────────────────────────────────── */

static uint8_t frame_append_char(char *frame, size_t len, size_t *off, char ch)
{
    if (*off >= (len - 1u)) return 0u;
    frame[(*off)++] = ch;
    frame[*off]     = '\0';
    return 1u;
}

static uint8_t frame_append_bits(char *frame, size_t len, size_t *off, const char *bits)
{
    if (!bits) return 0u;
    return (uint8_t)(frame_append_char(frame, len, off, bits[0]) &&
                     frame_append_char(frame, len, off, bits[1]));
}

static void send_uart_frame(void)
{
    char   frame[UART_FRAME_MAX_LEN];
    size_t off = 0u;

    frame[0] = '\0';

    frame_append_char(frame, sizeof(frame), &off,
                      (g_team_sel == DISPLAY_UI_TEAM_BLUE) ? '1' : '0');
    frame_append_char(frame, sizeof(frame), &off, ' ');
    frame_append_char(frame, sizeof(frame), &off, g_cam_screen ? '1' : '0');

    for (uint8_t i = 0u; i < GRID_CELL_COUNT; i++)
    {
        uint8_t     ci   = g_uart_cell_order[i];
        uint8_t     st   = (uint8_t)(g_matrix_state[ci] & 0x03u);
        const char *bits = "00";

        if      (st == 1u) bits = "01";
        else if (st == 2u) bits = "10";
        else if (st == 3u) bits = "11";

        frame_append_char(frame, sizeof(frame), &off, ' ');
        frame_append_bits(frame, sizeof(frame), &off, bits);
    }

    frame_append_char(frame, sizeof(frame), &off, '\r');
    frame_append_char(frame, sizeof(frame), &off, '\n');

    printf("%s", frame);
}

/* ── Motor press / release helpers ─────────────────────────────────────────
 *
 * These two functions mirror the uart_send_flag pattern:
 *
 *   press  → set bit in g_motor_held, call display_ui_set_motor_state(1)
 *             only when the state changes 0 → 1.
 *
 *   release → clear bit in g_motor_held, call display_ui_set_motor_state(0)
 *             only when the state changes 1 → 0.
 *
 * The change-guard (g_motor_state[]) prevents redundant redraws if the
 * touch scanner delivers multiple consecutive "still held" reports before
 * the finger actually lifts.
 * ──────────────────────────────────────────────────────────────────────── */

static void motor_press(uint8_t idx)
{
    if (idx >= MOTOR_COUNT) return;

    g_motor_held |= (uint8_t)(1u << idx);

    if (g_motor_state[idx] != 1u)   /* change-guard: only update on 0 → 1 */
    {
        g_motor_state[idx] = 1u;
        display_ui_set_motor_state(idx, 1u);
    }
}

static void motor_release(uint8_t idx)
{
    if (idx >= MOTOR_COUNT) return;

    g_motor_held &= (uint8_t)~(1u << idx);

    if (g_motor_state[idx] != 0u)   /* change-guard: only update on 1 → 0 */
    {
        g_motor_state[idx] = 0u;
        display_ui_set_motor_state(idx, 0u);
    }
}

/*
 * release_all_motors — release every motor that was held.
 * Called when a swipe is detected mid-hold so no motor stays stuck ON.
 */
static void release_all_motors(void)
{
    for (uint8_t i = 0u; i < MOTOR_COUNT; i++)
    {
        if (g_motor_held & (uint8_t)(1u << i))
            motor_release(i);
    }
}

/* ── Touch handlers ─────────────────────────────────────────────────────── */

/*
 * handle_touch_page1 — called on finger-lift while on page 1.
 * Handles all button taps; swipe detection is done before this call
 * so we never reach here for genuine swipe gestures.
 */
static void handle_touch_page1(uint16_t x, uint16_t y)
{
    ui_touch_id_t id = display_ui_get_touch_id(x, y);

    if (id == UI_TOUCH_NONE)
        return;

    /* Grid cell tap */
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
        g_team_sel =
            (g_team_sel == DISPLAY_UI_TEAM_RED) ? DISPLAY_UI_TEAM_BLUE : DISPLAY_UI_TEAM_RED;
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

/*
 * handle_finger_down_page2 — called on finger-DOWN while on page 2.
 *
 * Motor buttons are momentary: they activate on press (like uart_send_flag
 * activates on finger-down) and deactivate on release.
 * Init All and Start Tree remain tap-on-release (momentary feedback only).
 */
static void handle_finger_down_page2(uint16_t x, uint16_t y)
{
    ui_touch_id_t id = display_ui_get_touch_id(x, y);

    if (id >= UI_TOUCH_MOTOR_1 && id <= UI_TOUCH_MOTOR_6)
    {
        motor_press((uint8_t)(id - UI_TOUCH_MOTOR_1));
    }
}

/*
 * handle_finger_up_page2 — called on finger-UP while on page 2.
 *
 * Motor buttons: release whichever motor was held under the finger.
 * Init All / Start Tree: momentary visual feedback on tap-up.
 */
static void handle_finger_up_page2(uint16_t x, uint16_t y)
{
    ui_touch_id_t id = display_ui_get_touch_id(x, y);

    if (id >= UI_TOUCH_MOTOR_1 && id <= UI_TOUCH_MOTOR_6)
    {
        motor_release((uint8_t)(id - UI_TOUCH_MOTOR_1));
        return;
    }

    /*
     * If the finger lifted outside the button it was originally pressed on
     * (e.g. a short drag), release every held motor so nothing stays stuck.
     */
    if (g_motor_held)
        release_all_motors();

    if (id == UI_TOUCH_INIT_ALL)
    {
        display_ui_set_init_all_state(1u);
        /* TODO: trigger init-all action here */
        return;
    }

    if (id == UI_TOUCH_START_TREE)
    {
        display_ui_set_start_tree_state(1u);
        /* TODO: trigger start-tree action here */
        return;
    }
}

/*
 * check_swipe — called on every finger-lift.
 *
 * Returns 1 if the gesture was classified as a swipe and the page was
 * changed; returns 0 if it was a tap (caller should handle as button press).
 */
static uint8_t check_swipe(uint16_t x_down, uint16_t y_down,
                            uint16_t x_up,   uint16_t y_up)
{
    int32_t dx = (int32_t)x_up   - (int32_t)x_down;
    int32_t dy = (int32_t)y_up   - (int32_t)y_down;

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

        /* ── Periodic UART transmit (page 1 only) ─────────────────────── */
        if (elapsed_ms(now_ms, g_last_uart_sent_ms) >= UART_FRAME_INTERVAL_MS
            && uart_send_flag
            && display_ui_get_page() == DISPLAY_UI_PAGE_TX)
        {
            send_uart_frame();
            g_last_uart_sent_ms = now_ms;
        }

        /* ── Touch scan ───────────────────────────────────────────────── */
        tp_dev.scan(0);

        if (tp_dev.sta & TP_PRES_DOWN)
        {
            g_touch_x = tp_dev.x[0];
            g_touch_y = tp_dev.y[0];

            if (!g_touch_down)
            {
                /* Finger just landed — record origin for swipe detection */
                g_touch_down   = 1u;
                g_touch_down_x = g_touch_x;
                g_touch_down_y = g_touch_y;

                /* ── Press-sensitive buttons (activate on finger-down) ── */
                uint8_t page = display_ui_get_page();

                if (page == DISPLAY_UI_PAGE_TX)
                {
                    /*
                     * TX-enable: active while held, same as motor buttons.
                     */
                    ui_touch_id_t id =
                        display_ui_get_touch_id(g_touch_x, g_touch_y);

                    if (id == UI_TOUCH_UART_SEND)
                    {
                        uart_send_flag = 1u;
                        display_ui_set_uart_send(1u);
                    }
                }
                else if (page == DISPLAY_UI_PAGE_MOTOR)
                {
                    /*
                     * Motor buttons: activate on finger-down (momentary).
                     * The change-guard inside motor_press() ensures the UI
                     * function is only called once on the 0 → 1 transition.
                     */
                    handle_finger_down_page2(g_touch_x, g_touch_y);
                }
            }
        }
        else
        {
            if (g_touch_down)
            {
                /* Finger lifted — evaluate swipe vs tap */
                g_touch_down = 0u;

                uint8_t was_swipe = check_swipe(
                    g_touch_down_x, g_touch_down_y,
                    g_touch_x,      g_touch_y);

                if (!was_swipe)
                {
                    uint8_t page = display_ui_get_page();

                    if (page == DISPLAY_UI_PAGE_TX)
                    {
                        ui_touch_id_t id =
                            display_ui_get_touch_id(g_touch_x, g_touch_y);

                        if (id == UI_TOUCH_UART_SEND)
                        {
                            /* Release TX-send on finger-up */
                            uart_send_flag = 0u;
                            display_ui_set_uart_send(0u);
                        }
                        else
                        {
                            handle_touch_page1(g_touch_x, g_touch_y);
                        }
                    }
                    else if (page == DISPLAY_UI_PAGE_MOTOR)
                    {
                        /*
                         * Motor release on finger-up.
                         * The change-guard inside motor_release() ensures the
                         * UI function is only called once on the 1 → 0 transition.
                         */
                        handle_finger_up_page2(g_touch_x, g_touch_y);
                    }
                }
                else
                {
                    /*
                     * It was a swipe — release any held buttons so nothing
                     * stays stuck ON after the page changes.
                     */
                    if (uart_send_flag)
                    {
                        uart_send_flag = 0u;
                        display_ui_set_uart_send(0u);
                    }

                    if (g_motor_held)
                        release_all_motors();
                }
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