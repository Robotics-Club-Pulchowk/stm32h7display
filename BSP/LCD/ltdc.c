/**
 ****************************************************************************************************
 * @file        ltdc.c
 * @version     V1.0
 * @brief       LTDC ��������
 ****************************************************************************************************
 * @attention   Waiken-Smart ������Զ
 *
 * ʵ��ƽ̨:    STM32H743IIT6Сϵͳ��
 *
 ****************************************************************************************************
 */
 
#include "lcd.h"
#include "ltdc.h"
#include "delay.h"


#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
/* AC6 compiler */
#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888 || LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888
    uint32_t ltdc_lcd_framebuf[1280][800] __attribute__((section(".bss.ARM.__at_0XC0000000")));
#else
    uint16_t ltdc_lcd_framebuf[1280][800] __attribute__((section(".bss.ARM.__at_0XC0000000")));
#endif

#elif defined(__ARMCC_VERSION)
/* AC5 compiler */
#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888 || LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888
    uint32_t ltdc_lcd_framebuf[1280][800] __attribute__((at(LTDC_FRAME_BUF_ADDR)));
#else
    uint16_t ltdc_lcd_framebuf[1280][800] __attribute__((at(LTDC_FRAME_BUF_ADDR)));
#endif

#else
/* GCC compiler */
#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888 || LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888
    uint32_t ltdc_lcd_framebuf[1280][800] __attribute__((section(".sdram_bss")));
#else
    uint16_t ltdc_lcd_framebuf[1280][800] __attribute__((section(".sdram_bss")));
#endif
#endif


uint32_t *g_ltdc_framebuf[2];       /* LTDC LCD֡��������ָ��,����ָ���Ӧ��С���ڴ����� */
_ltdc_dev lcdltdc;                  /* ����LCD LTDC����Ҫ���� */


/**
 * @brief       LTDC����
 * @param       sw          : 1,��; 0,�ر�;
 * @retval      ��
 */
void ltdc_switch(uint8_t sw)
{
    if (sw)
    {
        LTDC->GCR |= 1 << 0;    /* ��LTDC */
    }
    else
    {
        LTDC->GCR &= ~(1 << 0); /* �ر�LTDC */
    }
}

/**
 * @brief       LTDC����ָ����
 * @param       layerx      : 0,��һ��; 1,�ڶ���;
 * @param       sw          : 1,��;   0,�ر�;
 * @retval      ��
 */
void ltdc_layer_switch(uint8_t layerx, uint8_t sw)
{
    if (sw)
    {
        if (layerx == 0)LTDC_Layer1->CR |= 1 << 0;      /* ������1 */
        else LTDC_Layer2->CR |= 1 << 0;                 /* ������2 */
    }
    else
    {
        if (layerx == 0)LTDC_Layer1->CR &= ~(1 << 0);   /* �رղ�1 */
        else LTDC_Layer2->CR &= ~(1 << 0);              /* �رղ�2 */
    }

    LTDC->SRCR |= 1 << 0;                               /* �������¼������� */
}

/**
 * @brief       LTDCѡ���
 * @param       layerx      : 0,��һ��; 1,�ڶ���;
 * @retval      ��
 */
void ltdc_select_layer(uint8_t layerx)
{
    lcdltdc.activelayer = layerx;
}

/**
 * @brief       LTDC��ʾ��������
 * @param       dir         : 0,����; 1,����;
 * @retval      ��
 */
void ltdc_display_dir(uint8_t dir)
{
    lcdltdc.dir = dir;      /* ��ʾ���� */

    if (dir == 0)           /* ���� */
    {
        lcdltdc.width = lcdltdc.pheight;
        lcdltdc.height = lcdltdc.pwidth;
    }
    else if (dir == 1)      /* ���� */
    {
        lcdltdc.width = lcdltdc.pwidth;
        lcdltdc.height = lcdltdc.pheight;
    }
}

/**
 * @brief       LTDC���㺯��
 * @param       x,y         : ����
 * @param       color       : ��ɫֵ
 * @retval      ��
 */
void ltdc_draw_point(uint16_t x, uint16_t y, uint32_t color)
{
#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888

    if (lcdltdc.dir)    /* ���� */
    {
        *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) = color;
    }
    else                /* ���� */
    {
        *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) = color;
    }

#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888

    if (lcdltdc.dir)     /* ���� */
    {
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) = color;
        *(uint8_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x) + 2) = color >> 16;
    }
    else                /* ���� */
    {
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) = color;
        *(uint8_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y) + 2) = color >> 16;
    }
    
#else

    if (lcdltdc.dir)    /* ���� */
    {
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) = color;
    }
    else                /* ���� */
    {
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) = color;
    }

#endif
}

/**
 * @brief       LTDC���㺯��
 * @param       x,y         : ����
 * @retval      ��ɫֵ
 */
uint32_t ltdc_read_point(uint16_t x, uint16_t y)
{
#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888

    if (lcdltdc.dir)    /* ���� */
    {
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x));
    }
    else                /* ���� */
    {
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y));
    }

#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888

    if (lcdltdc.dir)    /* ���� */
    {
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) & 0XFFFFFF;
    }
    else                /* ���� */
    {
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) & 0XFFFFFF;
    }
    
