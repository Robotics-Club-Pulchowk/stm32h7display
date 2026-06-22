#ifndef __DISPLAY_UI_H
#define __DISPLAY_UI_H

#include <stdint.h>

/* ── Touch IDs ──────────────────────────────────────────────────────────── */

typedef enum
{
    UI_TOUCH_NONE = 0,

    /* Page 1 – TX grid */
    UI_TOUCH_GRID_A,
    UI_TOUCH_GRID_B,
    UI_TOUCH_GRID_C,
    UI_TOUCH_GRID_D,
    UI_TOUCH_GRID_E,
    UI_TOUCH_GRID_F,
    UI_TOUCH_GRID_G,
    UI_TOUCH_GRID_H,
    UI_TOUCH_GRID_I,
    UI_TOUCH_GRID_J,
    UI_TOUCH_GRID_K,
    UI_TOUCH_GRID_L,
    UI_TOUCH_TEAM_TOGGLE,
    UI_TOUCH_SCROLL_MODE,
    UI_TOUCH_RESET,
    UI_TOUCH_CAM_SCREEN,
    UI_TOUCH_UART_SEND,

    /* Page 2 – Motor control */
    UI_TOUCH_MOTOR_1,
    UI_TOUCH_MOTOR_2,
    UI_TOUCH_MOTOR_3,
    UI_TOUCH_MOTOR_4,
    UI_TOUCH_MOTOR_5,
    UI_TOUCH_MOTOR_6,
    UI_TOUCH_INIT_ALL,
    UI_TOUCH_START_TREE,

} ui_touch_id_t;

/* ── Team / scroll constants ────────────────────────────────────────────── */

#define DISPLAY_UI_TEAM_RED    0u
#define DISPLAY_UI_TEAM_BLUE   1u

#define DISPLAY_UI_SCROLL_AR   1u
#define DISPLAY_UI_SCROLL_MR   2u
#define DISPLAY_UI_SCROLL_FAKE 3u

/* ── Page IDs ───────────────────────────────────────────────────────────── */

#define DISPLAY_UI_PAGE_TX     0u
#define DISPLAY_UI_PAGE_MOTOR  1u

/* ── Public API ─────────────────────────────────────────────────────────── */

/* Initialise layout geometry (call once after lcd_init) */
void          display_ui_init(void);

/* Repaint the current active page */
void          display_ui_draw(void);

/* Hit-test a touch coordinate; returns the matching UI_TOUCH_* id */
ui_touch_id_t display_ui_get_touch_id(uint16_t x, uint16_t y);

/* Page 1 state setters */
void display_ui_set_grid_state(uint8_t idx, uint8_t state);
void display_ui_set_team_selection(uint8_t team);
void display_ui_set_cam_screen(uint8_t state);
void display_ui_set_scroll_mode(uint8_t mode);
void display_ui_reset_visual_state(void);
void display_ui_set_uart_send(uint8_t state);

/* Page 2 state setters */
void display_ui_set_motor_state(uint8_t motor_idx, uint8_t active);
void display_ui_set_init_all_state(uint8_t active);
void display_ui_set_start_tree_state(uint8_t active);

/* Page navigation */
void display_ui_set_page(uint8_t page);
uint8_t display_ui_get_page(void);

#endif /* __DISPLAY_UI_H */