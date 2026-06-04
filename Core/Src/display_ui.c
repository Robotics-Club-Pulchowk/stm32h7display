#include "display_ui.h"
#include "lcd.h"
#include <string.h>

/* One cell in the Section-A grid */
typedef struct
{
    uint16_t x1, y1, x2, y2;
    char     label[5];   /* up to 4 chars + NUL */
} grid_cell_t;

/* Generic labelled button used for sections B and C */
typedef struct
{
    uint16_t    x1, y1, x2, y2;
    uint32_t    fill_color;
    uint32_t    text_color;
    const char *label;
} button_t;

/* Appearance */
#define BORDER_W    2    /* width of the white section-divider lines (px) */
#define CELL_PAD    3    /* black gap around each cell / button (px) */
#define FONT_SIZE   24   /* character height used throughout */
#define RESET_HEIGHT_RATIO_DEN 5u  /* reset height = available section-C height / 5 */
#define MODE_HEIGHT_RATIO_DEN  5u
#define APP_MODE_TX            0u
#define APP_MODE_RX            1u
#define RX_TEXT_LEN            96u

/* Section-A grid dimensions */
#define GRID_COLS   3
#define GRID_ROWS   4
#define GRID_CELLS  (GRID_COLS * GRID_ROWS)   /* 12 */

/* Section-B button count */
#define SEC_B_CNT   2

static grid_cell_t grid[GRID_CELLS];
static button_t    sec_b[SEC_B_CNT];
static button_t    sec_reset;
static button_t    sec_mode;
static const char *grid_labels[GRID_CELLS] =
{
    "12", "11", "10",
    "9",  "8",  "7",
    "6",  "5",  "4",
    "3",  "2",  "1"
};

static uint8_t     grid_state[GRID_CELLS]; /* 0:number, 1:AR, 2:MR, 3:FAKE */
static uint8_t     team_selected;     /* 0:none, 1:red, 2:blue */
static uint8_t     app_mode = APP_MODE_TX;
static char        rx_text[RX_TEXT_LEN] = "Waiting for UART data...";
static uint16_t    rx_area_x1, rx_area_y1, rx_area_x2, rx_area_y2;

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

static void draw_grid_idx(uint8_t idx)
{
    const char *label = grid[idx].label;
    uint16_t bw = (uint16_t)(grid[idx].x2 - grid[idx].x1 + 1);
    uint16_t bh = (uint16_t)(grid[idx].y2 - grid[idx].y1 + 1);
    uint16_t text_w;
    uint16_t tx;
    uint16_t ty;

    if (grid_state[idx] == 1u)
    {
        label = "AR";
    }
    else if (grid_state[idx] == 2u)
    {
        label = "MR";
    }
    else if (grid_state[idx] == 3u)
    {
        label = "FAKE";
    }

    text_w = (uint16_t)(strlen(label) * (FONT_SIZE / 2));
    if (text_w > bw) text_w = bw;
    tx = (uint16_t)(grid[idx].x1 + (bw - text_w) / 2);
    ty = (uint16_t)(grid[idx].y1 + (bh - FONT_SIZE) / 2);

    lcd_fill(grid[idx].x1, grid[idx].y1, grid[idx].x2, grid[idx].y2, GREEN);
    g_back_color = GREEN;
    lcd_show_string(tx, ty, text_w, FONT_SIZE, FONT_SIZE, (char *)label, BLACK);
}

static void draw_team_buttons(void)
{
    button_t b;

    for (uint8_t i = 0; i < SEC_B_CNT; i++)
    {
        b = sec_b[i];

        if ((team_selected == 1 && i == 0) || (team_selected == 2 && i == 1))
        {
            b.fill_color = BLACK;
            b.text_color = WHITE;
        }

        static void draw_mode_button(void)
        {
            button_t b = sec_mode;

            if (app_mode == APP_MODE_RX)
            {
                b.fill_color = BLACK;
                b.text_color = WHITE;
                b.label = "Mode: RX";
            }
            else
            {
                b.fill_color = WHITE;
                b.text_color = BLACK;
                b.label = "Mode: TX";
            }

            draw_button(&b);
        }

        static void draw_rx_area(void)
        {
            uint16_t rx_w = (uint16_t)(rx_area_x2 - rx_area_x1 + 1u);
            uint16_t rx_h = (uint16_t)(rx_area_y2 - rx_area_y1 + 1u);
            uint16_t text_w = (uint16_t)(rx_w > 16u ? rx_w - 16u : rx_w);
            uint16_t text_h = (uint16_t)(rx_h > (FONT_SIZE + 16u) ? rx_h - (FONT_SIZE + 16u) : FONT_SIZE);

            lcd_fill(rx_area_x1, rx_area_y1, rx_area_x2, rx_area_y2, BLACK);
            g_back_color = BLACK;
            lcd_show_string((uint16_t)(rx_area_x1 + 8u), (uint16_t)(rx_area_y1 + 8u), text_w, FONT_SIZE, FONT_SIZE, (char *)"RX MODE", GREEN);
            lcd_show_string((uint16_t)(rx_area_x1 + 8u), (uint16_t)(rx_area_y1 + FONT_SIZE + 12u), text_w, text_h, 16, rx_text, WHITE);
        }

        draw_button(&b);
    }
}