#else

    if (lcdltdc.dir)    /* ���� */
    {
        return *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x));
    }
    else                /* ���� */
    {
        return *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y));
    }

#endif
}

/**
 * @brief       LTDC������, DMA2D���
 * @note       (sx,sy),(ex,ey):�����ζԽ�����,�����СΪ:(ex - sx + 1) * (ey - sy + 1)
 *              ע��:sx,ex,���ܴ���lcddev.width - 1; sy,ey,���ܴ���lcddev.height - 1
 * @param       sx,sy       : ��ʼ����
 * @param       ex,ey       : ��������
 * @param       color       : ������ɫ
 * @retval      ��
 */
void ltdc_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color)
{
    uint32_t psx, psy, pex, pey;        /* ��LCD���Ϊ��׼������ϵ,����������仯���仯 */
    uint32_t timeout = 0;
    uint16_t offline;
    uint32_t addr;

    /* ����ϵת�� */
    if (lcdltdc.dir)                    /* ���� */
    {
        psx = sx;
        psy = sy;
        pex = ex;
        pey = ey;
    }
    else                                /* ���� */
    {
        if (ex >= lcdltdc.pheight)
        {
            ex = lcdltdc.pheight - 1;   /* ���Ʒ�Χ */
        }
        
        if (sx >= lcdltdc.pheight)
        {
            sx = lcdltdc.pheight - 1;   /* ���Ʒ�Χ */
        }
        
        psx = sy;
        psy = lcdltdc.pheight - ex - 1;
        pex = ey;
        pey = lcdltdc.pheight - sx - 1;
    }

    offline = lcdltdc.pwidth - (pex - psx + 1);   /* ��ƫ��:��ǰ�����һ�����غ���һ�е�һ������֮���������Ŀ */
    addr = ((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * psy + psx));
    
    RCC->AHB3ENR |= 1 << 4;               /* ʹ��DMA2Dʱ�� */
    DMA2D->CR &= ~(1 << 0);               /* ��ֹͣDMA2D */
    DMA2D->CR = 3 << 16;                  /* �Ĵ������洢��ģʽ */
    DMA2D->OPFCCR = LTDC_PIXFORMAT;       /* ������ɫ��ʽ */
    DMA2D->OOR = offline;                 /* ������ƫ�� */
    DMA2D->OMAR = addr;                   /* ����洢����ַ */
    DMA2D->NLR = (pey - psy + 1) | ((pex - psx + 1) << 16); /* �趨�����Ĵ��� */
    DMA2D->OCOLR = color;                 /* �趨�����ɫ�Ĵ��� */
    DMA2D->CR |= 1 << 0;                  /* ����DMA2D */

    while ((DMA2D->ISR & (1 << 1)) == 0)  /* �ȴ�������� */
    {
        timeout++;

        if (timeout > 0X1FFFFF)break;     /* ��ʱ�˳� */
    }

    DMA2D->IFCR |= 1 << 1;                /* ���������ɱ�־ */
}

/**
 * @brief       ��ָ�����������ָ����ɫ��, DMA2D���
 * @note        �˺�����֧��uint16_t,RGB565��ʽ����ɫ�������.
 *              (sx,sy),(ex,ey):�����ζԽ�����,�����СΪ:(ex - sx + 1) * (ey - sy + 1)
 *              ע��:sx,ex,���ܴ���lcddev.width - 1; sy,ey,���ܴ���lcddev.height - 1
 * @param       sx,sy       : ��ʼ����
 * @param       ex,ey       : ��������
 * @param       color       : ������ɫ�����׵�ַ
 * @retval      ��
 */
void ltdc_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    uint32_t psx, psy, pex, pey;   /* ��LCD���Ϊ��׼������ϵ,����������仯���仯 */
    uint32_t timeout = 0;
    uint16_t offline;
    uint32_t addr;

    /* ����ϵת�� */
    if (lcdltdc.dir)               /* ���� */
    {
        psx = sx;
        psy = sy;
        pex = ex;
        pey = ey;
    }
    else                           /* ���� */
    {
        psx = sy;
        psy = lcdltdc.pheight - ex - 1;
        pex = ey;
        pey = lcdltdc.pheight - sx - 1;
    }

    offline = lcdltdc.pwidth - (pex - psx + 1);   /* ��ƫ��:��ǰ�����һ�����غ���һ�е�һ������֮���������Ŀ */
    addr = ((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * psy + psx));
    
    RCC->AHB3ENR |= 1 << 4;               /* ʹ��DMA2Dʱ�� */
    DMA2D->CR &= ~(1 << 0);               /* ��ֹͣDMA2D */
    DMA2D->CR = 0 << 16;                  /* �洢�����洢��ģʽ */
    DMA2D->FGPFCCR = LTDC_PIXFORMAT;      /* ����ǰ������ɫ��ʽ */
    DMA2D->FGOR = 0;                      /* ǰ������ƫ��Ϊ0 */
    DMA2D->OOR = offline;                 /* ������ƫ�� */
    DMA2D->FGMAR = (uint32_t)color;       /* Դ��ַ */
    DMA2D->OMAR = addr;                   /* ����洢����ַ */
    DMA2D->NLR = (pey - psy + 1) | ((pex - psx + 1) << 16); /* �趨�����Ĵ��� */
    DMA2D->CR |= 1 << 0;                  /* ����DMA2D */

    while ((DMA2D->ISR & (1 << 1)) == 0)  /* �ȴ�������� */
    {
        timeout++;

        if (timeout > 0X1FFFFF)break;     /* ��ʱ�˳� */
    }

    DMA2D->IFCR |= 1 << 1;                /* ���������ɱ�־ */
}

