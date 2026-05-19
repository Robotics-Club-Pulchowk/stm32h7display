/**
 ****************************************************************************************************
 * @file        display_ui.h
 * @brief       Touch display UI layout, drawing, and touch-label mapping API.
 ****************************************************************************************************
 */

#ifndef __DISPLAY_UI_H
#define __DISPLAY_UI_H

#include <stdint.h>

void display_ui_init(void);
void display_ui_draw(void);
const char *display_ui_get_touch_msg(uint16_t x, uint16_t y);

#endif /* __DISPLAY_UI_H */
