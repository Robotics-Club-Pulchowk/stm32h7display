#include "display_ui.h"
#include "lcd.h"
#include <string.h>

/* One cell in the Section-A grid */
typedef struct
{
    uint16_t x1, y1, x2, y2;
    char     label[4];   /* up to 3 chars + NUL */
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

/* Section-A grid dimensions */
#define GRID_COLS   3
#define GRID_ROWS   4
#define GRID_CELLS  (GRID_COLS * GRID_ROWS)   /* 12 */

/* Section-B / C button counts */
#define SEC_B_CNT   2
#define SEC_C_CNT   3

static grid_cell_t grid[GRID_CELLS];
static button_t    sec_b[SEC_B_CNT];
static button_t    sec_c[SEC_C_CNT];

static void draw_cell(const grid_cell_t *c)
{
    uint16_t bw     = (uint16_t)(c->x2 - c->x1 + 1);
    uint16_t bh     = (uint16_t)(c->y2 - c->y1 + 1);
    uint16_t llen   = (uint16_t)strlen(c->label);
    uint16_t text_w = (uint16_t)(llen * (FONT_SIZE / 2));
    uint16_t text_h = FONT_SIZE;

    if (text_w > bw) text_w = bw;
    if (text_h > bh) text_h = bh;

    uint16_t tx = (uint16_t)(c->x1 + (bw - text_w) / 2);
    uint16_t ty = (uint16_t)(c->y1 + (bh - text_h) / 2);

    lcd_fill(c->x1, c->y1, c->x2, c->y2, GREEN);
    g_back_color = GREEN;
    lcd_show_string(tx, ty, text_w, text_h, FONT_SIZE, (char *)c->label, WHITE);
}

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

            grid[idx].label[0] = (char)('A' + idx);
            grid[idx].label[1] = '\0';
        }
    }

    /* ── Section B  (top-left quarter) ───────────────────────────────── */
    uint16_t bw     = midx;
    uint16_t bh     = midy;
    uint16_t half_w = (uint16_t)(bw / 2);
    uint16_t sq     = (uint16_t)((half_w < bh ? half_w : bh) - 2 * CELL_PAD);
    uint16_t bvy    = (uint16_t)((bh - sq) / 2);

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
    uint16_t cy1 = (uint16_t)(midy + BORDER_W);
    uint16_t cw2 = midx;
    uint16_t ch2 = (uint16_t)(h - cy1);
    uint16_t gap = (uint16_t)(2 * CELL_PAD);
    uint16_t slot_h = (uint16_t)(ch2 / SEC_C_CNT);
    uint16_t btn_h  = (uint16_t)(slot_h - gap);

    static const uint32_t c_fill[SEC_C_CNT] = { YELLOW,  MAGENTA, CYAN  };
    static const uint32_t c_text[SEC_C_CNT] = { BLACK,   WHITE,   BLACK };
    static const char    *c_lbl [SEC_C_CNT] = { "Start", "Retry1", "Retry2" };

    for (uint8_t i = 0; i < SEC_C_CNT; i++)
    {
        sec_c[i].x1         = (uint16_t)(CELL_PAD);
        sec_c[i].y1         = (uint16_t)(cy1 + i * slot_h + CELL_PAD);
        sec_c[i].x2         = (uint16_t)(cw2 - 1 - CELL_PAD);
        sec_c[i].y2         = (uint16_t)(sec_c[i].y1 + btn_h - 1);
        sec_c[i].fill_color = c_fill[i];
        sec_c[i].text_color = c_text[i];
        sec_c[i].label      = c_lbl[i];
    }
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

    for (uint8_t i = 0; i < GRID_CELLS; i++)
    {
        draw_cell(&grid[i]);
    }

    for (uint8_t i = 0; i < SEC_B_CNT; i++)
    {
        draw_button(&sec_b[i]);
    }

    for (uint8_t i = 0; i < SEC_C_CNT; i++)
    {
        draw_button(&sec_c[i]);
    }
}

const char *display_ui_get_touch_msg(uint16_t x, uint16_t y)
{
    uint8_t i;

    for (i = 0; i < GRID_CELLS; i++)
    {
        if (x >= grid[i].x1 && x <= grid[i].x2 &&
            y >= grid[i].y1 && y <= grid[i].y2)
        {
            return grid[i].label;
        }
    }

    for (i = 0; i < SEC_B_CNT; i++)
    {
        if (x >= sec_b[i].x1 && x <= sec_b[i].x2 &&
            y >= sec_b[i].y1 && y <= sec_b[i].y2)
        {
            return sec_b[i].label;
        }
    }

    for (i = 0; i < SEC_C_CNT; i++)
    {
        if (x >= sec_c[i].x1 && x <= sec_c[i].x2 &&
            y >= sec_c[i].y1 && y <= sec_c[i].y2)
        {
            return sec_c[i].label;
        }
    }

    return NULL;
}