/**
 * @brief       LTDC����
 * @param       color       : ��ɫֵ
 * @retval      ��
 */
void ltdc_clear(uint32_t color)
{
    ltdc_fill(0, 0, lcdltdc.width - 1, lcdltdc.height - 1, color);
}

/**
 * @brief       LTDCʱ��(Fdclk)���ú���
 * @param       pll3n     : PLL3 VCO��Ƶϵ��(PLL��Ƶ),        ȡֵ��Χ:4~512.
 * @param       pll3m     : PLL3Ԥ��Ƶϵ��(��PLL֮ǰ�ķ�Ƶ),  ȡֵ��Χ:1~63.
 * @param       pll3r     : PLL3��r��Ƶϵ��(PLL֮��ķ�Ƶ),   ȡֵ��Χ:1~128.
 *
 * @note        Fvco = Fs * (pll3n / pll3m);
 *              Fr = Fvco / pll3r = Fs * (pll3n / (pll3m * pll3r));
 *              Fdclk = Fr;
 *              ����:
 *              Fvco: VCOƵ��
 *              Fr: PLL3��r��Ƶ���ʱ��Ƶ��
 *              Fs: PLL3����ʱ��Ƶ��,������HSI,CSI,HSE��(ϵͳʱ�ӳ�ʼ��ʱѡ��HSE��ΪPLL������ʱ��Դ).
 *
 *              ����:�ⲿ����Ϊ25M, pllm = 25 ��ʱ��, Fs = 25Mhz�� pllm��Ƶ��Ƶ�� Ϊ1Mhz.
 *              ����: Ҫ�õ�33M��LTDCʱ��, ���������: pll3n = 300, pllm = 25, pll3r = 9
 *              Fdclk= ((25 / 25) * 300) / 9 = 33 Mhz
 * @retval      0, �ɹ�;
 *              ����, ʧ��;
 */
uint8_t ltdc_clk_set(uint32_t pll3n, uint32_t pll3m, uint32_t pll3r)
{
    uint16_t retry = 0;
    uint8_t status = 0;
    
    RCC->CR &= ~(1 << 28);  /* �ر�PLL3ʱ�� */

    while (((RCC->CR & (1 << 29))) && (retry < 0X1FFF))retry++; /* �ȴ�PLL3ʱ��ʧ�� */

    if (retry == 0X1FFF)status = 1;         /* PLL3ʱ�ӹر�ʧ�� */
    else
    {
        RCC->PLLCKSELR &= ~(0X3F << 20);    /* ���DIVM3[5:0]ԭ�������� */
        RCC->PLLCKSELR |= pll3m << 20;      /* DIVM3[5:0] = pll3m,����PLL3��Ԥ��Ƶϵ�� */
        RCC->PLL3DIVR &= ~(0X1FF << 0);     /* ���DIVN3[8:0]ԭ�������� */
        RCC->PLL3DIVR |= (pll3n - 1) << 0;  /* DIVN3[8:0] = pll3n - 1,����PLL3 VCO�ı�Ƶϵ��,����ֵ���1 */
        RCC->PLL3DIVR &= ~(0X7F << 24);     /* ���DIVR3[6:0]ԭ�������� */
        RCC->PLL3DIVR |= (pll3r - 1) << 24; /* DIVR3[6:0] = pll3r - 1,����PLL3��r��Ƶϵ��,����ֵ���1 */

        RCC->PLLCFGR &= ~(0X0F << 8);       /* ����PLL3RGE[1:0]/PLL3VCOSEL/PLL3FRACEN������ */
        RCC->PLLCFGR |= 0 << 10;            /* PLL3RGE[1:0] = 0,PLL3����ʱ��Ƶ����1~2Mhz֮��(25 / 25 = 1Mhz) */
        RCC->PLLCFGR |= 0 << 9;             /* PLL3VCOSEL = 0, PLL3��VCO��Χ,192~836Mhz */
        RCC->PLLCFGR |= 1 << 24;            /* DIVR3EN = 1, ʹ��pll3_r_ck��� */
        RCC->CR |= 1 << 28;                 /* PLL3ON = 1, ʹ��PLL3 */

        while (((RCC->CR & (1 << 29)) == 0) && (retry < 0X1FFF))retry++;    /* �ȴ�PLL3ʱ������ */

        if (retry == 0X1FFF)status = 2;     /* PLL3ʱ�ӿ���ʧ�� */
    }

    return status;
}

/**
 * @brief       LTDC�㴰������, ������LCD�������ϵΪ��׼
 * @note        �˺���������ltdc_layer_parameter_config֮��������.����,�����õĴ���ֵ���������ĳ�
 *              ��ʱ,GRAM�Ĳ���(��/д�㺯��),ҲҪ���ݴ��ڵĿ����������޸�,������ʾ������(�����̾�δ���޸�).
 * @param       layerx      : 0,��һ��; 1,�ڶ���;
 * @param       sx, sy      : ��ʼ����
 * @param       width,height: ���Ⱥ͸߶�
 * @retval      ��
 */
