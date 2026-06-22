#include "display_ui.h"
#include "lcd.h"
#include <string.h>

/* ══════════════════════════════════════════════════════════════════════════
 * Internal types
 * ══════════════════════════════════════════════════════════════════════════ */

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

/* ══════════════════════════════════════════════════════════════════════════
 * Appearance constants
 * ══════════════════════════════════════════════════════════════════════════ */

#define BORDER_W               2u
#define CELL_PAD               3u
#define FONT_SIZE              24u
#define SEC_C_VGAP_RATIO_DEN   10u

/* ── Grid (page 1) ─────────────────────────────────────────────────────── */
#define GRID_COLS   3u
#define GRID_ROWS   4u
#define GRID_CELLS  (GRID_COLS * GRID_ROWS)

/* ── Motor grid (page 2) ───────────────────────────────────────────────── */
#define MOTOR_COUNT        6u
#define MOTOR_GRID_COLS    2u
#define MOTOR_GRID_ROWS    3u

/* ── Cell-state colors ─────────────────────────────────────────────────── */
#define COLOR_AR   BROWN
#define COLOR_MR   MAGENTA
#define COLOR_FAKE BLACK

/* ── Page 2 colors ─────────────────────────────────────────────────────── */
/*
 * Motor buttons use a three-value color scheme for visual clarity:
 *
 *   OFF  — dark grey  : button is idle / not held
 *   ON   — bright green : button is actively held (momentary press)
 *   ON border — white ring drawn inside the cell boundary when held,
 *               giving immediate tactile feedback without a full repaint.
 *
 * Init All and Start Tree use their own color pairs.
 */
#define COLOR_MOTOR_OFF        0x4208u   /* dark grey        */
#define COLOR_MOTOR_ON         GREEN     /* bright green     */
#define COLOR_MOTOR_BORDER     WHITE     /* inner ring: held */

#define COLOR_INIT_OFF         0x630Cu   /* dark red         */
#define COLOR_INIT_ON          RED
#define COLOR_TREE_OFF         0x0019u   /* dark blue        */
#define COLOR_TREE_ON          BLUE

/* Thickness of the inner highlight ring drawn when a motor button is held */
#define MOTOR_PRESS_RING       3u

/* ══════════════════════════════════════════════════════════════════════════
 * Static state
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── Page 1 ────────────────────────────────────────────────────────────── */
static grid_cell_t grid[GRID_CELLS];
static button_t    sec_team;
static button_t    sec_scroll;
static button_t    sec_reset;
static button_t    sec_cam_scr;
static button_t    sec_uart;

static const char *grid_labels[GRID_CELLS] =
{
    "12", "11", "10",
     "9",  "8",  "7",
     "6",  "5",  "4",
     "3",  "2",  "1"
};

static uint8_t grid_state[GRID_CELLS];
static uint8_t team_selected;
static uint8_t scroll_mode      = DISPLAY_UI_SCROLL_AR;
static uint8_t cam_screen_state = 0u;
static uint8_t uart_send_state  = 0u;

/* ── Page 2 ────────────────────────────────────────────────────────────── */
static button_t motor_btn[MOTOR_COUNT];
static button_t btn_init_all;
static button_t btn_start_tree;

/*
 * motor_state[] — tracks the last value pushed to the display layer.
 * The UI redraw functions consult this to choose colors; the application
 * layer in main.c owns the logical state and drives changes here through
 * display_ui_set_motor_state().
 *
 * Values: 0 = off (not held), 1 = on (currently held).
 */
static uint8_t motor_state[MOTOR_COUNT];
static uint8_t init_all_state   = 0u;
static uint8_t start_tree_state = 0u;

static const char *motor_labels[MOTOR_COUNT] =
{
    "Motor 1", "Motor 2",
    "Motor 3", "Motor 4",
    "Motor 5", "Motor 6"
};

/* ── Active page ───────────────────────────────────────────────────────── */
static uint8_t active_page = DISPLAY_UI_PAGE_TX;

/* ══════════════════════════════════════════════════════════════════════════
 * Private helpers — generic button draw
 * ══════════════════════════════════════════════════════════════════════════ */

