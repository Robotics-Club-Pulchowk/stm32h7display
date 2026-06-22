#include "stdlib.h"
#include "lcd.h"
#include "lcdfont.h"
#include "ltdc.h"
#include "lcd_ex.c"

/* SPI LCD stubs - not used with LTDC RGB display */
static void SPI_DataWrite(uint16_t data) { (void)data; }
static void SPI_CmdWrite(uint16_t cmd) { (void)cmd; }
static void SPI_DataWrite_Pixel(uint32_t color) { (void)color; }
static void SPI_LCD_BL(uint8_t state) { (void)state; }
static void spi1_set_speed(uint8_t speed) { (void)speed; }
static void spi_lcd_init(void) {}
static void spi_dma_lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color) { (void)sx; (void)sy; (void)ex; (void)ey; (void)color; }
static void spi_dma_lcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color) { (void)sx; (void)sy; (void)ex; (void)ey; (void)color; }
static uint16_t lcd_read_id(uint8_t reg) { (void)reg; return 0; }
static uint16_t lcd_read_ram(uint8_t cmd) { (void)cmd; return 0; }
#define SPI_SPEED_4  0
#define SPI_SPEED_16 1
#define SPI_SPEED_32 2

/* LCD pen color and background color */
uint32_t g_point_color = RED;
uint32_t g_back_color  = 0XFFFFFFFF;

uint8_t g_spi_mode = 0;

/* LCD device parameters */
_lcd_dev lcddev;

void lcd_wr_data(volatile uint16_t data)
{
    data = data;
    SPI_DataWrite(data);
}

void lcd_wr_regno(volatile uint16_t regno)
{
    regno = regno;
    SPI_CmdWrite(regno);
}

void lcd_write_reg(uint16_t regno, uint16_t data)
{
    SPI_CmdWrite(regno);
    SPI_DataWrite(data);
}

void lcd_opt_delay(uint32_t i)
{
    while (i--);
}

void lcd_write_ram_prepare(void)
{
    if (g_spi_mode == 1)
    {
        SPI_CmdWrite(lcddev.wramcmd);
    }
}

void lcd_spi_speed_low(void)
{
    if (lcddev.id == 0x7796)
    {
        spi1_set_speed(SPI_SPEED_16);
    }
    else if (lcddev.id == 0X7789)
    {
        spi1_set_speed(SPI_SPEED_32);
    }
}

void lcd_spi_speed_high(void)
{
    if (lcddev.id == 0x7796)
    {
        spi1_set_speed(SPI_SPEED_4);
    }
    else if (lcddev.id == 0X7789)
    {
        spi1_set_speed(SPI_SPEED_4);
    }
}

uint32_t lcd_rgb565torgb888(uint16_t rgb565)
{
    uint16_t r, g, b;
    uint32_t rgb888;

    r = (rgb565 & 0XF800) >> 8;
    g = (rgb565 & 0X07E0) >> 3;
    b = (rgb565 & 0X001F) << 3;

    rgb888 = (r << 16) | (g << 8) | b;

    return rgb888;
}

uint32_t lcd_read_point(uint16_t x, uint16_t y)
{
    uint16_t color = 0;

    if (x >= lcddev.width || y >= lcddev.height)
    {
        return 0;
    }

    if (lcdltdc.pwidth != 0)
    {
        return ltdc_read_point(x, y);
    }

    lcd_set_cursor(x, y);

    if (g_spi_mode == 1)
    {
        lcd_opt_delay(2);
        color = lcd_read_ram(0X2E);
        return color;
    }

    return 0;
}

void lcd_display_on(void)
{
    if (lcdltdc.pwidth != 0)
    {
        ltdc_switch(1);
    }
    else if (lcddev.id == 0X5510)
    {
        lcd_wr_regno(0X2900);
    }
    else
    {
        lcd_wr_regno(0X29);
    }
}

void lcd_display_off(void)
{
    if (lcdltdc.pwidth != 0)
    {
        ltdc_switch(0);
    }
    else if (lcddev.id == 0X5510)
    {
        lcd_wr_regno(0X2800);
    }
    else
    {
        lcd_wr_regno(0X28);
    }
}