void ltdc_layer_window_config(uint8_t layerx, uint16_t sx, uint16_t sy, uint16_t width, uint16_t height)
{
    uint32_t temp;
    uint8_t pixformat = 0;

    if (layerx == 0)
    {
        temp = (sx + width + ((LTDC->BPCR & 0X0FFF0000) >> 16)) << 16;
        LTDC_Layer1->WHPCR = (sx + ((LTDC->BPCR & 0X0FFF0000) >> 16) + 1) | temp;   /* ��������ʼ�ͽ���λ�� */
        temp = (sy + height + (LTDC->BPCR & 0X7FF)) << 16;
        LTDC_Layer1->WVPCR = (sy + (LTDC->BPCR & 0X7FF) + 1) | temp;    /* ��������ʼ�ͽ���λ�� */
        pixformat = LTDC_Layer1->PFCR & 0X07;                           /* �õ������ظ�ʽ */

        if (pixformat == 0)temp = 4;                                    /* ARGB8888,һ�����ص�4���ֽ� */
        else if (pixformat == 1)temp = 3;                               /* RGB888,һ�����ص�3���ֽ� */
        else if (pixformat == 5 || pixformat == 6)temp = 1;             /* L8/AL44,һ�����ص�1���ֽ� */
        else temp = 2;                                                  /* ������ʽ,һ�����ص�2���ֽ� */

        if (lcdltdc.pheight == 1280)
        {
            temp = 4;
        }
        
        LTDC_Layer1->CFBLR = (width * temp << 16) | (width * temp + 7); /* ֡�������г����м������(���ֽ�Ϊ��λ) */
        LTDC_Layer1->CFBLNR = height;                                   /* ֡�������������� */
    }
    else
    {
        temp = (sx + width + ((LTDC->BPCR & 0X0FFF0000) >> 16)) << 16;
        LTDC_Layer2->WHPCR = (sx + ((LTDC->BPCR & 0X0FFF0000) >> 16) + 1) | temp;   /* ��������ʼ�ͽ���λ�� */
        temp = (sy + height + (LTDC->BPCR & 0X7FF)) << 16;
        LTDC_Layer2->WVPCR = (sy + (LTDC->BPCR & 0X7FF) + 1) | temp;    /* ��������ʼ�ͽ���λ�� */
        pixformat = LTDC_Layer2->PFCR & 0X07;                           /* �õ������ظ�ʽ */

        if (pixformat == 0)temp = 4;                                    /* ARGB8888,һ�����ص�4���ֽ� */
        else if (pixformat == 1)temp = 3;                               /* RGB888,һ�����ص�3���ֽ� */
        else if (pixformat == 5 || pixformat == 6)temp = 1;             /* L8/AL44,һ�����ص�1���ֽ� */
        else temp = 2;                                                  /* ������ʽ,һ�����ص�2���ֽ� */

        LTDC_Layer2->CFBLR = (width * temp << 16) | (width * temp + 7); /* ֡�������г����м������(���ֽ�Ϊ��λ) */
        LTDC_Layer2->CFBLNR = height;                                   /* ֡�������������� */
    }

    ltdc_layer_switch(layerx, 1);                                       /* ��ʹ�� */
}


/**
 * @brief       LTDC�������������
 * @note        �˺���,������ltdc_layer_window_config֮ǰ����.
 * @param       layerx      : 0,��һ��; 1,�ڶ���;
 * @param       bufaddr     : ����ɫ֡������ʼ��ַ
 * @param       pixformat   : �����ظ�ʽ. 0,ARGB8888; 1,RGB888; 2,RGB565; 3,ARGB1555; 4,ARGB4444; 5,L8; 6;AL44; 7;AL88
 * @param       alpha       : ��㶨Alphaֵ, 0,ȫ͸��;255,��͸��
 * @param       alpha0      : Ĭ����ɫAlphaֵ, 0,ȫ͸��;255,��͸��
 * @param       bfac1       : ���ϵ��1, 4(100),�㶨Alpha; 6(101),����Alpha*�㶨Alpha
 * @param       bfac2       : ���ϵ��2, 5(101),�㶨Alpha; 7(111),����Alpha*�㶨Alpha
 * @param       bkcolor     : ��Ĭ����ɫ,32λ,��24λ��Ч,RGB888��ʽ
 * @retval      ��
 */
