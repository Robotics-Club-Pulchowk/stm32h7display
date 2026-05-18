#include "display_app.h"

#include "main.h"
#include "sys.h"
#include "delay.h"
#include "sdram.h"
#include "lcd.h"
#include "mpu.h"
#include "touch.h"
#include "usart.h"

#include <string.h>
#include <stdio.h>

#define BUTTON_GAP        24
#define BUTTON_MARGIN     20
#define BUTTON_FONT_SIZE  24
#define BUTTON_COUNT      4
#define BUTTON_INDEX_NONE 0xFF

typedef struct
{
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    uint16_t color;
    const char *label;
} color_button_t;

static uint32_t uart_last_tick = 0;
static uint32_t uart_counter   = 0;

static color_button_t color_buttons[BUTTON_COUNT] =
{
    {0, 0, 0, 0, RED,   "RED"},
    {0, 0, 0, 0, GREEN, "GREEN"},
    {0, 0, 0, 0, BLUE,  "BLUE"},
    {0, 0, 0, 0, BLACK, "BLACK"}
};

static void draw_interface(void);
static void set_background_color(uint16_t color);
static uint8_t get_touched_button_index(uint16_t x, uint16_t y);

static void draw_interface(void)
{
    uint16_t matrix_w;
    uint16_t matrix_h;
    uint16_t start_x;
    uint16_t start_y;
    uint16_t button_size;
    uint8_t i;

    uint16_t max_button_w = (lcddev.width - (2 * BUTTON_MARGIN) - BUTTON_GAP) / 2;
    uint16_t max_button_h = (lcddev.height - (2 * BUTTON_MARGIN) - BUTTON_GAP) / 2;

    button_size = (max_button_w < max_button_h) ? max_button_w : max_button_h;
    if (button_size == 0)
    {
        return;
    }

    matrix_w = (2 * button_size) + BUTTON_GAP;
    matrix_h = (2 * button_size) + BUTTON_GAP;
    start_x = (lcddev.width - matrix_w) / 2;
    start_y = (lcddev.height - matrix_h) / 2;

    color_buttons[0].x1 = start_x;
    color_buttons[0].y1 = start_y;
    color_buttons[0].x2 = start_x + button_size - 1;
    color_buttons[0].y2 = start_y + button_size - 1;

    color_buttons[1].x1 = start_x + button_size + BUTTON_GAP;
    color_buttons[1].y1 = start_y;
    color_buttons[1].x2 = color_buttons[1].x1 + button_size - 1;
    color_buttons[1].y2 = start_y + button_size - 1;

    color_buttons[2].x1 = start_x;
    color_buttons[2].y1 = start_y + button_size + BUTTON_GAP;
    color_buttons[2].x2 = start_x + button_size - 1;
    color_buttons[2].y2 = color_buttons[2].y1 + button_size - 1;

    color_buttons[3].x1 = start_x + button_size + BUTTON_GAP;
    color_buttons[3].y1 = start_y + button_size + BUTTON_GAP;
    color_buttons[3].x2 = color_buttons[3].x1 + button_size - 1;
    color_buttons[3].y2 = color_buttons[3].y1 + button_size - 1;

    for (i = 0; i < BUTTON_COUNT; i++)
    {
        uint16_t label_len;
        uint16_t text_w;
        uint16_t text_h;
        uint16_t text_x;
        uint16_t text_y;

        label_len = (uint16_t)strlen(color_buttons[i].label);
        text_w = label_len * (BUTTON_FONT_SIZE / 2);
        text_h = BUTTON_FONT_SIZE;
        if (text_w > button_size) text_w = button_size;
        if (text_h > button_size) text_h = button_size;
        text_x = color_buttons[i].x1 + ((button_size - text_w) / 2);
        text_y = color_buttons[i].y1 + ((button_size - text_h) / 2);

        lcd_fill(
            color_buttons[i].x1,
            color_buttons[i].y1,
            color_buttons[i].x2,
            color_buttons[i].y2,
            color_buttons[i].color
        );

        g_back_color = color_buttons[i].color;
        lcd_show_string(text_x, text_y, text_w, text_h, BUTTON_FONT_SIZE, color_buttons[i].label, WHITE);
    }
}

static void set_background_color(uint16_t color)
{
    lcd_clear(color);
    draw_interface();
}

static uint8_t get_touched_button_index(uint16_t x, uint16_t y)
{
    uint8_t i;

    for (i = 0; i < BUTTON_COUNT; i++)
    {
        if ((x >= color_buttons[i].x1) && (x <= color_buttons[i].x2) &&
            (y >= color_buttons[i].y1) && (y <= color_buttons[i].y2))
        {
            return i;
        }
    }

    return BUTTON_INDEX_NONE;
}

void DisplayApp_Init(void)
{
    mpu_memory_protection();
    delay_init(480);
    sdram_init();
    lcd_init();
    usart1_init(115200);
    tp_dev.init();
    set_background_color(BLUE);
}

void DisplayApp_Run(void)
{
    tp_dev.scan(0);

    if (tp_dev.sta & TP_PRES_DOWN)
    {
        uint16_t x = tp_dev.x[0];
        uint16_t y = tp_dev.y[0];

        uint8_t button_index = get_touched_button_index(x, y);

        if (button_index != BUTTON_INDEX_NONE)
        {
            set_background_color(color_buttons[button_index].color);
            printf("%s\n", color_buttons[button_index].label);

            while (tp_dev.sta & TP_PRES_DOWN)
            {
                tp_dev.scan(0);
                delay_ms(10);
            }
        }
    }

    {
        uint32_t now = HAL_GetTick();
        if ((now - uart_last_tick) >= 1000U)
        {
            uart_last_tick = now;
            uart_counter++;
            printf("STM32H7 UART Hello #%lu\n", uart_counter);
        }
    }
}