void lcd_set_cursor(uint16_t x, uint16_t y)
{
    if (lcdltdc.pwidth != 0)
    {
        return;
    }

    if (lcddev.id == 0X5510)
    {
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(x >> 8);
        lcd_wr_regno(lcddev.setxcmd + 1);
        lcd_wr_data(x & 0XFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(y >> 8);
        lcd_wr_regno(lcddev.setycmd + 1);
        lcd_wr_data(y & 0XFF);
    }
    else
    {
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(x >> 8);
        lcd_wr_data(x & 0XFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(y >> 8);
        lcd_wr_data(y & 0XFF);
    }
}

void lcd_scan_dir(uint8_t dir)
{
    uint16_t regval = 0;
    uint16_t dirreg = 0;
    uint16_t temp;

    if (lcdltdc.pwidth != 0)
    {
        return;
    }

    if (lcddev.dir == 1)
    {
        switch (dir)
        {
            case 0: dir = 6; break;
            case 1: dir = 7; break;
            case 2: dir = 4; break;
            case 3: dir = 5; break;
            case 4: dir = 1; break;
            case 5: dir = 0; break;
            case 6: dir = 3; break;
            case 7: dir = 2; break;
        }
    }

    switch (dir)
    {
        case L2R_U2D: regval |= (0 << 7) | (0 << 6) | (0 << 5); break;
        case L2R_D2U: regval |= (1 << 7) | (0 << 6) | (0 << 5); break;
        case R2L_U2D: regval |= (0 << 7) | (1 << 6) | (0 << 5); break;
        case R2L_D2U: regval |= (1 << 7) | (1 << 6) | (0 << 5); break;
        case U2D_L2R: regval |= (0 << 7) | (0 << 6) | (1 << 5); break;
        case U2D_R2L: regval |= (0 << 7) | (1 << 6) | (1 << 5); break;
        case D2U_L2R: regval |= (1 << 7) | (0 << 6) | (1 << 5); break;
        case D2U_R2L: regval |= (1 << 7) | (1 << 6) | (1 << 5); break;
    }

    dirreg = 0X36;

    if (lcddev.id == 0X5510)
    {
        dirreg = 0X3600;
    }

    if ((lcddev.id == 0X7789 && g_spi_mode == 0) || lcddev.id == 0X7796)
    {
        regval |= 0X08;
    }

    lcd_write_reg(dirreg, regval);

    if (regval & 0X20)
    {
        if (lcddev.width < lcddev.height)
        {
            temp = lcddev.width;
            lcddev.width = lcddev.height;
            lcddev.height = temp;
        }
    }
    else
    {
        if (lcddev.width > lcddev.height)
        {
            temp = lcddev.width;
            lcddev.width = lcddev.height;
            lcddev.height = temp;
        }
    }

    if (lcddev.id == 0X5510)
    {
        lcd_wr_regno(lcddev.setxcmd);     lcd_wr_data(0);
        lcd_wr_regno(lcddev.setxcmd + 1); lcd_wr_data(0);
        lcd_wr_regno(lcddev.setxcmd + 2); lcd_wr_data((lcddev.width - 1) >> 8);
        lcd_wr_regno(lcddev.setxcmd + 3); lcd_wr_data((lcddev.width - 1) & 0XFF);
        lcd_wr_regno(lcddev.setycmd);     lcd_wr_data(0);
        lcd_wr_regno(lcddev.setycmd + 1); lcd_wr_data(0);
        lcd_wr_regno(lcddev.setycmd + 2); lcd_wr_data((lcddev.height - 1) >> 8);
        lcd_wr_regno(lcddev.setycmd + 3); lcd_wr_data((lcddev.height - 1) & 0XFF);
    }
    else
    {
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(0); lcd_wr_data(0);
        lcd_wr_data((lcddev.width - 1) >> 8);
        lcd_wr_data((lcddev.width - 1) & 0XFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(0); lcd_wr_data(0);
        lcd_wr_data((lcddev.height - 1) >> 8);
        lcd_wr_data((lcddev.height - 1) & 0XFF);
    }
}

void lcd_draw_point(uint16_t x, uint16_t y, uint32_t color)
{
    if (lcdltdc.pwidth != 0)
    {
        ltdc_draw_point(x, y, color);
    }
    else
    {
        lcd_set_cursor(x, y);
        lcd_write_ram_prepare();
        SPI_DataWrite_Pixel(color);
    }
}

void lcd_display_dir(uint8_t dir)
{
    lcddev.dir = dir;

    if (lcdltdc.pwidth != 0)
    {
        ltdc_display_dir(dir);
        lcddev.width = lcdltdc.width;
        lcddev.height = lcdltdc.height;
        return;
    }

    if (dir == 0)
    {
        lcddev.width = 240;
        lcddev.height = 320;

        if (lcddev.id == 0x5510)
        {
            lcddev.wramcmd = 0X2C00;
            lcddev.setxcmd = 0X2A00;
            lcddev.setycmd = 0X2B00;
            lcddev.width = 480;
            lcddev.height = 800;
        }
        else
        {
            lcddev.wramcmd = 0X2C;
            lcddev.setxcmd = 0X2A;
            lcddev.setycmd = 0X2B;
        }

        if (lcddev.id == 0X5310 || lcddev.id == 0X7796)
        {
            lcddev.width = 320;
            lcddev.height = 480;
        }

        if (lcddev.id == 0X9806)
        {
            lcddev.width = 480;
            lcddev.height = 800;
        }
    }
    else
    {
        lcddev.width = 320;
        lcddev.height = 240;

        if (lcddev.id == 0x5510)
        {
            lcddev.wramcmd = 0X2C00;
            lcddev.setxcmd = 0X2A00;
            lcddev.setycmd = 0X2B00;
            lcddev.width = 800;
            lcddev.height = 480;
        }
        else
        {
            lcddev.wramcmd = 0X2C;
            lcddev.setxcmd = 0X2A;
            lcddev.setycmd = 0X2B;
        }

        if (lcddev.id == 0X5310 || lcddev.id == 0X7796)
        {
            lcddev.width = 480;
            lcddev.height = 320;
        }

        if (lcddev.id == 0X9806)
        {
            lcddev.width = 800;
            lcddev.height = 480;
        }
    }

    lcd_scan_dir(DFT_SCAN_DIR);
}

void lcd_set_window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height)
{
    uint16_t twidth, theight;
    twidth = sx + width - 1;
    theight = sy + height - 1;

    if (lcdltdc.pwidth != 0)
    {
        return;
    }

    if (lcddev.id == 0X5510)
    {
        lcd_wr_regno(lcddev.setxcmd);     lcd_wr_data(sx >> 8);
        lcd_wr_regno(lcddev.setxcmd + 1); lcd_wr_data(sx & 0XFF);
        lcd_wr_regno(lcddev.setxcmd + 2); lcd_wr_data(twidth >> 8);
        lcd_wr_regno(lcddev.setxcmd + 3); lcd_wr_data(twidth & 0XFF);
        lcd_wr_regno(lcddev.setycmd);     lcd_wr_data(sy >> 8);
        lcd_wr_regno(lcddev.setycmd + 1); lcd_wr_data(sy & 0XFF);
        lcd_wr_regno(lcddev.setycmd + 2); lcd_wr_data(theight >> 8);
        lcd_wr_regno(lcddev.setycmd + 3); lcd_wr_data(theight & 0XFF);
    }
    else
    {
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(sx >> 8); lcd_wr_data(sx & 0XFF);
        lcd_wr_data(twidth >> 8); lcd_wr_data(twidth & 0XFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(sy >> 8); lcd_wr_data(sy & 0XFF);
        lcd_wr_data(theight >> 8); lcd_wr_data(theight & 0XFF);
    }
}

void lcd_init(void)
{
    lcddev.id = ltdc_panelid_read();

#if RGB_80_8001280
    lcddev.id = 0X8081;
#endif

    if (lcddev.id != 0)
    {
        ltdc_init();
    }
    else
    {
        g_spi_mode = 1;
        spi_lcd_init();
        lcd_spi_speed_low();
        lcddev.id = lcd_read_id(0XD3);

        if (lcddev.id != 0X7796)
        {
            lcddev.id = lcd_read_id(0X04);

            if (lcddev.id == 0X8552)
            {
                lcddev.id = 0x7789;
            }
        }

        lcdltdc.pixformat = 0X02;
    }

    if (lcddev.id == 0X7789)
    {
        lcd_ex_st7789_reginit();
    }
    else if (lcddev.id == 0x7796)
    {
        lcd_ex_st7796_reginit();
    }

    lcd_spi_speed_high();

    if (lcdltdc.pwidth != 0 && lcdltdc.pwidth != 480)
    {
        lcd_display_dir(1);
    }
    else
    {
        lcd_display_dir(0);
    }

    if (g_spi_mode == 0)
    {
        LCD_BL(1);
    }
    else
    {
        SPI_LCD_BL(1);
    }

    lcd_clear(WHITE);
}

void lcd_clear(uint32_t color)
{
    if (lcdltdc.pwidth != 0)
    {
        ltdc_clear(color);
    }
    else
    {
        spi_dma_lcd_fill(0, 0, lcddev.width - 1, lcddev.height - 1, color);
    }
}

void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color)
{
    uint16_t i, j;
    uint16_t xlen = 0;
    uint32_t lcdsize = (ex - sx + 1) * (ey - sy + 1);

    if (lcdltdc.pwidth != 0)
    {
        ltdc_fill(sx, sy, ex, ey, color);
    }
    else
    {
        if (lcdsize > lcddev.width)
        {
            spi_dma_lcd_fill(sx, sy, ex, ey, color);
        }
        else
        {
            xlen = ex - sx + 1;

            for (i = sy; i <= ey; i++)
            {
                lcd_set_cursor(sx, i);
                lcd_write_ram_prepare();

                for (j = 0; j < xlen; j++)
                {
                    SPI_DataWrite_Pixel(color);
                }
            }
        }
    }
}

void lcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    uint16_t height, width;
    uint16_t i, j;
    uint32_t lcdsize = (ex - sx + 1) * (ey - sy + 1);

    if (lcdltdc.pwidth != 0)
    {
        ltdc_color_fill(sx, sy, ex, ey, color);
    }
    else
    {
        if (lcdsize > lcddev.width)
        {
            spi_dma_lcd_color_fill(sx, sy, ex, ey, color);
        }
        else
        {
            width = ex - sx + 1;
            height = ey - sy + 1;

            for (i = 0; i < height; i++)
            {
                lcd_set_cursor(sx, sy + i);
                lcd_write_ram_prepare();

                for (j = 0; j < width; j++)
                {
                    SPI_DataWrite_Pixel(color[i * width + j]);
                }
            }
        }
    }
}

void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, row, col;
    delta_x = x2 - x1;
    delta_y = y2 - y1;
    row = x1;
    col = y1;

    if (delta_x > 0) incx = 1;
    else if (delta_x == 0) incx = 0;
    else { incx = -1; delta_x = -delta_x; }

    if (delta_y > 0) incy = 1;
    else if (delta_y == 0) incy = 0;
    else { incy = -1; delta_y = -delta_y; }

    if (delta_x > delta_y) distance = delta_x;
    else distance = delta_y;

    for (t = 0; t <= distance + 1; t++)
    {
        lcd_draw_point(row, col, color);
        xerr += delta_x;
        yerr += delta_y;

        if (xerr > distance) { xerr -= distance; row += incx; }
        if (yerr > distance) { yerr -= distance; col += incy; }
    }
}