void ltdc_layer_parameter_config(uint8_t layerx, uint32_t bufaddr, uint8_t pixformat, uint8_t alpha, uint8_t alpha0, uint8_t bfac1, uint8_t bfac2, uint32_t bkcolor)
{
    if (layerx == 0)
    {
        LTDC_Layer1->CFBAR = bufaddr;                           /* ���ò���ɫ֡������ʼ��ַ */
        LTDC_Layer1->PFCR = pixformat;                          /* ���ò����ظ�ʽ */
        LTDC_Layer1->CACR = alpha;                              /* ���ò�㶨Alphaֵ,0~255,Ӳ��255��Ƶ;����255,��͸�� */
        LTDC_Layer1->DCCR = ((uint32_t)alpha0 << 24) | bkcolor; /* ����Ĭ����ɫAlphaֵ,�Լ�Ĭ����ɫ */
        LTDC_Layer1->BFCR = ((uint32_t)bfac1 << 8) | bfac2;     /* ���ò���ϵ�� */
    }
    else
    {
        LTDC_Layer2->CFBAR = bufaddr;                           /* ���ò���ɫ֡������ʼ��ַ */
        LTDC_Layer2->PFCR = pixformat;                          /* ���ò����ظ�ʽ */
        LTDC_Layer2->CACR = alpha;                              /* ���ò�㶨Alphaֵ,Ӳ��255��Ƶ;����255,��͸�� */
        LTDC_Layer2->DCCR = ((uint32_t)alpha0 << 24) | bkcolor; /* ���ò�Ĭ����ɫAlphaֵ,�Լ�Ĭ����ɫ */
        LTDC_Layer2->BFCR = ((uint32_t)bfac1 << 8) | bfac2;     /* ���ò���ϵ�� */
    }
}

/**
 * @brief       LTDC��ȡ���ID
 * @note        ����LCD RGB�ߵ����λ(R7,G7,B7)��ʶ�����ID
 *              PG6 = R7(M0); PI2 = G7(M1); PI7 = B7(M2);
 *              M2:M1:M0
 *              0 :0 :0     4.3 ��480*272  RGB��,ID = 0X4342
 *              0 :0 :1     7   ��800*480  RGB��,ID = 0X7084
 *              0 :1 :0     7   ��1024*600 RGB��,ID = 0X7016
 *              0 :1 :1     5.5 ��720*1280 RGB��,ID = 0X5571
 *              1 :0 :0     4.3 ��800*480  RGB��,ID = 0X4384
 *              1 :0 :1     10.1��1280*800 RGB��,ID = 0X1018
 * @param       ��
 * @retval      0, �Ƿ�; 
 *              ����, LCD ID
 */
uint16_t ltdc_panelid_read(void)
{
    uint8_t idx = 0;
    
    RCC->AHB4ENR |= 1 << 6 | 1 << 8;    /* ʹ��PG/PIʱ�� */

    sys_gpio_set(GPIOG, SYS_GPIO_PIN6,
                 SYS_GPIO_MODE_IN, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);   /* R7����ģʽ����,�������� */
 
    sys_gpio_set(GPIOI, SYS_GPIO_PIN2 | SYS_GPIO_PIN7,
                 SYS_GPIO_MODE_IN, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);   /* G7,B7����ģʽ����,�������� */

    delay_us(10);
    idx  = sys_gpio_pin_get(GPIOG, SYS_GPIO_PIN6);      /* ��ȡM0 */
    idx |= sys_gpio_pin_get(GPIOI, SYS_GPIO_PIN2) << 1; /* ��ȡM1 */
    idx |= sys_gpio_pin_get(GPIOI, SYS_GPIO_PIN7) << 2; /* ��ȡM2 */

    switch (idx)
    {
        case 0:
            return 0X4342;      /* 4.3����,480*272�ֱ��� */

        case 1:
            return 0X7084;      /* 7����,800*480�ֱ��� */

        case 2:
            return 0X7016;      /* 7����,1024*600�ֱ��� */

        case 3:
            return 0X5571;      /* 5.5����,720*1280�ֱ��� */

        case 4:
            return 0X4384;      /* 4.3����,800*480�ֱ��� */

        case 5:
            return 0X1018;      /* 10.1����,1280*800�ֱ��� */

        default:
            return 0;
    }
}

/**
 * @brief       LTDC��ʼ������
 * @param       ��
 * @retval      ��
 */
