/**
 ****************************************************************************************************
 * @file        touch.c
 * @version     V1.0
 * @brief       ������ ��������
 * @note        ֧�ֵ���/����ʽ������
 *
 ****************************************************************************************************
 * @attention   Waiken-Smart ������Զ
 *
 * ʵ��ƽ̨:    STM32H743IIT6Сϵͳ��
 *
 ****************************************************************************************************
 */

#include "stdio.h"
#include "stdlib.h"
#include "lcd.h"
#include "touch.h"
#include "24cxx_stub.h"

#include "delay.h"


_m_tp_dev tp_dev =
{
    tp_init,
    tp_scan,
    tp_adjust,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

/**
 * @brief       SPIд����
 * @note        ������ICд��1 byte����
 * @param       data: Ҫд�������
 * @retval      ��
 */
static void tp_write_byte(uint8_t data)
{
    uint8_t count = 0;

    for (count = 0; count < 8; count++)
    {
        if (data & 0x80)    /* ����1 */
        {
            T_MOSI(1);
        }
        else                /* ����0 */
        {
            T_MOSI(0);
        }

        data <<= 1;
        T_CLK(0);
        delay_us(1);
        T_CLK(1);           /* ��������Ч */
        delay_us(1);
    }
}

/**
 * @brief       SPI������
 * @note        �Ӵ�����IC��ȡadcֵ
 * @param       cmd: ָ��
 * @retval      ��ȡ��������,ADCֵ(12bit)
 */
static uint16_t tp_read_ad(uint8_t cmd)
{
    uint8_t count = 0;
    uint16_t num = 0;
  
    T_CLK(0);           /* ������ʱ�� */
    T_MOSI(0);          /* ���������� */
    T_CS(0);            /* ѡ�д�����IC */
    tp_write_byte(cmd); /* ���������� */
    delay_us(6);        /* ADS7846��ת��ʱ���Ϊ6us */
    T_CLK(0);
    delay_us(1);
    T_CLK(1);           /* ��1��ʱ�ӣ����BUSY */
    delay_us(1);
    T_CLK(0);

    for (count = 0; count < 16; count++)    /* ����16λ����,ֻ�и�12λ��Ч */
    {
        num <<= 1;
        T_CLK(0);       /* �½�����Ч */
        delay_us(1);
        T_CLK(1);

        if (T_MISO)num++;
    }

    num >>= 4;          /* ֻ�и�12λ��Ч. */
    T_CS(1);            /* �ͷ�Ƭѡ */
    return num;
}

/* ���败������оƬ ���ݲɼ� �˲��ò��� */
#define TP_READ_TIMES   5       /* ��ȡ���� */
#define TP_LOST_VAL     1       /* ����ֵ */

/**
 * @brief       ��ȡһ������ֵ(x����y)
 * @note        ������ȡTP_READ_TIMES������,����Щ������������,
 *              Ȼ��ȥ����ͺ����TP_LOST_VAL����, ȡƽ��ֵ
 *              ����ʱ������: TP_READ_TIMES > 2*TP_LOST_VAL ������
 *
 * @param       cmd : ָ��
 *   @arg       0XD0: ��ȡX������(@����״̬,����״̬��Y�Ե�.)
 *   @arg       0X90: ��ȡY������(@����״̬,����״̬��X�Ե�.)
 *
 * @retval      ��ȡ��������(�˲����), ADCֵ(12bit)
 */
static uint16_t tp_read_xoy(uint8_t cmd)
{
    uint16_t i, j;
    uint16_t buf[TP_READ_TIMES];
    uint16_t sum = 0;
    uint16_t temp;

    for (i = 0; i < TP_READ_TIMES; i++)      /* �ȶ�ȡTP_READ_TIMES������ */
    {
        buf[i] = tp_read_ad(cmd);
    }

    for (i = 0; i < TP_READ_TIMES - 1; i++)  /* �����ݽ������� */
    {
        for (j = i + 1; j < TP_READ_TIMES; j++)
        {
            if (buf[i] > buf[j])   /* �������� */
            {
                temp = buf[i];
                buf[i] = buf[j];
                buf[j] = temp;
            }
        }
    }

    sum = 0;

    for (i = TP_LOST_VAL; i < TP_READ_TIMES - TP_LOST_VAL; i++)   /* ȥ�����˵Ķ���ֵ */
    {
        sum += buf[i];  /* �ۼ�ȥ������ֵ�Ժ������. */
    }

    temp = sum / (TP_READ_TIMES - 2 * TP_LOST_VAL); /* ȡƽ��ֵ */
    return temp;
}

/**
 * @brief       ��ȡx, y����
 * @param       x,y: ��ȡ��������ֵ
 * @retval      ��
 */
static void tp_read_xy(uint16_t *x, uint16_t *y)
{
    uint16_t xval, yval;

    if (tp_dev.touchtype & 0X01)    /* X,Y��������Ļ�෴ */
    {
        xval = tp_read_xoy(0X90);   /* ��ȡX������ADֵ, �����з���任 */
        yval = tp_read_xoy(0XD0);   /* ��ȡY������ADֵ */
    }
    else                            /* X,Y��������Ļ��ͬ */
    {
        xval = tp_read_xoy(0XD0);   /* ��ȡX������ADֵ */
        yval = tp_read_xoy(0X90);   /* ��ȡY������ADֵ */
    }

    *x = xval;
    *y = yval;
}

/* �������ζ�ȡX,Y�������������������ֵ */
#define TP_ERR_RANGE    50      /* ��Χ */

/**
 * @brief       ������ȡ2�δ���IC����, ���˲�
 * @note        ����2�ζ�ȡ������IC,�������ε�ƫ��ܳ���ERR_RANGE,����
 *              ����,����Ϊ������ȷ,�����������.�ú����ܴ�����׼ȷ��.
 *
 * @param       x,y: ��ȡ��������ֵ
 * @retval      0, ʧ��; 1, �ɹ�;
 */
static uint8_t tp_read_xy2(uint16_t *x, uint16_t *y)
{
    uint16_t x1, y1;
    uint16_t x2, y2;

    tp_read_xy(&x1, &y1);   /* ��ȡ��һ������ */
    tp_read_xy(&x2, &y2);   /* ��ȡ�ڶ������� */

    /* ǰ�����β�����+-TP_ERR_RANGE�� */
    if (((x2 <= x1 && x1 < x2 + TP_ERR_RANGE) || (x1 <= x2 && x2 < x1 + TP_ERR_RANGE)) &&
            ((y2 <= y1 && y1 < y2 + TP_ERR_RANGE) || (y1 <= y2 && y2 < y1 + TP_ERR_RANGE)))
    {
        *x = (x1 + x2) / 2;
        *y = (y1 + y2) / 2;
        return 1;
    }

    return 0;
}

/******************************************************************************************/
/* ��LCD�����йصĺ���, ����У׼�õ� */

/**
 * @brief       ��һ��У׼�õĴ�����(ʮ�ּ�)
 * @param       x,y   : ����
 * @param       color : ��ɫ
 * @retval      ��
 */
static void tp_draw_touch_point(uint16_t x, uint16_t y, uint32_t color)
{
    lcd_draw_line(x - 12, y, x + 13, y, color); /* ���� */
    lcd_draw_line(x, y - 12, x, y + 13, color); /* ���� */
    lcd_draw_point(x + 1, y + 1, color);
    lcd_draw_point(x - 1, y + 1, color);
    lcd_draw_point(x + 1, y - 1, color);
    lcd_draw_point(x - 1, y - 1, color);
    lcd_draw_circle(x, y, 6, color);            /* ������Ȧ */
}

/**
 * @brief       ��һ�����(2*2�ĵ�)
 * @param       x,y   : ����
 * @param       color : ��ɫ
 * @retval      ��
 */
void tp_draw_big_point(uint16_t x, uint16_t y, uint32_t color)
{
    lcd_draw_point(x, y, color);       /* ���ĵ� */
    lcd_draw_point(x + 1, y, color);
    lcd_draw_point(x, y + 1, color);
    lcd_draw_point(x + 1, y + 1, color);
}

/******************************************************************************************/

/**
 * @brief       ��������ɨ��
 * @param       mode: ����ģʽ
 *   @arg       0, ��Ļ����;
 *   @arg       1, ��������(У׼�����ⳡ����)
 *
 * @retval      0, �����޴���; 1, �����д���;
 */
uint8_t tp_scan(uint8_t mode)
{
    if (T_PEN == 0)     /* �а������� */
    {
        if (mode)       /* ��ȡ��������, ����ת�� */
        {
            tp_read_xy2(&tp_dev.x[0], &tp_dev.y[0]);
        }
        else if (tp_read_xy2(&tp_dev.x[0], &tp_dev.y[0]))     /* ��ȡ��Ļ����, ��Ҫת�� */
        {
            /* ��X�� ��������ת�����߼�����(����ӦLCD��Ļ�����X����ֵ) */
            tp_dev.x[0] = (signed short)(tp_dev.x[0] - tp_dev.xc) / tp_dev.xfac + lcddev.width / 2;

            /* ��Y�� ��������ת�����߼�����(����ӦLCD��Ļ�����Y����ֵ) */
            tp_dev.y[0] = (signed short)(tp_dev.y[0] - tp_dev.yc) / tp_dev.yfac + lcddev.height / 2;
        }

        if ((tp_dev.sta & TP_PRES_DOWN) == 0)   /* ֮ǰû�б����� */
        {
            tp_dev.sta = TP_PRES_DOWN | TP_CATH_PRES;   /* �������� */
            tp_dev.x[CT_MAX_TOUCH - 1] = tp_dev.x[0];   /* ��¼��һ�ΰ���ʱ������ */
            tp_dev.y[CT_MAX_TOUCH - 1] = tp_dev.y[0];
        }
    }
    else
    {
        if (tp_dev.sta & TP_PRES_DOWN)      /* ֮ǰ�Ǳ����µ� */
        {
            tp_dev.sta &= ~TP_PRES_DOWN;    /* ��ǰ����ɿ� */
        }
        else     /* ֮ǰ��û�б����� */
        {
            tp_dev.x[CT_MAX_TOUCH - 1] = 0;
            tp_dev.y[CT_MAX_TOUCH - 1] = 0;
            tp_dev.x[0] = 0xffff;
            tp_dev.y[0] = 0xffff;
        }
    }

    return tp_dev.sta & TP_PRES_DOWN; /* ���ص�ǰ�Ĵ���״̬ */
}

/* TP_SAVE_ADDR_BASE���崥����У׼����������EEPROM�����λ��(��ʼ��ַ)
 * ռ�ÿռ� : 13�ֽ�.
 */
#define TP_SAVE_ADDR_BASE   40

/**
 * @brief       ����У׼����
 * @note        ����������EEPROMоƬ����(24C02),��ʼ��ַΪTP_SAVE_ADDR_BASE.
 *              ռ�ô�СΪ13�ֽ�
 * @param       ��
 * @retval      ��
 */
void tp_save_adjust_data(void)
{
    uint8_t *p = (uint8_t *)&tp_dev.xfac;   /* ָ���׵�ַ */
  
    /* pָ��tp_dev.xfac�ĵ�ַ, p+4����tp_dev.yfac�ĵ�ַ
     * p+8����tp_dev.xoff�ĵ�ַ,p+10,����tp_dev.yoff�ĵ�ַ
     * �ܹ�ռ��12���ֽ�(4������)
     * p+12���ڴ�ű�ǵ��败�����Ƿ�У׼������(0X0A)
     * ��p[12]д��0X0A. ����Ѿ�У׼��.
     */
    at24cxx_write(TP_SAVE_ADDR_BASE, p, 12);                /* ����12���ֽ�����(xfac,yfac,xc,yc) */
    at24cxx_write_one_byte(TP_SAVE_ADDR_BASE + 12, 0X0A);   /* ����У׼ֵ */
}

/**
 * @brief       ��ȡ������EEPROM�����У׼ֵ
 * @param       ��
 * @retval      0����ȡʧ�ܣ�Ҫ����У׼
 *              1���ɹ���ȡ����
 */
uint8_t tp_get_adjust_data(void)
{
    uint8_t *p = (uint8_t *)&tp_dev.xfac;
    uint8_t temp = 0;

    /* ����������ֱ��ָ��tp_dev.xfac��ַ���б����, ��ȡ��ʱ��,����ȡ����������
     * д��ָ��tp_dev.xfac���׵�ַ, �Ϳ��Ի�ԭд���ȥ��ֵ, ������Ҫ����������
     * ������. �˷��������ڸ�������(�����ṹ��)�ı���/��ȡ(�����ṹ��).
     */
    at24cxx_read(TP_SAVE_ADDR_BASE, p, 12);                 /* ��ȡ12�ֽ����� */
    temp = at24cxx_read_one_byte(TP_SAVE_ADDR_BASE + 12);   /* ��ȡУ׼״̬��� */

    if (temp == 0X0A)   /* �������Ѿ�У׼���� */
    {
        return 1;
    }

    return 0;
}

/* ��ʾ�ַ��� */
char *const TP_REMIND_MSG_TBL = "Please use the stylus click the cross on the screen.The cross will always move until the screen adjustment is completed.";

/**
 * @brief       ��ʾУ׼���(��������)
 * @param       xy[5][2]: 5����������ֵ
 * @param       px,py   : x,y����ı�������(Լ�ӽ�1Խ��)
 * @retval      ��
 */
static void tp_adjust_info_show(uint16_t xy[5][2], double px, double py)
{
    uint8_t i;
    char sbuf[20];

    for (i = 0; i < 5; i++)   /* ��ʾ5����������ֵ */
    {
        sprintf(sbuf, "x%d:%d", i + 1, xy[i][0]);
        lcd_show_string(40, 160 + (i * 20), lcddev.width, lcddev.height, 16, sbuf, RED);
        sprintf(sbuf, "y%d:%d", i + 1, xy[i][1]);
        lcd_show_string(40 + 80, 160 + (i * 20), lcddev.width, lcddev.height, 16, sbuf, RED);
    }

    /* ��ʾX/Y����ı������� */
    lcd_fill(40, 160 + (i * 20), lcddev.width - 1, 16, WHITE);  /* ���֮ǰ��px,py��ʾ */
    sprintf(sbuf, "px:%0.2f", px);
    sbuf[7] = 0; /* ���ӽ����� */
    lcd_show_string(40, 160 + (i * 20), lcddev.width, lcddev.height, 16, sbuf, RED);
    sprintf(sbuf, "py:%0.2f", py);
    sbuf[7] = 0; /* ���ӽ����� */
    lcd_show_string(40 + 80, 160 + (i * 20), lcddev.width, lcddev.height, 16, sbuf, RED);
}

/**
 * @brief       ������У׼����
 * @note        ʹ�����У׼��
 *              �������õ�x��/y���������xfac/yfac��������������ֵ(xc,yc)��4������
 *              ���ǹ涨: �������꼴AD�ɼ���������ֵ,��Χ��0~4095.
 *                        �߼����꼴LCD��Ļ������, ��ΧΪLCD��Ļ�ķֱ���.
 *
 * @param       ��
 * @retval      ��
 */
void tp_adjust(void)
{
    uint16_t pxy[5][2];     /* �������껺��ֵ */
    uint8_t  cnt = 0;
    short s1, s2, s3, s4;   /* 4����������ֵ */
    double px, py;          /* X,Y�������������,�����ж��Ƿ�У׼�ɹ� */
    uint16_t outtime = 0;
    cnt = 0;

    lcd_clear(WHITE);       /* ���� */
    lcd_show_string(40, 40, 160, 100, 16, TP_REMIND_MSG_TBL, RED); /* ��ʾ��ʾ��Ϣ */
    tp_draw_touch_point(20, 20, RED);   /* ����1 */
    tp_dev.sta = 0;         /* ���������ź� */

    while (1)               /* �������10����û�а���,���Զ��˳� */
    {
        tp_dev.scan(1);     /* ɨ���������� */

        if ((tp_dev.sta & 0xc000) == TP_CATH_PRES)   /* ����������һ��(��ʱ�����ɿ���.) */
        {
            outtime = 0;
            tp_dev.sta &= ~TP_CATH_PRES;    /* ��ǰ����Ѿ�����������. */

            pxy[cnt][0] = tp_dev.x[0];      /* ����X�������� */
            pxy[cnt][1] = tp_dev.y[0];      /* ����Y�������� */
            cnt++;

            switch (cnt)
            {
                case 1:
                    tp_draw_touch_point(20, 20, WHITE);                 /* �����1 */
                    tp_draw_touch_point(lcddev.width - 20, 20, RED);    /* ����2 */
                    break;

                case 2:
                    tp_draw_touch_point(lcddev.width - 20, 20, WHITE);  /* �����2 */
                    tp_draw_touch_point(20, lcddev.height - 20, RED);   /* ����3 */
                    break;

                case 3:
                    tp_draw_touch_point(20, lcddev.height - 20, WHITE); /* �����3 */
                    tp_draw_touch_point(lcddev.width - 20, lcddev.height - 20, RED);  /* ����4 */
                    break;

                case 4:
                    lcd_clear(WHITE);   /* �����������, ֱ������ */
                    tp_draw_touch_point(lcddev.width / 2, lcddev.height / 2, RED);    /* ����5 */
                    break;

                case 5:     /* ȫ��5�����Ѿ��õ� */
                    s1 = pxy[1][0] - pxy[0][0]; /* ��2����͵�1�����X�����������ֵ(ADֵ) */
                    s3 = pxy[3][0] - pxy[2][0]; /* ��4����͵�3�����X�����������ֵ(ADֵ) */
                    s2 = pxy[3][1] - pxy[1][1]; /* ��4����͵�2�����Y�����������ֵ(ADֵ) */
                    s4 = pxy[2][1] - pxy[0][1]; /* ��3����͵�1�����Y�����������ֵ(ADֵ) */

                    px = (double)s1 / s3;       /* X��������� */
                    py = (double)s2 / s4;       /* Y��������� */

                    if (px < 0)px = -px;        /* ���������� */
                    if (py < 0)py = -py;        /* ���������� */

                    if (px < 0.95 || px > 1.05 || py < 0.95 || py > 1.05 ||     /* �������ϸ� */
                            abs(s1) > 4095 || abs(s2) > 4095 || abs(s3) > 4095 || abs(s4) > 4095 || /* ��ֵ���ϸ�, �������귶Χ */
                            abs(s1) == 0 || abs(s2) == 0 || abs(s3) == 0 || abs(s4) == 0            /* ��ֵ���ϸ�, ����0 */
                       )
                    {
                        cnt = 0;
                        tp_draw_touch_point(lcddev.width / 2, lcddev.height / 2, WHITE); /* �����5 */
                        tp_draw_touch_point(20, 20, RED);   /* ���»���1 */
                        tp_adjust_info_show(pxy, px, py);   /* ��ʾ��ǰ��Ϣ,���������� */
                        continue;
                    }

                    tp_dev.xfac = (float)(s1 + s3) / (2 * (lcddev.width - 40));
                    tp_dev.yfac = (float)(s2 + s4) / (2 * (lcddev.height - 40));

                    tp_dev.xc = pxy[4][0];      /* X��,������������ */
                    tp_dev.yc = pxy[4][1];      /* Y��,������������ */

                    lcd_clear(WHITE);   /* ���� */
                    lcd_show_string(35, 110, lcddev.width, lcddev.height, 16, "Touch Screen Adjust OK!", BLUE); /* У����� */
                    delay_ms(1000);
                    tp_save_adjust_data();

                    lcd_clear(WHITE);   /* ���� */
                    return;             /* У����� */
            }
        }

        delay_ms(10);
        outtime++;

        if (outtime > 1000)
        {
            tp_get_adjust_data();
            break;
        }
    }
}

/**
 * @brief       ��������ʼ��
 * @param       ��
 * @retval      0,û�н���У׼
 *              1,���й�У׼
 */
uint8_t tp_init(void)
{
    tp_dev.touchtype = 0;                   /* Ĭ������(������ & ����) */
    tp_dev.touchtype |= lcddev.dir & 0X01;  /* ����LCD�ж��Ǻ����������� */

    if (lcddev.id == 0x7796 || lcddev.id == 0X5310 || lcddev.id == 0X5510 || lcddev.id == 0X7789 || lcddev.id == 0X9806)  /* MCU����SPI�ӿ������е��败�����͵��ݴ����� */
    {
        if (gt9xxx_init() == 0)             /* ��ʼ��GTϵ�д������ɹ�,����ǰLCD��Ϊ���ݴ����� */
        {
            tp_dev.scan = gt9xxx_scan;      /* ɨ�躯��ָ��gt9xxx������ɨ�� */
            tp_dev.touchtype |= 0X80;       /* ������ */
            return 0;
        }
    }

    if (lcddev.id == 0X4342 || lcddev.id == 0X4384 || lcddev.id == 0X5571 || lcddev.id == 0X8081 || lcddev.id == 0X1018)  /* gtxxϵ�е��ݴ����� */    
    {
        gt9xxx_init();
        tp_dev.scan = gt9xxx_scan;          /* ɨ�躯��ָ��gt9xxx������ɨ�� */
        tp_dev.touchtype |= 0X80;           /* ������ */
        return 0;
    }
    else if (lcddev.id == 0X7084 || lcddev.id == 0X7016)   /* ftxxϵ�е��ݴ����� */
    {
        // if (!ft5206_init())                 /* ����IC��FTϵ�еľ�ִ��ft5206_init�����Լ�ʹ��ft5206_scanɨ�躯�� */
        // {
        //     tp_dev.scan = ft5206_scan;      /* ɨ�躯��ָ��FT5206������ɨ�� */
        // }
        // else                                /* ����IC��GTϵ�еľ�ִ��gt9xxx_init�����Լ�ʹ��gt9xxx_scanɨ�躯�� */
        // {
            gt9xxx_init();
            tp_dev.scan = gt9xxx_scan;      /* ɨ�躯��ָ��gt9xxx������ɨ�� */
        // }
        
        tp_dev.touchtype |= 0X80;           /* ������ */
        return 0;
    }    
      
    /* ���败���� */
    T_PEN_GPIO_CLK_ENABLE();    /* T_PEN��ʱ��ʹ�� */
    T_CS_GPIO_CLK_ENABLE();     /* T_CS��ʱ��ʹ�� */
    T_MISO_GPIO_CLK_ENABLE();   /* T_MISO��ʱ��ʹ�� */
    T_MOSI_GPIO_CLK_ENABLE();   /* T_MOSI��ʱ��ʹ�� */
    T_CLK_GPIO_CLK_ENABLE();    /* T_CLK��ʱ��ʹ�� */

    sys_gpio_set(T_PEN_GPIO_PORT, T_PEN_GPIO_PIN,
                 SYS_GPIO_MODE_IN, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_PU);  /* T_PEN����ģʽ����,�������� */

    sys_gpio_set(T_MISO_GPIO_PORT, T_MISO_GPIO_PIN,
                 SYS_GPIO_MODE_IN, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_PU);  /* T_MISO����ģʽ����,�������� */

    sys_gpio_set(T_MOSI_GPIO_PORT, T_MOSI_GPIO_PIN,
                 SYS_GPIO_MODE_OUT, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_PU); /* T_MOSI����ģʽ����,������� */

    sys_gpio_set(T_CLK_GPIO_PORT, T_CLK_GPIO_PIN,
                 SYS_GPIO_MODE_OUT, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_PU); /* T_CLK����ģʽ����,������� */

    sys_gpio_set(T_CS_GPIO_PORT, T_CS_GPIO_PIN,
                 SYS_GPIO_MODE_OUT, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_PU); /* T_CS����ģʽ����,������� */

    tp_read_xy(&tp_dev.x[0], &tp_dev.y[0]); /* ��һ�ζ�ȡ��ʼ�� */
    at24cxx_init();                         /* ��ʼ��24CXX */

    if (tp_get_adjust_data())
    {
        return 0;                           /* �Ѿ�У׼ */
    }
    else                                    /* δУ׼? */
    {
        lcd_clear(WHITE);                   /* ���� */
        tp_adjust();                        /* ��ĻУ׼ */
        tp_save_adjust_data();
    }

    tp_get_adjust_data();
    
    return 1;
}