void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint32_t color)
{
    if ((len == 0) || (x > lcddev.width) || (y > lcddev.height)) return;
    lcd_fill(x, y, x + len - 1, y, color);
}

void lcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint32_t color)
{
    lcd_draw_line(x1, y1, x2, y1, color);
    lcd_draw_line(x1, y1, x1, y2, color);
    lcd_draw_line(x1, y2, x2, y2, color);
    lcd_draw_line(x2, y1, x2, y2, color);
}

void lcd_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint32_t color)
{
    int a, b;
    int di;
    a = 0;
    b = r;
    di = 3 - (r << 1);

    while (a <= b)
    {
        lcd_draw_point(x0 + a, y0 - b, color);
        lcd_draw_point(x0 + b, y0 - a, color);
        lcd_draw_point(x0 + b, y0 + a, color);
        lcd_draw_point(x0 + a, y0 + b, color);
        lcd_draw_point(x0 - a, y0 + b, color);
        lcd_draw_point(x0 - b, y0 + a, color);
        lcd_draw_point(x0 - a, y0 - b, color);
        lcd_draw_point(x0 - b, y0 - a, color);
        a++;

        if (di < 0) { di += 4 * a + 6; }
        else { di += 10 + 4 * (a - b); b--; }
    }
}

void lcd_fill_circle(uint16_t x, uint16_t y, uint16_t r, uint32_t color)
{
    uint32_t i;
    uint32_t imax = ((uint32_t)r * 707) / 1000 + 1;
    uint32_t sqmax = (uint32_t)r * (uint32_t)r + (uint32_t)r / 2;
    uint32_t xr = r;

    lcd_draw_hline(x - r, y, 2 * r, color);

    for (i = 1; i <= imax; i++)
    {
        if ((i * i + xr * xr) > sqmax)
        {
            if (xr > imax)
            {
                lcd_draw_hline(x - i + 1, y + xr, 2 * (i - 1), color);
                lcd_draw_hline(x - i + 1, y - xr, 2 * (i - 1), color);
            }
            xr--;
        }

        lcd_draw_hline(x - xr, y + i, 2 * xr, color);
        lcd_draw_hline(x - xr, y - i, 2 * xr, color);
    }
}

void lcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint32_t color)
{
    uint8_t temp, t1, t;
    uint16_t y0 = y;
    uint8_t csize = 0;
    uint8_t *pfont = 0;

    csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);
    chr = chr - ' ';

    switch (size)
    {
        case 12: pfont = (uint8_t *)asc2_1206[chr]; break;
        case 16: pfont = (uint8_t *)asc2_1608[chr]; break;
        case 24: pfont = (uint8_t *)asc2_2412[chr]; break;
        case 32: pfont = (uint8_t *)asc2_3216[chr]; break;
        default: return;
    }

    for (t = 0; t < csize; t++)
    {
        temp = pfont[t];

        for (t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80)
            {
                lcd_draw_point(x, y, color);
            }
            else if (mode == 0)
            {
                lcd_draw_point(x, y, g_back_color);
            }

            temp <<= 1;
            y++;

            if (y >= lcddev.height) return;

            if ((y - y0) == size)
            {
                y = y0;
                x++;
                if (x >= lcddev.width) return;
                break;
            }
        }
    }
}

static uint32_t lcd_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while (n--) result *= m;
    return result;
}

void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint32_t color)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)
    {
        temp = (num / lcd_pow(10, len - t - 1)) % 10;

        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                lcd_show_char(x + (size / 2) * t, y, ' ', size, 0, color);
                continue;
            }
            else
            {
                enshow = 1;
            }
        }

        lcd_show_char(x + (size / 2) * t, y, temp + '0', size, 0, color);
    }
}

void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint32_t color)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)
    {
        temp = (num / lcd_pow(10, len - t - 1)) % 10;

        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                if (mode & 0X80)
                {
                    lcd_show_char(x + (size / 2) * t, y, '0', size, mode & 0X01, color);
                }
                else
                {
                    lcd_show_char(x + (size / 2) * t, y, ' ', size, mode & 0X01, color);
                }
                continue;
            }
            else
            {
                enshow = 1;
            }
        }

        lcd_show_char(x + (size / 2) * t, y, temp + '0', size, mode & 0X01, color);
    }
}

void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint32_t color)
{
    uint8_t x0 = x;

    width += x;
    height += y;

    while ((*p <= '~') && (*p >= ' '))
    {
        if (x >= width)
        {
            x = x0;
            y += size;
        }

        if (y >= height) break;

        lcd_show_char(x, y, *p, size, 0, color);
        x += size / 2;
        p++;
    }
}