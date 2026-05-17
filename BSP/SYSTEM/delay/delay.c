/**
 ****************************************************************************************************					 
 * @file        delay.c
 * @version     V1.1
 * @brief       ʹ��SysTick����ͨ����ģʽ���ӳٽ��й���(֧��ucosii)
 *              �ṩdelay_init��ʼ��������delay_us��delay_ms����ʱ���� 
 ****************************************************************************************************
 *
 * V1.1
 * �޸�SYS_SUPPORT_OS���ִ���, Ĭ�Ͻ�֧��UCOSII 2.93.01�汾, ����OS��ο�ʵ��
 * �޸�delay_initʹ��ϵͳʱ��
 * �޸�delay_usʹ��ʱ��ժȡ����ʱ, ����OS
 * �޸�delay_msֱ��ʹ��delay_us��ʱʵ��.
 *
 ****************************************************************************************************
 */ 

#include "sys.h"
#include "delay.h"
#include "stm32h7xx_hal.h"


static uint32_t g_fac_us = 0;       /* us��ʱ������ */

/* ���SYS_SUPPORT_OS������,˵��Ҫ֧��OS��(������UCOS) */
#if SYS_SUPPORT_OS

/* ���ӹ���ͷ�ļ� ( ucos��Ҫ�õ�) */
#include "os.h"

/* ����g_fac_ms����, ��ʾms��ʱ�ı�����, ����ÿ�����ĵ�ms��, (����ʹ��os��ʱ��,��Ҫ�õ�) */
static uint16_t g_fac_ms = 0;

/*
 *  ��delay_us/delay_ms��Ҫ֧��OS��ʱ����Ҫ������OS��صĺ궨��ͺ�����֧��
 *  ������3���궨��:
 *      delay_osrunning    :���ڱ�ʾOS��ǰ�Ƿ���������,�Ծ����Ƿ����ʹ����غ���
 *      delay_ostickspersec:���ڱ�ʾOS�趨��ʱ�ӽ���,delay_init�����������������ʼ��systick
 *      delay_osintnesting :���ڱ�ʾOS�ж�Ƕ�׼���,��Ϊ�ж����治���Ե���,delay_msʹ�øò����������������
 *  Ȼ����3������:
 *      delay_osschedlock  :��������OS�������,��ֹ����
 *      delay_osschedunlock:���ڽ���OS�������,���¿�������
 *      delay_ostimedly    :����OS��ʱ,���������������.
 *
 *  �����̽���UCOSII��֧��,����OS,�����вο���ֲ
 */

/* ֧��UCOSII */
#define delay_osrunning     OSRunning           /* OS�Ƿ����б��,0,������;1,������ */
#define delay_ostickspersec OS_TICKS_PER_SEC    /* OSʱ�ӽ���,��ÿ����ȴ��� */
#define delay_osintnesting  OSIntNesting        /* �ж�Ƕ�׼���,���ж�Ƕ�״��� */


/**
 * @brief     us����ʱʱ,�ر��������(��ֹ���us���ӳ�)
 * @param     ��
 * @retval    ��
 */
void delay_osschedlock(void)
{
    OSSchedLock();                      /* UCOSII�ķ�ʽ,��ֹ���ȣ���ֹ���us��ʱ */
}

/**
 * @brief     us����ʱʱ,�ָ��������
 * @param     ��
 * @retval    ��
 */
void delay_osschedunlock(void)
{
    OSSchedUnlock();                    /* UCOSII�ķ�ʽ,�ָ����� */
}

/**
 * @brief     os��ʱ,���Խ����������
 * @param     ticks: ��ʱ�Ľ�����
 * @retval    ��
 */
void delay_ostimedly(uint32_t ticks)
{
    OSTimeDly(ticks);                   /* UCOSII��ʱ */
}

/**
 * @brief     systick�жϷ�����,ʹ��OSʱ�õ�
 * @param     ��
 * @retval    ��
 */
void SysTick_Handler(void)
{
    if (delay_osrunning == OS_TRUE)     /* OS��ʼ����,��ִ�������ĵ��ȴ��� */
    {
        OS_CPU_SysTickHandler();        /* ���� uC/OS-II �� SysTick �жϷ����� */
    }
}

#endif

/**
 * @brief     ��ʼ���ӳٺ���
 * @param     sysclk: ϵͳʱ��Ƶ��, ��CPUƵ��(HCLK), ��λ Mhz
 * @retval    ��
 */
