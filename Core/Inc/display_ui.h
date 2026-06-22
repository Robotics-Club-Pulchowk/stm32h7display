#ifndef __DISPLAY_UI_H
#define __DISPLAY_UI_H

#include <stdint.h>

typedef enum
{
    UI_TOUCH_NONE = 0,
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
} ui_touch_id_t;

#define DISPLAY_UI_TEAM_RED  0u
#define DISPLAY_UI_TEAM_BLUE 1u

#define DISPLAY_UI_SCROLL_AR   1u
#define DISPLAY_UI_SCROLL_MR   2u
#define DISPLAY_UI_SCROLL_FAKE 3u

void          display_ui_init(void);
void          display_ui_draw(void);
ui_touch_id_t display_ui_get_touch_id(uint16_t x, uint16_t y);

void display_ui_set_grid_state(uint8_t idx, uint8_t state);
void display_ui_set_team_selection(uint8_t team);
void display_ui_set_cam_screen(uint8_t state);
void display_ui_set_scroll_mode(uint8_t mode);
void display_ui_reset_visual_state(void);
void display_ui_set_uart_send(uint8_t state);

#endif