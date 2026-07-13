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

/* Reserved touch-free strip at the very bottom of pages 1 (TX) and 2 (Motor)
 * so a swipe started low on the screen never lands on a button first. */
#define SWIPE_MARGIN_H         40u

/* ── Grid (page 2) ─────────────────────────────────────────────────────── */
#define GRID_COLS   3u
#define GRID_ROWS   4u
#define GRID_CELLS  (GRID_COLS * GRID_ROWS)

/* ── Motor grid (page 1) ───────────────────────────────────────────────── */
#define MOTOR_COUNT        7u
#define MOTOR_GRID_COLS    2u
#define MOTOR_GRID_ROWS    3u

/* ── Tree control grid (page 0) ────────────────────────────────────────── */
#define TREE_CTRL_COUNT 4u
#define TREE_CTRL_COLS  2u
#define TREE_CTRL_ROWS  2u

/* ── Lift / tic-tac-toe grid (page 3) ─────────────────────────────────── */
#define TTT_COLS  3u
#define TTT_ROWS  3u
#define TTT_CELLS (TTT_COLS * TTT_ROWS)
#define TTT_TOUCH_CELLS 6u  /* top + middle rows only */

/* ── Cell-state colors ─────────────────────────────────────────────────── */
#define COLOR_AR   BROWN
#define COLOR_MR   MAGENTA
#define COLOR_FAKE BLACK

/* ── Page 1/0 colors ───────────────────────────────────────────────────── */
#define COLOR_MOTOR_OFF    0x4208u   /* dark grey  */
#define COLOR_MOTOR_ON     GREEN
#define COLOR_INIT_OFF     0x630Cu   /* dark red   */
#define COLOR_INIT_ON      RED
#define COLOR_TREE_OFF     0x0019u   /* dark blue  */
#define COLOR_TREE_ON      BLUE
#define COLOR_TREE_STOP_OFF 0x630Cu
#define COLOR_TREE_STOP_ON  RED

/* ── Page 3 (Lift) colors ─────────────────────────────────────────────── */
#define COLOR_LIFT_OFF     0x630Cu   /* dark red   */
#define COLOR_LIFT_ON      GREEN
#define COLOR_TTT_OFF      BROWN     /* brown, inactive / disabled cell  */
#define COLOR_TTT_ON       0x5140u   /* darker brown, active cell        */
#define COLOR_PAGE0_BTN_OFF 0x8410u  /* gray */

/* ══════════════════════════════════════════════════════════════════════════
 * Static state
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── Page 2 ────────────────────────────────────────────────────────────── */
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

/* ── Page 1 ────────────────────────────────────────────────────────────── */
static button_t motor_btn[MOTOR_COUNT];
static button_t btn_init_all;

static uint8_t motor_state[MOTOR_COUNT];   /* 0 = off, 1 = on */
static uint8_t init_all_state   = 0u;

/* ── Page 0 ────────────────────────────────────────────────────────────── */
static button_t tree_ctrl_btn[TREE_CTRL_COUNT];
static uint8_t tree_start_state   = 0u;
static uint8_t tree_stop_state    = 0u;
static uint8_t bringup_start_state = 0u;
static uint8_t bringup_stop_state  = 0u;

static const char *motor_labels[MOTOR_COUNT] =
{
    "Motor 1", "Motor 2",
    "Motor 3", "Motor 4",
    "Motor 5", "Motor 6",
    "Motor 7"
};

/* ── Page 3 (Lift) ─────────────────────────────────────────────────────── */
static button_t  btn_lift;
static button_t  btn_start;
static button_t  btn_retry1;
static button_t  btn_retry2;
static button_t  ttt_cell[TTT_CELLS];

/*
 * Row-major, top-left to bottom-right, matching the user-facing numbering:
 *   row0 (top):    9 8 7
 *   row1 (middle): 6 5 4
 *   row2 (bottom): 3 2 1
 */
static const uint8_t ttt_cell_number[TTT_CELLS] =
{
    9u, 8u, 7u,
    6u, 5u, 4u,
    3u, 2u, 1u
};

