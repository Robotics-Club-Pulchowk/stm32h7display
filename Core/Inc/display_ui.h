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
    UI_TOUCH_CTRL_START,
    UI_TOUCH_CTRL_RETRY1,
    UI_TOUCH_CTRL_RETRY2,
    UI_TOUCH_RESET,
} ui_touch_id_t;

void display_ui_init(void);
void display_ui_draw(void);
ui_touch_id_t display_ui_get_touch_id(uint16_t x, uint16_t y);
void display_ui_set_grid_selected(uint8_t idx, bool selected);
void display_ui_set_team_selection(uint8_t team);
void display_ui_set_ctrl_selection(int8_t idx);
void display_ui_reset_visual_state(void);

#endif /* __DISPLAY_UI_H */