void delay_init(uint16_t sysclk)
{
    uint32_t reload;
    uint32_t hclk_hz;
#if SYS_SUPPORT_OS                             /* �����Ҫ֧��OS. */
    uint32_t os_reload;
#endif
    if (sysclk == 0U)
    {
        return;
    }

    hclk_hz = HAL_RCC_GetHCLKFreq();
    if (hclk_hz == 0U)
    {
        hclk_hz = (uint32_t)sysclk * 1000000U; /* fallback when HAL clock query is unavailable */
    }

    SysTick->CTRL |= (1 << 2);                 /* SYSTICKʹ��ϵͳʱ��Դ,Ƶ��ΪHCLK */
    g_fac_us = hclk_hz / 1000000U;             /* 1us ticks derived from real HCLK */
    if (g_fac_us == 0U)
    {
        g_fac_us = 1U;
    }
    SysTick->CTRL |= 1 << 0;                   /* ʹ��SysTick */
    reload = hclk_hz / 1000U;                  /* 1ms tick */
    SysTick->LOAD = reload - 1U;               /* SysTick N-cycle period requires LOAD = N-1 */
    SysTick->CTRL |= 1 << 1;                   /* Enable SysTick interrupt for HAL_IncTick */
#if SYS_SUPPORT_OS                             /* �����Ҫ֧��OS */
    os_reload = sysclk;                        /* ÿ���ӵļ������� ��λΪM */
    os_reload *= 1000000 / delay_ostickspersec;/* ����delay_ostickspersec�趨���ʱ��
                                                 * reloadΪ24λ�Ĵ���,���ֵ:16777216
                                                 */
    g_fac_ms = 1000 / delay_ostickspersec;     /* ����OS������ʱ�����ٵ�λ */
    SysTick->CTRL |= 1 << 1;                   /* ����SYSTICK�ж� */
    SysTick->LOAD = os_reload;                 /* ÿ1/delay_ostickspersec���ж�һ�� */
#endif
}

/**
 * @brief     ��ʱnus
 * @note      �����Ƿ�ʹ��OS, ������ʱ��ժȡ������us��ʱ
 * @param     nus: Ҫ��ʱ��us��
 * @note      nusȡֵ��Χ: 0 ~ (2^32 / g_fac_us) (g_fac_usһ�����ϵͳ��Ƶ, �����������)
 * @retval    ��
 */
void delay_us(uint32_t nus)
{
    uint32_t ticks;
    uint32_t told, tnow, tcnt = 0;
    uint32_t reload = SysTick->LOAD;        /* LOAD��ֵ */
    ticks = nus * g_fac_us;                 /* ��Ҫ�Ľ����� */
    
#if SYS_SUPPORT_OS                          /* �����Ҫ֧��OS */
    delay_osschedlock();                    /* ���� OS ���������������ֹ���us��ʱ */
#endif

    told = SysTick->VAL;                    /* �ս���ʱ�ļ�����ֵ */
  
    while (1)
    {
        tnow = SysTick->VAL;
      
        if (tnow != told)
        {
            if (tnow < told)
            {
                tcnt += told - tnow;        /* ����ע��һ��SYSTICK��һ���ݼ��ļ������Ϳ����� */
            }
            else
            {
                tcnt += reload - tnow + told;
            }
            
            told = tnow;
            
            if (tcnt >= ticks) 
            {
                break;                      /* ʱ�䳬��/����Ҫ�ӳٵ�ʱ��,���˳� */
            }
        }
    }

#if SYS_SUPPORT_OS                          /* �����Ҫ֧��OS */
    delay_osschedunlock();                  /* �ָ� OS ����������� */
#endif 
}

/**
 * @brief     ��ʱnms
 * @param     nms: Ҫ��ʱ��ms�� (0< nms <= (2^32 / g_fac_us / 1000))(g_fac_usһ�����ϵͳ��Ƶ, �����������)
 * @retval    ��
 */
void delay_ms(uint16_t nms)
{    
#if SYS_SUPPORT_OS  /* �����Ҫ֧��OS, ������������os��ʱ���ͷ�CPU */
    if (delay_osrunning && delay_osintnesting == 0)     /* ���OS�Ѿ�������,���Ҳ������ж�����(�ж����治���������) */
    {
        if (nms >= g_fac_ms)                            /* ��ʱ��ʱ�����OS������ʱ������ */
        {
            delay_ostimedly(nms / g_fac_ms);            /* OS��ʱ */
        }

        nms %= g_fac_ms;                                /* OS�Ѿ��޷��ṩ��ôС����ʱ��,������ͨ��ʽ��ʱ */
    }
#endif

    delay_us((uint32_t)(nms * 1000));                   /* ��ͨ��ʽ��ʱ */
}