static void draw_button(const button_t *b)
{
    uint16_t bw     = (uint16_t)(b->x2 - b->x1 + 1u);
    uint16_t bh     = (uint16_t)(b->y2 - b->y1 + 1u);
    uint16_t llen   = (uint16_t)strlen(b->label);
    uint16_t text_w = (uint16_t)(llen * (FONT_SIZE / 2u));
    uint16_t text_h = FONT_SIZE;

    if (text_w > bw) text_w = bw;
    if (text_h > bh) text_h = bh;

    uint16_t tx = (uint16_t)(b->x1 + (bw - text_w) / 2u);
    uint16_t ty = (uint16_t)(b->y1 + (bh - text_h) / 2u);

    lcd_fill(b->x1, b->y1, b->x2, b->y2, b->fill_color);
    g_back_color = b->fill_color;
    lcd_show_string(tx, ty, text_w, text_h, FONT_SIZE, (char *)b->label, b->text_color);
}

/*
 * draw_button_with_ring — draws a button and, when `ring` is non-zero,
 * overlays a thin WHITE rectangle just inside the button boundary.
 *
 * This gives held motor buttons a crisp "active" highlight ring without
 * needing additional bitmap assets or a second fill pass over the full cell.
 * The ring is drawn AFTER the fill/text so it sits on top cleanly.
 *
 * The ring thickness is MOTOR_PRESS_RING pixels on all four sides.
 */