void ltdc_init(void)
{
    uint32_t tempreg = 0;
    uint16_t lcdid = 0;
    
    lcdid = ltdc_panelid_read();    /* ��ȡLCD���ID */

    if (lcdid == 0X5571)
    {
        
    }
    
    /* LTDC�źſ������� LTDC_DE(PF10), LTDC_VSYNC(PI9), LTDC_HSYNC(PI10), LTDC_CLK(PG7) */
    /* LTDC ��������    LTDC_R7(PG6), LTDC_R6(PH12), LTDC_R5(PH11), LTDC_R4(PH10), LTDC_R3(PH9), LTDC_R2(PH8), LTDC_R1(PA2), LTDC_R0(PG13),
                       LTDC_G7(PI2), LTDC_G6(PI1), LTDC_G5(PI0), LTDC_G4(PH15), LTDC_G3(PH14), LTDC_G2(PH13), LTDC_G1(PE6), LTDC_G0(PE5),
                       LTDC_B7(PI7), LTDC_B6(PI6), LTDC_B5(PI5), LTDC_B4(PI4), LTDC_B3(PA8), LTDC_B2(PD6), LTDC_B1(PG12), LTDC_B0(PG14) */
  
    /* ������LTDC�źſ������� DE/VSYNC/HSYNC/CLK�ȵ����� */
    LTDC_BL_GPIO_CLK_ENABLE();      /* LTDC_BL����ʱ��ʹ�� */
    LTDC_RST_GPIO_CLK_ENABLE();     /* LTDC_RST����ʱ��ʹ�� */  
    LTDC_DE_GPIO_CLK_ENABLE();      /* LTDC_DE����ʱ��ʹ�� */
    LTDC_VSYNC_GPIO_CLK_ENABLE();   /* LTDC_VSYNC����ʱ��ʹ�� */
    LTDC_HSYNC_GPIO_CLK_ENABLE();   /* LTDC_HSYNC����ʱ��ʹ�� */
    LTDC_CLK_GPIO_CLK_ENABLE();     /* LTDC_CLK����ʱ��ʹ�� */

    sys_gpio_set(LTDC_BL_GPIO_PORT, LTDC_BL_GPIO_PIN,
                 SYS_GPIO_MODE_OUT, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);  /* LTDC_BL����ģʽ����(�������) */

    sys_gpio_set(LTDC_RST_GPIO_PORT, LTDC_RST_GPIO_PIN,
                 SYS_GPIO_MODE_OUT, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);  /* LTDC_RST����ģʽ����(�������) */
  
    sys_gpio_set(LTDC_DE_GPIO_PORT, LTDC_DE_GPIO_PIN,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);   /* LTDC_DE����ģʽ���� */

    sys_gpio_set(LTDC_VSYNC_GPIO_PORT, LTDC_VSYNC_GPIO_PIN,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);   /* LTDC_VSYNC����ģʽ���� */

    sys_gpio_set(LTDC_HSYNC_GPIO_PORT, LTDC_HSYNC_GPIO_PIN,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);   /* LTDC_HSYNC����ģʽ���� */

    sys_gpio_set(LTDC_CLK_GPIO_PORT, LTDC_CLK_GPIO_PIN,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);   /* LTDC_CLK����ģʽ���� */

    sys_gpio_af_set(LTDC_DE_GPIO_PORT, LTDC_DE_GPIO_PIN, 14);       /* LTDC_DE��, AF14 */
    sys_gpio_af_set(LTDC_VSYNC_GPIO_PORT, LTDC_VSYNC_GPIO_PIN, 14); /* LTDC_VSYNC��, AF14 */
    sys_gpio_af_set(LTDC_HSYNC_GPIO_PORT, LTDC_HSYNC_GPIO_PIN, 14); /* LTDC_HSYNC��, AF14 */
    sys_gpio_af_set(LTDC_CLK_GPIO_PORT, LTDC_CLK_GPIO_PIN, 14);     /* LTDC_CLK��, AF14 */

    /* ������LTDC �������ŵ����� */
    RCC->APB3ENR |= 1 << 3;             /* ����LTDCʱ�� */
    RCC->AHB4ENR |= 1 << 0;             /* ʹ��PAʱ�� */
    RCC->AHB4ENR |= 3 << 3;             /* ʹ��PD/PEʱ�� */
    RCC->AHB4ENR |= 0X7 << 6;           /* ʹ��PG/PH/PIʱ�� */

    sys_gpio_set(GPIOA, SYS_GPIO_PIN2 | SYS_GPIO_PIN8,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);   /* LTDC ��������ģʽ���� */

    sys_gpio_set(GPIOD, SYS_GPIO_PIN6,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);   /* LTDC ��������ģʽ���� */
                 
    sys_gpio_set(GPIOE, SYS_GPIO_PIN5 | SYS_GPIO_PIN6,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);   /* LTDC ��������ģʽ���� */
                 
    sys_gpio_set(GPIOG, SYS_GPIO_PIN6 | 7 << 12,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);   /* LTDC ��������ģʽ���� */
                 
    sys_gpio_set(GPIOH, 0XFF << 8,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);   /* LTDC ��������ģʽ���� */

    sys_gpio_set(GPIOI, 7 << 0 | 0XF << 4,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);   /* LTDC ��������ģʽ���� */

    sys_gpio_af_set(GPIOA, SYS_GPIO_PIN2, 14);                      /* LTDC ��������, AF14 */
    sys_gpio_af_set(GPIOA, SYS_GPIO_PIN8, 13);                      /* LTDC ��������, AF13 */
    sys_gpio_af_set(GPIOD, SYS_GPIO_PIN6, 14);                      /* LTDC ��������, AF14 */
    sys_gpio_af_set(GPIOE, SYS_GPIO_PIN5 | SYS_GPIO_PIN6, 14);      /* LTDC ��������, AF14 */
    sys_gpio_af_set(GPIOG, SYS_GPIO_PIN6 | 7 << 12, 14);            /* LTDC ��������, AF14 */
    sys_gpio_af_set(GPIOH, 0XFF << 8, 14);                          /* LTDC ��������, AF14 */
    sys_gpio_af_set(GPIOI, 7 << 0 | 0XF << 4, 14);                  /* LTDC ��������, AF14 */

#if RGB_80_8001280         
    lcdid = 0X8081;
#endif

    if (lcdid == 0X4342)
    {
        lcdltdc.pwidth = 480;       /* ������,��λ:���� */
        lcdltdc.pheight = 272;      /* ���߶�,��λ:���� */
        lcdltdc.hsw = 1;            /* ˮƽͬ������ */
        lcdltdc.hbp = 40;           /* ˮƽ���� */
        lcdltdc.hfp = 5;            /* ˮƽǰ�� */      
        lcdltdc.vsw = 1;            /* ��ֱͬ������ */
        lcdltdc.vbp = 8;            /* ��ֱ���� */      
        lcdltdc.vfp = 8;            /* ��ֱǰ�� */
        ltdc_clk_set(300, 25, 33);  /* ��������ʱ��  9Mhz */
    }
    else if (lcdid == 0X7084)
    {
        lcdltdc.pwidth = 800;       /* ������,��λ:���� */
        lcdltdc.pheight = 480;      /* ���߶�,��λ:���� */
        lcdltdc.hsw = 1;            /* ˮƽͬ������ */
        lcdltdc.hbp = 46;           /* ˮƽ���� */
        lcdltdc.hfp = 210;          /* ˮƽǰ�� */
        lcdltdc.vsw = 1;            /* ��ֱͬ������ */
        lcdltdc.vbp = 23;           /* ��ֱ���� */
        lcdltdc.vfp = 22;           /* ��ֱǰ�� */
        ltdc_clk_set(300, 25, 9);   /* ��������ʱ�� 33Mhz(�����˫��,��Ҫ����DCLK��18.75Mhz,�Ż�ȽϺ�) */
    }
    else if (lcdid == 0X7016)
    {
        lcdltdc.pwidth = 1024;      /* ������,��λ:���� */
        lcdltdc.pheight = 600;      /* ���߶�,��λ:���� */
        lcdltdc.hsw = 20;           /* ˮƽͬ������ */
        lcdltdc.hbp = 140;          /* ˮƽ���� */
        lcdltdc.hfp = 160;          /* ˮƽǰ�� */
        lcdltdc.vsw = 3;            /* ��ֱͬ������ */
        lcdltdc.vbp = 20;           /* ��ֱ���� */
        lcdltdc.vfp = 12;           /* ��ֱǰ�� */
        ltdc_clk_set(300, 25, 6);   /* ��������ʱ��  50Mhz */
    }
    else if (lcdid == 0X5571)
    {
        lcdltdc.pwidth = 720;       /* ������,��λ:���� */
        lcdltdc.pheight = 1280;     /* ���߶�,��λ:���� */
        lcdltdc.hsw = 10;           /* ˮƽͬ������ */
        lcdltdc.hbp = 36;           /* ˮƽ���� */
        lcdltdc.hfp = 46;           /* ˮƽǰ�� */
        lcdltdc.vsw = 5;            /* ��ֱͬ������ */
        lcdltdc.vbp = 5;            /* ��ֱ���� */
        lcdltdc.vfp = 16;           /* ��ֱǰ�� */
        ltdc_clk_set(330, 25, 6);   /* ��������ʱ��  55Mhz */
    }
    else if (lcdid == 0X4384)
    {
        lcdltdc.pwidth = 800;       /* ������,��λ:���� */
        lcdltdc.pheight = 480;      /* ���߶�,��λ:���� */
        lcdltdc.hsw = 48;           /* ˮƽͬ������ */      
        lcdltdc.hbp = 88;           /* ˮƽ���� */
        lcdltdc.hfp = 40;           /* ˮƽǰ�� */
        lcdltdc.vsw = 3;            /* ��ֱͬ������ */
        lcdltdc.vbp = 32;           /* ��ֱ���� */
        lcdltdc.vfp = 13;           /* ��ֱǰ�� */
        ltdc_clk_set(300, 25, 9);   /* ��������ʱ�� 33Mhz */ 
    }
    else if (lcdid == 0X8081)       /* 8��800*1280 RGB�� */
    {
        lcdltdc.pwidth = 800;       /* ������,��λ:���� */
        lcdltdc.pheight = 1280;     /* ���߶�,��λ:���� */
        lcdltdc.hsw = 5;            /* ˮƽͬ������ */
        lcdltdc.hbp = 20;           /* ˮƽ���� */
        lcdltdc.hfp = 40;           /* ˮƽǰ�� */
        lcdltdc.vsw = 3;            /* ��ֱͬ������ */
        lcdltdc.vbp = 20;           /* ��ֱ���� */
        lcdltdc.vfp = 30;           /* ��ֱǰ�� */
        ltdc_clk_set(300, 25, 5);   /* ��������ʱ��  60Mhz */
    }
    else if (lcdid == 0X1018)       /* 10.1��1280*800 RGB�� */
    {
        lcdltdc.pwidth = 1280;      /* ������,��λ:���� */
        lcdltdc.pheight = 800;      /* ���߶�,��λ:���� */
        lcdltdc.hsw = 10;           /* ˮƽͬ������ */
        lcdltdc.hbp = 140;          /* ˮƽ���� */
        lcdltdc.hfp = 10;           /* ˮƽǰ�� */
        lcdltdc.vsw = 3;            /* ��ֱͬ������ */
        lcdltdc.vbp = 10;           /* ��ֱ���� */
        lcdltdc.vfp = 10;           /* ��ֱǰ�� */
        ltdc_clk_set(300, 25, 5);   /* ��������ʱ��  60Mhz */
    }

    if (lcdid == 0X1018 || lcdid == 0X8081)
    {
        tempreg = 1 << 28;          /* ����ʱ�Ӽ���:�ߵ�ƽ��Ч */
    }
    else
    {
        tempreg = 0 << 28;          /* ����ʱ�Ӽ���:�͵�ƽ��Ч */
    }

    tempreg |= 0 << 29;             /* ����ʹ�ܼ���:�͵�ƽ��Ч */
    tempreg |= 0 << 30;             /* ��ֱͬ������:�͵�ƽ��Ч */
    
    if (lcdid == 0X8081)
    {
        tempreg |= (uint32_t)1 << 31;   /* ˮƽͬ������:�ߵ�ƽ��Ч */
    }
    else
    {
        tempreg |= 0 << 31;             /* ˮƽͬ������:�͵�ƽ��Ч */
    }
    
    LTDC->GCR = tempreg;                /* ����ȫ�ֿ��ƼĴ��� */
    tempreg = (lcdltdc.vsw - 1) << 0;   /* ��ֱͬ���߶�-1 */
    tempreg |= (lcdltdc.hsw - 1) << 16; /* ˮƽͬ������-1 */
    LTDC->SSCR = tempreg;               /* ����ͬ����С���üĴ��� */

    tempreg = (lcdltdc.vsw + lcdltdc.vbp - 1) << 0;     /* �ۼӴ�ֱ���ظ߶�=��ֱͬ���߶�+��ֱ����-1 */
    tempreg |= (lcdltdc.hsw + lcdltdc.hbp - 1) << 16;   /* �ۼ�ˮƽ���ؿ���=ˮƽͬ������+ˮƽ����-1 */
    LTDC->BPCR = tempreg;                               /* ���ú������üĴ��� */

    tempreg = (lcdltdc.vsw + lcdltdc.vbp + lcdltdc.pheight - 1) << 0;    /* �ۼ���Ч�߶�=��ֱͬ���߶�+��ֱ����+��ֱ�ֱ���-1 */
    tempreg |= (lcdltdc.hsw + lcdltdc.hbp + lcdltdc.pwidth - 1) << 16;   /* �ۼ���Ч����=ˮƽͬ������+ˮƽ����+ˮƽ�ֱ���-1 */
    LTDC->AWCR = tempreg;                                                /* ������Ч�������üĴ��� */

    tempreg = (lcdltdc.vsw + lcdltdc.vbp + lcdltdc.pheight + lcdltdc.vfp - 1) << 0;    /* �ܸ߶�=��ֱͬ���߶�+��ֱ����+��ֱ�ֱ���+��ֱǰ��-1 */
    tempreg |= (lcdltdc.hsw + lcdltdc.hbp + lcdltdc.pwidth + lcdltdc.hfp - 1) << 16;   /* �ܿ���=ˮƽͬ������+ˮƽ����+ˮƽ�ֱ���+ˮƽǰ��-1 */
    LTDC->TWCR = tempreg;   /* �����ܿ������üĴ��� */

    LTDC->BCCR = LTDC_BACKLAYERCOLOR;   /* ���ñ�������ɫ�Ĵ���(RGB888��ʽ) */
    ltdc_switch(1);         /* ����LTDC */

#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888
    g_ltdc_framebuf[0] = (uint32_t *)&ltdc_lcd_framebuf;
    lcdltdc.pixsize = 4;    /* ÿ������ռ4���ֽ� */
#elif LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888
    g_ltdc_framebuf[0] = (uint32_t *)&ltdc_lcd_framebuf;
    lcdltdc.pixsize = 3;    /* ÿ������ռ3���ֽ� */
#else
    g_ltdc_framebuf[0] = (uint32_t *)&ltdc_lcd_framebuf;
    //g_ltdc_framebuf[1] = (uint32_t*)&ltdc_lcd_framebuf1;
    lcdltdc.pixsize = 2;    /* ÿ������ռ2���ֽ� */
#endif

    /* ������ */
    ltdc_layer_parameter_config(0, (uint32_t)g_ltdc_framebuf[0], LTDC_PIXFORMAT, 255, 0, 6, 7, 0X000000);   /* ��������� */
    ltdc_layer_window_config(0, 0, 0, lcdltdc.pwidth, lcdltdc.pheight);                                     /* �㴰������,��LCD�������ϵΪ��׼,��Ҫ����޸�! */

    //ltdc_layer_parameter_config(1, (uint32_t)g_ltdc_framebuf[1], LTDC_PIXFORMAT, 127, 0, 6, 7, 0X000000); /* ��������� */
    //ltdc_layer_window_config(1, 0, 0, lcdltdc.pwidth, lcdltdc.pheight);                                   /* �㴰������,��LCD�������ϵΪ��׼,��Ҫ����޸�! */

    lcddev.width = lcdltdc.pwidth;      /* ����lcddev�Ŀ��Ȳ��� */
    lcddev.height = lcdltdc.pheight;    /* ����lcddev�ĸ߶Ȳ��� */
    lcdltdc.pixformat = LTDC_PIXFORMAT; /* ��ɫ���ظ�ʽ */
    //ltdc_display_dir(1);              /* Ĭ�ϙM������lcd_init������������ */
    ltdc_select_layer(0);               /* ѡ���1�� */

    if (lcdid != 0X5571)                /* 5571�Ѿ���λ���� */
    {
        /* LTDC LCD��λ */
        LTDC_RST(1);
        delay_ms(10);
        LTDC_RST(0);
        delay_ms(50);
        LTDC_RST(1); 
        delay_ms(200); 
    }
    
    LTDC_BL(1);                         /* �������� */
    ltdc_clear(0XFFFFFFFF);             /* ���� */
}