static uint8_t lift_state    = 0u;   /* 0 = Dropped, 1 = Lifted          */
static uint8_t start_state   = 0u;
static uint8_t retry1_state  = 0u;
static uint8_t retry2_state  = 0u;
static uint8_t ttt_mid_state = 0u;   /* 0=none, 1=cell4, 2=cell5, 3=cell6 */
static uint8_t ttt_top_state = 0u;   /* 0=none, 1=cell7, 2=cell8, 3=cell9 */

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
    if (team_selected == DISPLAY_UI_TEAM_BLUE)
    {
        b.fill_color = BLUE;
        b.text_color = WHITE;
        b.label      = "Blue";
    }
    else if (team_selected == DISPLAY_UI_TEAM_RED)
    {
        b.fill_color = RED;
        b.text_color = WHITE;
        b.label      = "Red";
    }
    else
    {
        b.fill_color = WHITE;
        b.text_color = BLACK;
        b.label      = "Choose Color";
    }
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

static void draw_motor_btn(uint8_t idx)
{
    button_t b   = motor_btn[idx];
    b.fill_color = motor_state[idx] ? COLOR_MOTOR_ON  : COLOR_MOTOR_OFF;
    b.text_color = motor_state[idx] ? BLACK            : WHITE;
    b.label      = motor_labels[idx];
    draw_button(&b);
}

static void draw_init_all_btn(void)
{
    button_t b   = btn_init_all;
    b.fill_color = init_all_state ? COLOR_INIT_ON  : COLOR_INIT_OFF;
    b.text_color = WHITE;
    b.label      = "Init All";
    draw_button(&b);
}