static void draw_button_with_ring(const button_t *b, uint8_t ring)
{
    draw_button(b);

    if (!ring) return;

    uint16_t r = MOTOR_PRESS_RING;

    /* Top bar */
    lcd_fill(b->x1,      b->y1,
             b->x2,      (uint16_t)(b->y1 + r - 1u),
             COLOR_MOTOR_BORDER);

    /* Bottom bar */
    lcd_fill(b->x1,      (uint16_t)(b->y2 - r + 1u),
             b->x2,      b->y2,
             COLOR_MOTOR_BORDER);

    /* Left bar */
    lcd_fill(b->x1,      (uint16_t)(b->y1 + r),
             (uint16_t)(b->x1 + r - 1u), (uint16_t)(b->y2 - r),
             COLOR_MOTOR_BORDER);

    /* Right bar */
    lcd_fill((uint16_t)(b->x2 - r + 1u), (uint16_t)(b->y1 + r),
             b->x2,                        (uint16_t)(b->y2 - r),
             COLOR_MOTOR_BORDER);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Private helpers — page 1 draw
 * ══════════════════════════════════════════════════════════════════════════ */

static void draw_grid_idx(uint8_t idx)
{
    const char *label      = grid[idx].label;
    uint32_t    fill_color = GREEN;
    uint32_t    text_color = BLACK;

    if (grid_state[idx] == 1u)
    {
        label      = "AR";
        fill_color = COLOR_AR;
        text_color = WHITE;
    }
    else if (grid_state[idx] == 2u)
    {
        label      = "MR";
        fill_color = COLOR_MR;
        text_color = WHITE;
    }
    else if (grid_state[idx] == 3u)
    {
        label      = "FAKE";
        fill_color = COLOR_FAKE;
        text_color = WHITE;
    }

    uint16_t bw     = (uint16_t)(grid[idx].x2 - grid[idx].x1 + 1u);
    uint16_t bh     = (uint16_t)(grid[idx].y2 - grid[idx].y1 + 1u);
    uint16_t text_w = (uint16_t)(strlen(label) * (FONT_SIZE / 2u));
    if (text_w > bw) text_w = bw;
    uint16_t tx = (uint16_t)(grid[idx].x1 + (bw - text_w) / 2u);
    uint16_t ty = (uint16_t)(grid[idx].y1 + (bh - FONT_SIZE) / 2u);

    lcd_fill(grid[idx].x1, grid[idx].y1, grid[idx].x2, grid[idx].y2, fill_color);
    g_back_color = fill_color;
    lcd_show_string(tx, ty, text_w, FONT_SIZE, FONT_SIZE, (char *)label, text_color);
}

static void draw_team_button(void)
{
    button_t b  = sec_team;
    b.fill_color = (team_selected == DISPLAY_UI_TEAM_BLUE) ? BLUE : RED;
    b.text_color = WHITE;
    b.label      = (team_selected == DISPLAY_UI_TEAM_BLUE) ? "Blue" : "Red";
    draw_button(&b);
}

static void draw_scroll_button(void)
{
    button_t b   = sec_scroll;
    b.text_color = WHITE;

    if (scroll_mode == DISPLAY_UI_SCROLL_MR)
    {
        b.fill_color = COLOR_MR;
        b.label      = "MR";
    }
    else if (scroll_mode == DISPLAY_UI_SCROLL_FAKE)
    {
        b.fill_color = COLOR_FAKE;
        b.label      = "FAKE";
    }
    else
    {
        b.fill_color = COLOR_AR;
        b.label      = "AR";
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
        b.label      = "SCR";
    }
    else
    {
        b.fill_color = GREEN;
        b.text_color = WHITE;
        b.label      = "CAM";
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
        b.label      = "TX ON";
    }
    else
    {
        b.fill_color = RED;
        b.text_color = WHITE;
        b.label      = "TX OFF";
    }
    draw_button(&b);
}

static void paint_tx_page(void)
{
    uint16_t w    = lcddev.width;
    uint16_t h    = lcddev.height;
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
 * Private helpers — page 2 draw
 * ══════════════════════════════════════════════════════════════════════════ */

/*
 * draw_motor_btn — repaints a single motor button to reflect its current
 * state in motor_state[].
 *
 * When the button is held (state = 1):
 *   • fill  → bright green   (COLOR_MOTOR_ON)
 *   • text  → black          (high contrast on bright background)
 *   • ring  → white inner border (MOTOR_PRESS_RING pixels thick)
 *
 * When the button is idle (state = 0):
 *   • fill  → dark grey      (COLOR_MOTOR_OFF)
 *   • text  → white
 *   • ring  → none
 *
 * The ring is added via draw_button_with_ring() so the "active" state
 * is unmistakable at a glance without requiring a full page repaint.
 *
 * This function is called only when state actually changes (0→1 or 1→0),
 * so there is no redundant redraw cost.
 */
static void draw_motor_btn(uint8_t idx)
{
    button_t b   = motor_btn[idx];
    uint8_t  on  = motor_state[idx];

    b.fill_color = on ? COLOR_MOTOR_ON  : COLOR_MOTOR_OFF;
    b.text_color = on ? BLACK           : WHITE;
    b.label      = motor_labels[idx];

    draw_button_with_ring(&b, on);
}

static void draw_init_all_btn(void)
{
    button_t b   = btn_init_all;
    b.fill_color = init_all_state ? COLOR_INIT_ON  : COLOR_INIT_OFF;
    b.text_color = WHITE;
    b.label      = "Init All";
    draw_button(&b);
}

static void draw_start_tree_btn(void)
{
    button_t b   = btn_start_tree;
    b.fill_color = start_tree_state ? COLOR_TREE_ON  : COLOR_TREE_OFF;
    b.text_color = WHITE;
    b.label      = "Start Tree";
    draw_button(&b);
}

/*
 * paint_motor_page — full repaint of page 2.
 *
 * Layout (top to bottom):
 *   1. Title bar  — "Motor Control" centred, with a WHITE underline rule
 *                   that matches the page-1 divider style.
 *   2. Motor grid — 2 × 3 buttons filling the middle section.
 *   3. Action row — Init All | Start Tree side-by-side at the bottom.
 *
 * The underline rule under the title makes the page header feel consistent
 * with the dividers on page 1, and visually separates the title from the
 * motor grid without wasting vertical space.
 */
static void paint_motor_page(void)
{
    uint16_t w = lcddev.width;

    lcd_clear(BLACK);

    /* Title */
    g_back_color = BLACK;
    lcd_show_string(0u, CELL_PAD, w, FONT_SIZE, FONT_SIZE,
                    "Motor Control", WHITE);

    /* Underline rule beneath the title — mirrors page-1 divider style */
    uint16_t rule_y = (uint16_t)(CELL_PAD + FONT_SIZE + CELL_PAD);
    lcd_fill(0u, rule_y, (uint16_t)(w - 1u), (uint16_t)(rule_y + BORDER_W - 1u), WHITE);

    for (uint8_t i = 0u; i < MOTOR_COUNT; i++)
        draw_motor_btn(i);

    draw_init_all_btn();
    draw_start_tree_btn();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Layout initialisation helpers
 * ══════════════════════════════════════════════════════════════════════════ */

static void init_page1_layout(void)
{
    uint16_t w    = lcddev.width;
    uint16_t h    = lcddev.height;
    uint16_t midx = (uint16_t)(w / 2u);
    uint16_t midy = (uint16_t)(h / 2u);

    /* ── Section A  (right half) ─────────────────────────────────────── */
    uint16_t ax1 = (uint16_t)(midx + BORDER_W);
    uint16_t aw  = (uint16_t)(w - ax1);
    uint16_t cw  = (uint16_t)(aw / GRID_COLS);
    uint16_t ch  = (uint16_t)(h / GRID_ROWS);

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
    uint16_t bw     = midx;
    uint16_t bh     = midy;
    uint16_t half_w = (uint16_t)(bw / 2u);
    uint16_t sq     = (uint16_t)((half_w < bh ? half_w : bh) - 2u * CELL_PAD);
    uint16_t bvy    = (uint16_t)((bh - sq) / 2u);

    sec_team.x1 = CELL_PAD;
    sec_team.y1 = bvy;
    sec_team.x2 = (uint16_t)(half_w - 1u - CELL_PAD);
    sec_team.y2 = (uint16_t)(bvy + sq - 1u);
    sec_team.fill_color = RED;
    sec_team.text_color = WHITE;
    sec_team.label      = "Red";

    sec_scroll.x1 = (uint16_t)(half_w + CELL_PAD);
    sec_scroll.y1 = bvy;
    sec_scroll.x2 = (uint16_t)(bw - 1u - CELL_PAD);
    sec_scroll.y2 = (uint16_t)(bvy + sq - 1u);
    sec_scroll.fill_color = COLOR_AR;
    sec_scroll.text_color = WHITE;
    sec_scroll.label      = "AR";

    /* ── Section C  (bottom-left quarter) ────────────────────────────── */
    uint16_t cy1    = (uint16_t)(midy + BORDER_W);
    uint16_t cw2    = midx;
    uint16_t ch2    = (uint16_t)(h - cy1);
    uint16_t avail_h = (uint16_t)(ch2 > 2u * CELL_PAD ? ch2 - 2u * CELL_PAD : 1u);

    uint16_t vgap = (uint16_t)(avail_h / SEC_C_VGAP_RATIO_DEN);
    if (vgap == 0u) vgap = 1u;

    uint16_t btn_h = (uint16_t)(avail_h - 2u * vgap);
    if (btn_h == 0u) btn_h = 1u;

    uint16_t y_cursor = (uint16_t)(cy1 + CELL_PAD + vgap);
    uint16_t third    = cw2 / 3u;

    sec_reset.x1  = CELL_PAD;
    sec_reset.x2  = third - CELL_PAD;

    sec_cam_scr.x1 = third + CELL_PAD;
    sec_cam_scr.x2 = 2u * third - CELL_PAD;

    sec_uart.x1 = 2u * third + CELL_PAD;
    sec_uart.x2 = cw2 - CELL_PAD;

    sec_reset.y1  = y_cursor;
    sec_reset.y2  = y_cursor + btn_h - 1u;

    sec_cam_scr.y1 = y_cursor;
    sec_cam_scr.y2 = y_cursor + btn_h - 1u;

    sec_uart.y1 = y_cursor;
    sec_uart.y2 = y_cursor + btn_h - 1u;

    sec_reset.fill_color  = WHITE;
    sec_reset.text_color  = BLACK;
    sec_reset.label       = "Reset";

    sec_cam_scr.fill_color = GREEN;
    sec_cam_scr.text_color = WHITE;
    sec_cam_scr.label      = "CAM";

    sec_uart.fill_color = RED;
    sec_uart.text_color = WHITE;
    sec_uart.label      = "TX OFF";
}

static void init_page2_layout(void)
{
    uint16_t w = lcddev.width;
    uint16_t h = lcddev.height;

    /*
     * Title bar height: top padding + font + bottom padding + underline rule.
     * This matches the values used in paint_motor_page() so geometry and
     * paint are always in sync.
     */
    uint16_t title_h = (uint16_t)(CELL_PAD + FONT_SIZE + CELL_PAD + BORDER_W + CELL_PAD);

    /* ── Motor grid  (2 columns × 3 rows) ───────────────────────────── */
    uint16_t motor_area_h = (uint16_t)(h - title_h);

    /* Bottom 25 % → action buttons; top 75 % → motor grid */
    uint16_t action_h     = (uint16_t)(motor_area_h / 4u);
    uint16_t motor_grid_h = (uint16_t)(motor_area_h - action_h);

    uint16_t cw = (uint16_t)(w / MOTOR_GRID_COLS);
    uint16_t ch = (uint16_t)(motor_grid_h / MOTOR_GRID_ROWS);

    for (uint8_t r = 0u; r < MOTOR_GRID_ROWS; r++)
    {
        for (uint8_t c = 0u; c < MOTOR_GRID_COLS; c++)
        {
            uint8_t idx    = (uint8_t)(r * MOTOR_GRID_COLS + c);
            motor_btn[idx].x1 = (uint16_t)(c * cw + CELL_PAD);
            motor_btn[idx].y1 = (uint16_t)(title_h + r * ch + CELL_PAD);
            motor_btn[idx].x2 = (uint16_t)((c + 1u) * cw - 1u - CELL_PAD);
            motor_btn[idx].y2 = (uint16_t)(title_h + (r + 1u) * ch - 1u - CELL_PAD);
            motor_btn[idx].fill_color = COLOR_MOTOR_OFF;
            motor_btn[idx].text_color = WHITE;
            motor_btn[idx].label      = motor_labels[idx];
        }
    }

    /* ── Action buttons row ─────────────────────────────────────────── */
    uint16_t action_y1 = (uint16_t)(title_h + motor_grid_h + CELL_PAD);
    uint16_t action_y2 = (uint16_t)(h - 1u - CELL_PAD);
    uint16_t half      = (uint16_t)(w / 2u);

    btn_init_all.x1 = CELL_PAD;
    btn_init_all.y1 = action_y1;
    btn_init_all.x2 = half - CELL_PAD;
    btn_init_all.y2 = action_y2;
    btn_init_all.fill_color = COLOR_INIT_OFF;
    btn_init_all.text_color = WHITE;
    btn_init_all.label      = "Init All";

    btn_start_tree.x1 = half + CELL_PAD;
    btn_start_tree.y1 = action_y1;
    btn_start_tree.x2 = (uint16_t)(w - 1u - CELL_PAD);
    btn_start_tree.y2 = action_y2;
    btn_start_tree.fill_color = COLOR_TREE_OFF;
    btn_start_tree.text_color = WHITE;
    btn_start_tree.label      = "Start Tree";
}

/* ══════════════════════════════════════════════════════════════════════════
 * Public API
 * ══════════════════════════════════════════════════════════════════════════ */

void display_ui_init(void)
{
    init_page1_layout();
    init_page2_layout();
    display_ui_reset_visual_state();
    active_page = DISPLAY_UI_PAGE_TX;
}

void display_ui_draw(void)
{
    if (active_page == DISPLAY_UI_PAGE_MOTOR)
        paint_motor_page();
    else
        paint_tx_page();
}

/* ── Page navigation ────────────────────────────────────────────────────── */

void display_ui_set_page(uint8_t page)
{
    if (page > DISPLAY_UI_PAGE_MOTOR)
        return;
    active_page = page;
    display_ui_draw();
}

uint8_t display_ui_get_page(void)
{
    return active_page;
}

/* ── Hit-testing ─────────────────────────────────────────────────────────
 *  Only tests elements on the currently active page so that overlapping
 *  coordinate ranges on different pages never produce false hits.
 * ──────────────────────────────────────────────────────────────────────── */

ui_touch_id_t display_ui_get_touch_id(uint16_t x, uint16_t y)
{
    if (active_page == DISPLAY_UI_PAGE_TX)
    {
        /* Grid cells */
        for (uint8_t i = 0u; i < GRID_CELLS; i++)
        {
            if (x >= grid[i].x1 && x <= grid[i].x2 &&
                y >= grid[i].y1 && y <= grid[i].y2)
            {
                return (ui_touch_id_t)(UI_TOUCH_GRID_A + i);
            }
        }

        if (x >= sec_team.x1   && x <= sec_team.x2   && y >= sec_team.y1   && y <= sec_team.y2)
            return UI_TOUCH_TEAM_TOGGLE;
        if (x >= sec_scroll.x1 && x <= sec_scroll.x2 && y >= sec_scroll.y1 && y <= sec_scroll.y2)
            return UI_TOUCH_SCROLL_MODE;
        if (x >= sec_reset.x1  && x <= sec_reset.x2  && y >= sec_reset.y1  && y <= sec_reset.y2)
            return UI_TOUCH_RESET;
        if (x >= sec_cam_scr.x1 && x <= sec_cam_scr.x2 &&
            y >= sec_cam_scr.y1 && y <= sec_cam_scr.y2)
            return UI_TOUCH_CAM_SCREEN;
        if (x >= sec_uart.x1 && x <= sec_uart.x2 &&
            y >= sec_uart.y1 && y <= sec_uart.y2)
            return UI_TOUCH_UART_SEND;
    }
    else  /* DISPLAY_UI_PAGE_MOTOR */
    {
        for (uint8_t i = 0u; i < MOTOR_COUNT; i++)
        {
            if (x >= motor_btn[i].x1 && x <= motor_btn[i].x2 &&
                y >= motor_btn[i].y1 && y <= motor_btn[i].y2)
            {
                return (ui_touch_id_t)(UI_TOUCH_MOTOR_1 + i);
            }
        }

        if (x >= btn_init_all.x1   && x <= btn_init_all.x2   &&
            y >= btn_init_all.y1   && y <= btn_init_all.y2)
            return UI_TOUCH_INIT_ALL;

        if (x >= btn_start_tree.x1 && x <= btn_start_tree.x2 &&
            y >= btn_start_tree.y1 && y <= btn_start_tree.y2)
            return UI_TOUCH_START_TREE;
    }

    return UI_TOUCH_NONE;
}

/* ── Page 1 state setters ───────────────────────────────────────────────── */

void display_ui_set_grid_state(uint8_t idx, uint8_t state)
{
    if (idx >= GRID_CELLS || state > 3u)
        return;
    grid_state[idx] = state;
    if (active_page == DISPLAY_UI_PAGE_TX)
        draw_grid_idx(idx);
}

void display_ui_set_team_selection(uint8_t team)
{
    if (team > DISPLAY_UI_TEAM_BLUE)
        return;
    team_selected = team;
    if (active_page == DISPLAY_UI_PAGE_TX)
        draw_team_button();
}

void display_ui_set_scroll_mode(uint8_t mode)
{
    if (mode != DISPLAY_UI_SCROLL_AR &&
        mode != DISPLAY_UI_SCROLL_MR &&
        mode != DISPLAY_UI_SCROLL_FAKE)
        return;
    scroll_mode = mode;
    if (active_page == DISPLAY_UI_PAGE_TX)
        draw_scroll_button();
}

void display_ui_set_cam_screen(uint8_t state)
{
    cam_screen_state = state;
    if (active_page == DISPLAY_UI_PAGE_TX)
        draw_cam_scr_button();
}

void display_ui_set_uart_send(uint8_t state)
{
    uart_send_state = state;
    if (active_page == DISPLAY_UI_PAGE_TX)
        draw_uart_button();
}

void display_ui_reset_visual_state(void)
{
    /* Page 1 */
    for (uint8_t i = 0u; i < GRID_CELLS; i++)
        grid_state[i] = 0u;
    team_selected   = DISPLAY_UI_TEAM_RED;
    scroll_mode     = DISPLAY_UI_SCROLL_AR;
    cam_screen_state = 0u;
    uart_send_state  = 0u;

    /* Page 2 */
    for (uint8_t i = 0u; i < MOTOR_COUNT; i++)
        motor_state[i] = 0u;
    init_all_state   = 0u;
    start_tree_state = 0u;
}

/* ── Page 2 state setters ───────────────────────────────────────────────── */

/*
 * display_ui_set_motor_state — update the visual state of one motor button.
 *
 * This is called by main.c exactly once per state transition:
 *   • Once on finger-down  (0 → 1) via motor_press()
 *   • Once on finger-up    (1 → 0) via motor_release()
 *
 * No change-guard is needed here because main.c's motor_press() /
 * motor_release() functions already guard the call site with g_motor_state[].
 * We update our local mirror and repaint if the page is active.
 */
void display_ui_set_motor_state(uint8_t motor_idx, uint8_t active)
{
    if (motor_idx >= MOTOR_COUNT)
        return;
    motor_state[motor_idx] = active ? 1u : 0u;
    if (active_page == DISPLAY_UI_PAGE_MOTOR)
        draw_motor_btn(motor_idx);
}

void display_ui_set_init_all_state(uint8_t active)
{
    init_all_state = active ? 1u : 0u;
    if (active_page == DISPLAY_UI_PAGE_MOTOR)
        draw_init_all_btn();
}

void display_ui_set_start_tree_state(uint8_t active)
{
    start_tree_state = active ? 1u : 0u;
    if (active_page == DISPLAY_UI_PAGE_MOTOR)
        draw_start_tree_btn();
}