void display_ui_init(void)
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

            strncpy(grid[idx].label, grid_labels[idx], sizeof(grid[idx].label) - 1);
            grid[idx].label[sizeof(grid[idx].label) - 1] = '\0';
        }

        rx_area_x1 = (uint16_t)(ax1 + CELL_PAD);
        rx_area_y1 = CELL_PAD;
        rx_area_x2 = (uint16_t)(w - 1u - CELL_PAD);
        rx_area_y2 = (uint16_t)(h - 1u - CELL_PAD);
    }

    /* ── Section B  (top-left quarter) ───────────────────────────────── */
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
    uint16_t cy1         = (uint16_t)(midy + BORDER_W);
    uint16_t cw2         = midx;
    uint16_t ch2         = (uint16_t)(h - cy1);
    uint16_t inner_top   = (uint16_t)(cy1 + CELL_PAD);
    uint16_t avail_h     = (uint16_t)(ch2 > 2 * CELL_PAD ? (ch2 - 2 * CELL_PAD) : 1);
    uint16_t reset_h     = (uint16_t)(avail_h / RESET_HEIGHT_RATIO_DEN);
    uint16_t mode_h      = (uint16_t)(avail_h / MODE_HEIGHT_RATIO_DEN);
    uint16_t mode_y2     = (uint16_t)(cy1 + ch2 - 1u - CELL_PAD);
    if (reset_h == 0) reset_h = 1;
    if (mode_h == 0) mode_h = 1;

    sec_reset.x1         = CELL_PAD;
    sec_reset.y1         = (uint16_t)(inner_top + (avail_h - reset_h) / 2);
    sec_reset.x2         = (uint16_t)(cw2 - 1 - CELL_PAD);
    sec_reset.y2         = (uint16_t)(sec_reset.y1 + reset_h - 1);
    sec_reset.fill_color = WHITE;
    sec_reset.text_color = BLACK;
    sec_reset.label      = "Reset";

    sec_mode.x1          = CELL_PAD;
    sec_mode.y2          = mode_y2;
    sec_mode.y1          = (uint16_t)(mode_y2 >= (mode_h - 1u) ? (mode_y2 - mode_h + 1u) : mode_y2);
    sec_mode.x2          = (uint16_t)(cw2 - 1u - CELL_PAD);
    sec_mode.fill_color  = WHITE;
    sec_mode.text_color  = BLACK;
    sec_mode.label       = "Mode: TX";

    display_ui_reset_visual_state();
}

void display_ui_draw(void)
{
    uint16_t w    = lcddev.width;
    uint16_t h    = lcddev.height;
    uint16_t midx = (uint16_t)(w / 2);
    uint16_t midy = (uint16_t)(h / 2);

    lcd_clear(BLACK);

    lcd_fill(midx, 0,
             (uint16_t)(midx + BORDER_W - 1),
             (uint16_t)(h - 1),
             WHITE);

    lcd_fill(0, midy,
             (uint16_t)(midx - 1),
             (uint16_t)(midy + BORDER_W - 1),
             WHITE);

    if (app_mode == APP_MODE_TX)
    {
        for (uint8_t i = 0; i < GRID_CELLS; i++) draw_grid_idx(i);
        draw_team_buttons();
        draw_button(&sec_reset);
    }
    else
    {
        draw_rx_area();
    }

    draw_mode_button();
}

ui_touch_id_t display_ui_get_touch_id(uint16_t x, uint16_t y)
{
    uint8_t i;

    for (i = 0; i < GRID_CELLS; i++)
    {
        if (x >= grid[i].x1 && x <= grid[i].x2 &&
            y >= grid[i].y1 && y <= grid[i].y2)
        {
            return (ui_touch_id_t)(UI_TOUCH_GRID_A + i);
        }
    }

    for (i = 0; i < SEC_B_CNT; i++)
    {
        if (x >= sec_b[i].x1 && x <= sec_b[i].x2 &&
            y >= sec_b[i].y1 && y <= sec_b[i].y2)
        {
            return (i == 0) ? UI_TOUCH_TEAM_RED : UI_TOUCH_TEAM_BLUE;
        }
    }

    if (x >= sec_reset.x1 && x <= sec_reset.x2 &&
        y >= sec_reset.y1 && y <= sec_reset.y2)
    {
        return UI_TOUCH_RESET;
    }

    if (x >= sec_mode.x1 && x <= sec_mode.x2 &&
        y >= sec_mode.y1 && y <= sec_mode.y2)
    {
        return UI_TOUCH_MODE_TOGGLE;
    }

    return UI_TOUCH_NONE;
}

void display_ui_set_grid_state(uint8_t idx, uint8_t state)
{
    if (idx >= GRID_CELLS || state > 3u)
    {
        return;
    }

    grid_state[idx] = state;
    draw_grid_idx(idx);
}

void display_ui_set_team_selection(uint8_t team)
{
    if (team > 2)
    {
        return;
    }

    team_selected = team;
    draw_team_buttons();
}

void display_ui_reset_visual_state(void)
{
    for (uint8_t i = 0; i < GRID_CELLS; i++)
    {
        grid_state[i] = 0;
    }

    team_selected = 0;
}

void display_ui_set_mode(uint8_t mode)
{
    if (mode > APP_MODE_RX)
    {
        return;
    }

    app_mode = mode;
    display_ui_draw();
}

void display_ui_set_rx_text(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    strncpy(rx_text, text, sizeof(rx_text) - 1u);
    rx_text[sizeof(rx_text) - 1u] = '\0';

    if (app_mode == APP_MODE_RX)
    {
        draw_rx_area();
        draw_mode_button();
    }
}