static void draw_tree_ctrl_btn(uint8_t idx, uint8_t active)
{
    static const char *labels[TREE_CTRL_COUNT] =
    {
        "Start Tree",
        "Stop Tree",
        "Start Bringup",
        "Stop Bringup"
    };

    button_t b = tree_ctrl_btn[idx];
    if (idx == 0u || idx == 2u)
        b.fill_color = active ? COLOR_TREE_ON : COLOR_TREE_OFF;
    else
        b.fill_color = active ? COLOR_TREE_STOP_ON : COLOR_TREE_STOP_OFF;
    b.text_color = WHITE;
    b.label = labels[idx];
    draw_button(&b);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Private helpers — page 0 (Lift) draw
 * ══════════════════════════════════════════════════════════════════════════ */

static uint8_t ttt_cell_is_active(uint8_t number)
{
    switch (number)
    {
        case 4u: return (ttt_mid_state == 1u);
        case 5u: return (ttt_mid_state == 2u);
        case 6u: return (ttt_mid_state == 3u);
        case 7u: return (ttt_top_state == 1u);
        case 8u: return (ttt_top_state == 2u);
        case 9u: return (ttt_top_state == 3u);
        default: return 0u;   /* bottom row (1/2/3) never active */
    }
}

static void draw_start_button(void)
{
    button_t b = btn_start;
    b.fill_color = start_state ? BLUE : COLOR_PAGE0_BTN_OFF;
    b.text_color = WHITE;
    b.label      = "Start";
    draw_button(&b);
}

static void draw_retry1_button(void)
{
    button_t b = btn_retry1;
    b.fill_color = retry1_state ? GREEN : COLOR_PAGE0_BTN_OFF;
    b.text_color = WHITE;
    b.label      = "Retry1";
    draw_button(&b);
}

static void draw_retry2_button(void)
{
    button_t b = btn_retry2;
    b.fill_color = retry2_state ? RED : COLOR_PAGE0_BTN_OFF;
    b.text_color = WHITE;
    b.label      = "Retry2";
    draw_button(&b);
}

static void draw_lift_button(void)
{
    button_t b = btn_lift;
    b.fill_color = lift_state ? COLOR_LIFT_ON : COLOR_LIFT_OFF;
    b.text_color = WHITE;
    b.label      = lift_state ? "ON" : "DEFAULT";
    draw_button(&b);
}

static void draw_ttt_idx(uint8_t idx)
{
    char label[4];
    uint8_t number = ttt_cell_number[idx];
    uint8_t active = ttt_cell_is_active(number);

    button_t b   = ttt_cell[idx];
    b.fill_color = active ? COLOR_TTT_ON : COLOR_TTT_OFF;
    b.text_color = WHITE;

    label[0] = (char)('0' + number);
    label[1] = '\0';
    b.label  = label;

    draw_button(&b);
}

static void paint_lift_page(void)
{
    uint16_t w    = lcddev.width;
    uint16_t h    = (uint16_t)(lcddev.height - SWIPE_MARGIN_H);
    uint16_t midx = (uint16_t)(w / 2u);

    lcd_clear(BLACK);

    /* Section divider between left half (lift) and right half (grid) */
    lcd_fill(midx, 0u,
             (uint16_t)(midx + BORDER_W - 1u), (uint16_t)(h - 1u), WHITE);

    draw_start_button();
    draw_retry1_button();
    draw_retry2_button();
    draw_lift_button();

    for (uint8_t i = 0u; i < TTT_TOUCH_CELLS; i++)
        draw_ttt_idx(i);
}

static void paint_tree_page(void)
{
    lcd_clear(BLACK);

    uint16_t w = lcddev.width;
    g_back_color = BLACK;
    lcd_show_string(0u, 0u, w, FONT_SIZE, FONT_SIZE,
                    "Tree Control", WHITE);

    draw_tree_ctrl_btn(0u, tree_start_state);
    draw_tree_ctrl_btn(1u, tree_stop_state);
    draw_tree_ctrl_btn(2u, bringup_start_state);
    draw_tree_ctrl_btn(3u, bringup_stop_state);
}

static void paint_motor_page(void)
{
    lcd_clear(BLACK);

    /* Draw page title */
    uint16_t w = lcddev.width;
    g_back_color = BLACK;
    lcd_show_string(0u, 0u, w, FONT_SIZE, FONT_SIZE,
                    "Motor Control", WHITE);

    for (uint8_t i = 0u; i < MOTOR_COUNT; i++)
        draw_motor_btn(i);

    draw_init_all_btn();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Layout initialisation helpers
 * ══════════════════════════════════════════════════════════════════════════ */

static void init_page0_layout(void)
{
    uint16_t w = lcddev.width;
    uint16_t h = (uint16_t)(lcddev.height - SWIPE_MARGIN_H);
    uint16_t title_h = (uint16_t)(FONT_SIZE + CELL_PAD * 2u);
    uint16_t area_h = (uint16_t)(h - title_h);
    uint16_t cw = (uint16_t)(w / TREE_CTRL_COLS);
    uint16_t ch = (uint16_t)(area_h / TREE_CTRL_ROWS);

    for (uint8_t r = 0u; r < TREE_CTRL_ROWS; r++)
    {
        for (uint8_t c = 0u; c < TREE_CTRL_COLS; c++)
        {
            uint8_t idx = (uint8_t)(r * TREE_CTRL_COLS + c);
            tree_ctrl_btn[idx].x1 = (uint16_t)(c * cw + CELL_PAD);
            tree_ctrl_btn[idx].y1 = (uint16_t)(title_h + r * ch + CELL_PAD);
            tree_ctrl_btn[idx].x2 = (uint16_t)((c + 1u) * cw - 1u - CELL_PAD);
            tree_ctrl_btn[idx].y2 = (uint16_t)(title_h + (r + 1u) * ch - 1u - CELL_PAD);
            tree_ctrl_btn[idx].fill_color = (idx == 0u || idx == 2u) ? COLOR_TREE_OFF : COLOR_TREE_STOP_OFF;
            tree_ctrl_btn[idx].text_color = WHITE;
            tree_ctrl_btn[idx].label = "";
        }
    }
}

static void init_page3_layout(void)
{
    uint16_t w    = lcddev.width;
    uint16_t h    = (uint16_t)(lcddev.height - SWIPE_MARGIN_H);
    uint16_t midx = (uint16_t)(w / 2u);
    uint16_t lw   = midx;
    uint16_t rwx1 = (uint16_t)(midx + BORDER_W);
    uint16_t rw   = (uint16_t)(w - rwx1);
    uint16_t row_h = (uint16_t)(h / TTT_ROWS);

    /* ── Left half — Start / Retry1 / Retry2 ──────────────────────────── */
    uint16_t left_btn_h = (uint16_t)(h / 3u);
    uint16_t left_x1 = CELL_PAD;
    uint16_t left_x2 = (uint16_t)(lw - 1u - CELL_PAD);

    btn_start.x1 = left_x1;
    btn_start.x2 = left_x2;
    btn_start.y1 = CELL_PAD;
    btn_start.y2 = (uint16_t)(left_btn_h - 1u - CELL_PAD);
    btn_start.fill_color = COLOR_PAGE0_BTN_OFF;
    btn_start.text_color = WHITE;
    btn_start.label      = "Start";

    btn_retry1.x1 = left_x1;
    btn_retry1.x2 = left_x2;
    btn_retry1.y1 = (uint16_t)(left_btn_h + CELL_PAD);
    btn_retry1.y2 = (uint16_t)(2u * left_btn_h - 1u - CELL_PAD);
    btn_retry1.fill_color = COLOR_PAGE0_BTN_OFF;
    btn_retry1.text_color = WHITE;
    btn_retry1.label      = "Retry1";

    btn_retry2.x1 = left_x1;
    btn_retry2.x2 = left_x2;
    btn_retry2.y1 = (uint16_t)(2u * left_btn_h + CELL_PAD);
    btn_retry2.y2 = (uint16_t)(h - 1u - CELL_PAD);
    btn_retry2.fill_color = COLOR_PAGE0_BTN_OFF;
    btn_retry2.text_color = WHITE;
    btn_retry2.label      = "Retry2";

    /* ── Right half — top/middle grid cells 9..4 ──────────────────────── */
    uint16_t cw  = (uint16_t)(rw / TTT_COLS);
    uint16_t ch  = row_h;

    for (uint8_t r = 0u; r < 2u; r++)
    {
        for (uint8_t c = 0u; c < TTT_COLS; c++)
        {
            uint8_t idx = (uint8_t)(r * TTT_COLS + c);
            ttt_cell[idx].x1 = (uint16_t)(rwx1 + c * cw + CELL_PAD);
            ttt_cell[idx].y1 = (uint16_t)(r * ch + CELL_PAD);
            ttt_cell[idx].x2 = (uint16_t)(rwx1 + (c + 1u) * cw - 1u - CELL_PAD);
            ttt_cell[idx].y2 = (uint16_t)((r + 1u) * ch - 1u - CELL_PAD);
        }
    }

    /* ── Right half bottom row — ON/DEFAULT button ─────────────────────── */
    btn_lift.x1 = (uint16_t)(rwx1 + CELL_PAD);
    btn_lift.y1 = (uint16_t)(2u * row_h + CELL_PAD);
    btn_lift.x2 = (uint16_t)(w - 1u - CELL_PAD);
    btn_lift.y2 = (uint16_t)(h - 1u - CELL_PAD);
    btn_lift.fill_color = COLOR_LIFT_OFF;
    btn_lift.text_color = WHITE;
    btn_lift.label      = "DEFAULT";
}

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
    sec_team.fill_color = WHITE;
    sec_team.text_color = BLACK;
    sec_team.label      = "Choose Color";

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
    uint16_t h = (uint16_t)(lcddev.height - SWIPE_MARGIN_H);

    /* Reserve top row for title */
    uint16_t title_h = (uint16_t)(FONT_SIZE + CELL_PAD * 2u);

    /* ── Motor grid  (2 columns × 3 rows) ───────────────────────────── */
    uint16_t motor_area_h = (uint16_t)(h - title_h);

    /*  Bottom 25 % → action buttons; top 75 % → motor grid */
    uint16_t action_h    = (uint16_t)(motor_area_h / 4u);
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

    motor_btn[6].x1 = CELL_PAD;
    motor_btn[6].y1 = action_y1;
    motor_btn[6].x2 = half - CELL_PAD;
    motor_btn[6].y2 = action_y2;
    motor_btn[6].fill_color = COLOR_MOTOR_OFF;
    motor_btn[6].text_color = WHITE;
    motor_btn[6].label      = motor_labels[6];

    btn_init_all.x1 = half + CELL_PAD;
    btn_init_all.y1 = action_y1;
    btn_init_all.x2 = (uint16_t)(w - 1u - CELL_PAD);
    btn_init_all.y2 = action_y2;
    btn_init_all.fill_color = COLOR_INIT_OFF;
    btn_init_all.text_color = WHITE;
    btn_init_all.label      = "Init All";
}

/* ══════════════════════════════════════════════════════════════════════════
 * Public API
 * ══════════════════════════════════════════════════════════════════════════ */

void display_ui_init(void)
{
    init_page0_layout();
    init_page3_layout();
    init_page1_layout();
    init_page2_layout();
    display_ui_reset_visual_state();
    active_page = DISPLAY_UI_PAGE_TX;
}

void display_ui_draw(void)
{
    if (active_page == DISPLAY_UI_PAGE_TREE)
        paint_tree_page();
    else if (active_page == DISPLAY_UI_PAGE_LIFT)
        paint_lift_page();
    else if (active_page == DISPLAY_UI_PAGE_MOTOR)
        paint_motor_page();
    else
        paint_tx_page();
}

/* ── Page navigation ────────────────────────────────────────────────────── */

void display_ui_set_page(uint8_t page)
{
    if (page > DISPLAY_UI_PAGE_LIFT)
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
    if (active_page == DISPLAY_UI_PAGE_TREE)
    {
        for (uint8_t i = 0u; i < TREE_CTRL_COUNT; i++)
        {
            if (x >= tree_ctrl_btn[i].x1 && x <= tree_ctrl_btn[i].x2 &&
                y >= tree_ctrl_btn[i].y1 && y <= tree_ctrl_btn[i].y2)
            {
                return (ui_touch_id_t)(UI_TOUCH_TREE_START + i);
            }
        }
    }
    else if (active_page == DISPLAY_UI_PAGE_LIFT)
    {
        if (x >= btn_lift.x1 && x <= btn_lift.x2 &&
            y >= btn_lift.y1 && y <= btn_lift.y2)
            return UI_TOUCH_LIFT_TOGGLE;

        if (x >= btn_start.x1 && x <= btn_start.x2 &&
            y >= btn_start.y1 && y <= btn_start.y2)
            return UI_TOUCH_START_TOGGLE;
        if (x >= btn_retry1.x1 && x <= btn_retry1.x2 &&
            y >= btn_retry1.y1 && y <= btn_retry1.y2)
            return UI_TOUCH_RETRY1_TOGGLE;
        if (x >= btn_retry2.x1 && x <= btn_retry2.x2 &&
            y >= btn_retry2.y1 && y <= btn_retry2.y2)
            return UI_TOUCH_RETRY2_TOGGLE;

        for (uint8_t i = 0u; i < TTT_TOUCH_CELLS; i++)
        {
            if (x >= ttt_cell[i].x1 && x <= ttt_cell[i].x2 &&
                y >= ttt_cell[i].y1 && y <= ttt_cell[i].y2)
            {
                uint8_t number = ttt_cell_number[i];
                switch (number)
                {
                    case 4u: return UI_TOUCH_TTT_4;
                    case 5u: return UI_TOUCH_TTT_5;
                    case 6u: return UI_TOUCH_TTT_6;
                    case 7u: return UI_TOUCH_TTT_7;
                    case 8u: return UI_TOUCH_TTT_8;
                    case 9u: return UI_TOUCH_TTT_9;
                    default: return UI_TOUCH_NONE; /* bottom row 1/2/3: not touchable */
                }
            }
        }
    }
    else if (active_page == DISPLAY_UI_PAGE_TX)
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
    else if (active_page == DISPLAY_UI_PAGE_MOTOR)
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
    if (team != DISPLAY_UI_TEAM_NONE &&
        team != DISPLAY_UI_TEAM_RED  &&
        team != DISPLAY_UI_TEAM_BLUE)
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
    team_selected   = DISPLAY_UI_TEAM_NONE;
    scroll_mode     = DISPLAY_UI_SCROLL_AR;
    cam_screen_state = 0u;
    uart_send_state  = 0u;

    /* Page 1 */
    for (uint8_t i = 0u; i < MOTOR_COUNT; i++)
        motor_state[i] = 0u;
    init_all_state   = 0u;
    tree_start_state = 0u;
    tree_stop_state = 0u;
    bringup_start_state = 0u;
    bringup_stop_state = 0u;

    /* Page 3 (Lift) */
    lift_state    = 0u;
    start_state   = 0u;
    retry1_state  = 0u;
    retry2_state  = 0u;
    ttt_mid_state = 0u;
    ttt_top_state = 0u;
}

/* ── Page 1/0 state setters ─────────────────────────────────────────────── */

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

void display_ui_set_tree_start_state(uint8_t active)
{
    tree_start_state = active ? 1u : 0u;
    if (active_page == DISPLAY_UI_PAGE_TREE)
        draw_tree_ctrl_btn(0u, tree_start_state);
}

void display_ui_set_tree_stop_state(uint8_t active)
{
    tree_stop_state = active ? 1u : 0u;
    if (active_page == DISPLAY_UI_PAGE_TREE)
        draw_tree_ctrl_btn(1u, tree_stop_state);
}

void display_ui_set_bringup_start_state(uint8_t active)
{
    bringup_start_state = active ? 1u : 0u;
    if (active_page == DISPLAY_UI_PAGE_TREE)
        draw_tree_ctrl_btn(2u, bringup_start_state);
}

void display_ui_set_bringup_stop_state(uint8_t active)
{
    bringup_stop_state = active ? 1u : 0u;
    if (active_page == DISPLAY_UI_PAGE_TREE)
        draw_tree_ctrl_btn(3u, bringup_stop_state);
}

/* ── Page 0 (Lift) state setters ─────────────────────────────────────────── */

void display_ui_set_lift_state(uint8_t active)
{
    lift_state = active ? 1u : 0u;

    /* Dropping disarms any mid/top selection — nothing should stay "armed"
     * once the rig is no longer lifted. */
    if (!lift_state)
    {
        ttt_mid_state = 0u;
        ttt_top_state = 0u;
    }

    if (active_page == DISPLAY_UI_PAGE_LIFT)
    {
        draw_lift_button();

        /* Only the mid/top cells need redrawing — and only when dropping,
         * since that's the only case where their visual state changes. */
        if (!lift_state)
        {
            draw_ttt_idx(0u); draw_ttt_idx(1u); draw_ttt_idx(2u);
            draw_ttt_idx(3u); draw_ttt_idx(4u); draw_ttt_idx(5u);
        }
    }
}

void display_ui_set_start_state(uint8_t active)
{
    start_state = active ? 1u : 0u;
    if (active_page == DISPLAY_UI_PAGE_LIFT)
        draw_start_button();
}

void display_ui_set_retry1_state(uint8_t active)
{
    retry1_state = active ? 1u : 0u;
    if (active_page == DISPLAY_UI_PAGE_LIFT)
        draw_retry1_button();
}

void display_ui_set_retry2_state(uint8_t active)
{
    retry2_state = active ? 1u : 0u;
    if (active_page == DISPLAY_UI_PAGE_LIFT)
        draw_retry2_button();
}

void display_ui_set_ttt_mid_state(uint8_t mode)
{
    if (mode > 3u)
        return;
    ttt_mid_state = mode;
    if (active_page == DISPLAY_UI_PAGE_LIFT)
    {
        /* Redraw only the middle-row cells (idx 3,4,5 in row-major order) */
        draw_ttt_idx(3u);
        draw_ttt_idx(4u);
        draw_ttt_idx(5u);
    }
}

void display_ui_set_ttt_top_state(uint8_t mode)
{
    if (mode > 3u)
        return;
    ttt_top_state = mode;
    if (active_page == DISPLAY_UI_PAGE_LIFT)
    {
        /* Redraw only the top-row cells (idx 0,1,2 in row-major order) */
        draw_ttt_idx(0u);
        draw_ttt_idx(1u);
        draw_ttt_idx(2u);
    }
}