#include "display_ui.h"
#include "lcd.h"
#include <string.h>

/* ── Internal types ──────────────────────────────────────────────────────── */

typedef struct
{
    uint16_t x1, y1, x2, y2;
    char label[5];
} grid_cell_t;

typedef struct
{
    uint16_t x1, y1, x2, y2;
    uint32_t fill_color;
    uint32_t text_color;
    const char *label;
} button_t;

/* ── Appearance constants ────────────────────────────────────────────────── */

#define BORDER_W 2u
#define CELL_PAD 3u
#define FONT_SIZE 24u
#define SEC_C_VGAP_RATIO_DEN 10u

/* ── Grid dimensions ───────────────────────────────────────────────────── */

#define GRID_COLS 3u
#define GRID_ROWS 4u
#define GRID_CELLS (GRID_COLS * GRID_ROWS)

/* ── Cell-state colors ─────────────────────────────────────────────────── */

#define COLOR_AR BROWN
#define COLOR_MR MAGENTA
#define COLOR_FAKE BLACK

/* ── Static state ───────────────────────────────────────────────────────── */

static grid_cell_t grid[GRID_CELLS];
static button_t sec_team;
static button_t sec_scroll;
static button_t sec_reset;
static button_t sec_cam_scr;

static const char *grid_labels[GRID_CELLS] =
    {
        "12", "11", "10",
        "9", "8", "7",
        "6", "5", "4",
        "3", "2", "1"};

static uint8_t grid_state[GRID_CELLS];
static uint8_t team_selected;
static uint8_t scroll_mode = DISPLAY_UI_SCROLL_AR;
static uint8_t cam_screen_state = 0u;
static button_t sec_uart;
static uint8_t uart_send_state = 0u;
/* ══════════════════════════════════════════════════════════════════════════
 * Private draw helpers
 * ══════════════════════════════════════════════════════════════════════════ */

static void draw_button(const button_t *b)
{
    uint16_t bw = (uint16_t)(b->x2 - b->x1 + 1u);
    uint16_t bh = (uint16_t)(b->y2 - b->y1 + 1u);
    uint16_t llen = (uint16_t)strlen(b->label);
    uint16_t text_w = (uint16_t)(llen * (FONT_SIZE / 2u));
    uint16_t text_h = FONT_SIZE;

    if (text_w > bw)
        text_w = bw;
    if (text_h > bh)
        text_h = bh;

    uint16_t tx = (uint16_t)(b->x1 + (bw - text_w) / 2u);
    uint16_t ty = (uint16_t)(b->y1 + (bh - text_h) / 2u);

    lcd_fill(b->x1, b->y1, b->x2, b->y2, b->fill_color);
    g_back_color = b->fill_color;
    lcd_show_string(tx, ty, text_w, text_h, FONT_SIZE, (char *)b->label, b->text_color);
}

static void draw_grid_idx(uint8_t idx)
{
    const char *label = grid[idx].label;
    uint32_t fill_color = GREEN;
    uint32_t text_color = BLACK;

    if (grid_state[idx] == 1u)
    {
        label = "AR";
        fill_color = COLOR_AR;
        text_color = WHITE;
    }
    else if (grid_state[idx] == 2u)
    {
        label = "MR";
        fill_color = COLOR_MR;
        text_color = WHITE;
    }
    else if (grid_state[idx] == 3u)
    {
        label = "FAKE";
        fill_color = COLOR_FAKE;
        text_color = WHITE;
    }

    uint16_t bw = (uint16_t)(grid[idx].x2 - grid[idx].x1 + 1u);
    uint16_t bh = (uint16_t)(grid[idx].y2 - grid[idx].y1 + 1u);
    uint16_t text_w = (uint16_t)(strlen(label) * (FONT_SIZE / 2u));
    if (text_w > bw)
        text_w = bw;
    uint16_t tx = (uint16_t)(grid[idx].x1 + (bw - text_w) / 2u);
    uint16_t ty = (uint16_t)(grid[idx].y1 + (bh - FONT_SIZE) / 2u);

    lcd_fill(grid[idx].x1, grid[idx].y1, grid[idx].x2, grid[idx].y2, fill_color);
    g_back_color = fill_color;
    lcd_show_string(tx, ty, text_w, FONT_SIZE, FONT_SIZE, (char *)label, text_color);
}

static void draw_team_button(void)
{
    button_t b = sec_team;
    b.fill_color = (team_selected == DISPLAY_UI_TEAM_BLUE) ? BLUE : RED;
    b.text_color = WHITE;
    b.label = (team_selected == DISPLAY_UI_TEAM_BLUE) ? "Blue" : "Red";
    draw_button(&b);
}

