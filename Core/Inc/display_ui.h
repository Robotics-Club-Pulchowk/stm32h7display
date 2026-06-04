/**
 ****************************************************************************************************
 * @file        display_ui.h
 * @brief       Touch display UI layout, drawing, and touch-label mapping API.
 ****************************************************************************************************
 */

#ifndef __DISPLAY_UI_H
#define __DISPLAY_UI_H

#include <stdint.h>
#include <stdbool.h>

#define DISPLAY_UI_MODE_TX 0u
#define DISPLAY_UI_MODE_RX 1u

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
    UI_TOUCH_TEAM_RED,
    UI_TOUCH_TEAM_BLUE,
    UI_TOUCH_RESET,
    UI_TOUCH_MODE_TOGGLE,
} ui_touch_id_t;

void display_ui_init(void);
void display_ui_draw(void);
ui_touch_id_t display_ui_get_touch_id(uint16_t x, uint16_t y);
void display_ui_set_grid_state(uint8_t idx, uint8_t state);
void display_ui_set_team_selection(uint8_t team);
void display_ui_reset_visual_state(void);
void display_ui_set_mode(uint8_t mode);
void display_ui_set_rx_text(const char *text);

#endif /* __DISPLAY_UI_H */