static void draw_scroll_button(void)
{
    button_t b = sec_scroll;
    b.text_color = WHITE;
    if (scroll_mode == DISPLAY_UI_SCROLL_MR)
    {
        b.fill_color = COLOR_MR;
        b.label = "MR";
    }
    else if (scroll_mode == DISPLAY_UI_SCROLL_FAKE)
    {
        b.fill_color = COLOR_FAKE;
        b.label = "FAKE";
    }
    else
    {
        b.fill_color = COLOR_AR;
        b.label = "AR";
    }
    draw_button(&b);
}
static void draw_cam_scr_button(void)
{
    button_t b = sec_cam_scr;

    if (cam_screen_state)
    {
        b.fill_color = BLUE;
        b.text_color = WHITE;
        b.label = "SCR";
    }
    else
    {
        b.fill_color = GREEN;
        b.text_color = WHITE;
        b.label = "CAM";
    }

    draw_button(&b);
}
static void draw_uart_button(void)
{
    button_t b = sec_uart;

    if (uart_send_state)
    {
        b.fill_color = GREEN;
        b.text_color = BLACK;
        b.label = "TX ON";
    }
    else
    {
        b.fill_color = RED;
        b.text_color = WHITE;
        b.label = "TX OFF";
    }

    draw_button(&b);
}
static void paint_tx_page(void)
{
    uint16_t w = lcddev.width;
    uint16_t h = lcddev.height;
    uint16_t midx = (uint16_t)(w / 2u);
    uint16_t midy = (uint16_t)(h / 2u);

    lcd_clear(BLACK);

    /* Section dividers */
    lcd_fill(midx, 0u,
             (uint16_t)(midx + BORDER_W - 1u), (uint16_t)(h - 1u), WHITE);
    lcd_fill(0u, midy,
             (uint16_t)(midx - 1u), (uint16_t)(midy + BORDER_W - 1u), WHITE);

    for (uint8_t i = 0u; i < GRID_CELLS; i++)
        draw_grid_idx(i);
    draw_team_button();
    draw_scroll_button();
    draw_button(&sec_reset);
    draw_cam_scr_button();
    draw_uart_button();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Public API
 * ══════════════════════════════════════════════════════════════════════════ */

void display_ui_init(void)
{
    uint16_t w = lcddev.width;
    uint16_t h = lcddev.height;
    uint16_t midx = (uint16_t)(w / 2u);
    uint16_t midy = (uint16_t)(h / 2u);

    /* ── Section A  (right half) ─────────────────────────────────────── */
    uint16_t ax1 = (uint16_t)(midx + BORDER_W);
    uint16_t aw = (uint16_t)(w - ax1);
    uint16_t cw = (uint16_t)(aw / GRID_COLS);
    uint16_t ch = (uint16_t)(h / GRID_ROWS);

    for (uint8_t r = 0u; r < GRID_ROWS; r++)
    {
        for (uint8_t c = 0u; c < GRID_COLS; c++)
        {
            uint8_t idx = (uint8_t)(r * GRID_COLS + c);
            grid[idx].x1 = (uint16_t)(ax1 + c * cw + CELL_PAD);
            grid[idx].y1 = (uint16_t)(r * ch + CELL_PAD);
            grid[idx].x2 = (uint16_t)(ax1 + (c + 1u) * cw - 1u - CELL_PAD);
            grid[idx].y2 = (uint16_t)((r + 1u) * ch - 1u - CELL_PAD);
            strncpy(grid[idx].label, grid_labels[idx], sizeof(grid[idx].label) - 1u);
            grid[idx].label[sizeof(grid[idx].label) - 1u] = '\0';
        }
    }

    /* ── Section B  (top-left quarter) ──────────────────────────────── */
    uint16_t bw = midx;
    uint16_t bh = midy;
    uint16_t half_w = (uint16_t)(bw / 2u);
    uint16_t sq = (uint16_t)((half_w < bh ? half_w : bh) - 2u * CELL_PAD);
    uint16_t bvy = (uint16_t)((bh - sq) / 2u);

    sec_team.x1 = CELL_PAD;
    sec_team.y1 = bvy;
    sec_team.x2 = (uint16_t)(half_w - 1u - CELL_PAD);
    sec_team.y2 = (uint16_t)(bvy + sq - 1u);
    sec_team.fill_color = RED;
    sec_team.text_color = WHITE;
    sec_team.label = "Red";

    sec_scroll.x1 = (uint16_t)(half_w + CELL_PAD);
    sec_scroll.y1 = bvy;
    sec_scroll.x2 = (uint16_t)(bw - 1u - CELL_PAD);
    sec_scroll.y2 = (uint16_t)(bvy + sq - 1u);
    sec_scroll.fill_color = COLOR_AR;
    sec_scroll.text_color = WHITE;
    sec_scroll.label = "AR";

    /* ── Section C  (bottom-left quarter) ────────────────────────────── */
    uint16_t cy1 = (uint16_t)(midy + BORDER_W);
    uint16_t cw2 = midx;
    uint16_t ch2 = (uint16_t)(h - cy1);
    uint16_t avail_h = (uint16_t)(ch2 > 2u * CELL_PAD ? ch2 - 2u * CELL_PAD : 1u);

    uint16_t vgap = (uint16_t)(avail_h / SEC_C_VGAP_RATIO_DEN);
    if (vgap == 0u)
        vgap = 1u;

    uint16_t btn_h = (uint16_t)(avail_h - 2u * vgap);
    if (btn_h == 0u)
        btn_h = 1u;

    uint16_t y_cursor = (uint16_t)(cy1 + CELL_PAD + vgap);

    uint16_t third = cw2 / 3u;
    sec_reset.x1 = CELL_PAD;
    sec_reset.x2 = third - CELL_PAD;

    sec_cam_scr.x1 = third + CELL_PAD;
    sec_cam_scr.x2 = 2u * third - CELL_PAD;

    sec_uart.x1 = 2u * third + CELL_PAD;
    sec_uart.x2 = cw2 - CELL_PAD;
    sec_reset.y1 = y_cursor;
    sec_reset.y2 = y_cursor + btn_h - 1u;

    sec_cam_scr.y1 = y_cursor;
    sec_cam_scr.y2 = y_cursor + btn_h - 1u;
    sec_uart.y1 = y_cursor;
    sec_uart.y2 = y_cursor + btn_h - 1u;
    sec_reset.fill_color = WHITE;
    sec_reset.text_color = BLACK;
    sec_reset.label = "Reset";
    sec_cam_scr.fill_color = GREEN;
    sec_cam_scr.text_color = WHITE;
    sec_cam_scr.label = "CAM";
    sec_uart.fill_color = RED;
    sec_uart.text_color = WHITE;
    sec_uart.label = "TX OFF";

    display_ui_reset_visual_state();
}

void display_ui_draw(void)
{
    paint_tx_page();
}

ui_touch_id_t display_ui_get_touch_id(uint16_t x, uint16_t y)
{
    for (uint8_t i = 0u; i < GRID_CELLS; i++)
    {
        if (x >= grid[i].x1 && x <= grid[i].x2 &&
            y >= grid[i].y1 && y <= grid[i].y2)
        {
            return (ui_touch_id_t)(UI_TOUCH_GRID_A + i);
        }
    }

    if (x >= sec_team.x1 && x <= sec_team.x2 && y >= sec_team.y1 && y <= sec_team.y2)
        return UI_TOUCH_TEAM_TOGGLE;
    if (x >= sec_scroll.x1 && x <= sec_scroll.x2 && y >= sec_scroll.y1 && y <= sec_scroll.y2)
        return UI_TOUCH_SCROLL_MODE;
    if (x >= sec_reset.x1 && x <= sec_reset.x2 && y >= sec_reset.y1 && y <= sec_reset.y2)
        return UI_TOUCH_RESET;
    if (x >= sec_cam_scr.x1 &&
        x <= sec_cam_scr.x2 &&
        y >= sec_cam_scr.y1 &&
        y <= sec_cam_scr.y2)
    {
        return UI_TOUCH_CAM_SCREEN;
    }
    if (x >= sec_uart.x1 &&
        x <= sec_uart.x2 &&
        y >= sec_uart.y1 &&
        y <= sec_uart.y2)
    {
        return UI_TOUCH_UART_SEND;
    }

    return UI_TOUCH_NONE;
}

void display_ui_set_grid_state(uint8_t idx, uint8_t state)
{
    if (idx >= GRID_CELLS || state > 3u)
        return;
    grid_state[idx] = state;
    draw_grid_idx(idx);
}

void display_ui_set_team_selection(uint8_t team)
{
    if (team > DISPLAY_UI_TEAM_BLUE)
        return;
    team_selected = team;
    draw_team_button();
}

void display_ui_set_scroll_mode(uint8_t mode)
{
    if (mode != DISPLAY_UI_SCROLL_AR &&
        mode != DISPLAY_UI_SCROLL_MR &&
        mode != DISPLAY_UI_SCROLL_FAKE)
        return;
    scroll_mode = mode;
    draw_scroll_button();
}
void display_ui_set_cam_screen(uint8_t state)
{
    cam_screen_state = state;
    draw_cam_scr_button();
}
void display_ui_reset_visual_state(void)
{
    for (uint8_t i = 0u; i < GRID_CELLS; i++)
        grid_state[i] = 0u;
    team_selected = DISPLAY_UI_TEAM_RED;
    scroll_mode = DISPLAY_UI_SCROLL_AR;
    cam_screen_state = 0u;
    uart_send_state = 0u;
    
}
void display_ui_set_uart_send(uint8_t state)
{
    uart_send_state = state;
    draw_uart_button();
}
