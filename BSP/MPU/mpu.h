/**
 ****************************************************************************************************
 * @file        mpu.h
 * @version     V1.0
 * @brief       MPU内存保护 驱动代码
 ****************************************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 *
 * 实验平台:    STM32H743IIT6小系统板
 *
 ****************************************************************************************************
 */

#ifndef __MPU_H
#define __MPU_H

#include "sys.h"


/* MPU的详细设置关系，请看：《STM32H7编程手册.pdf》
 * 这个文档的4.6.6节,Table 93.
 * MPU保护区域许可属性定义（拷贝自stm32h7xx_hal_cortex.h）
 * 定义MPU->RASR寄存器AP[26:24]位的设置值
 */ 
#define  MPU_REGION_NO_ACCESS       ((uint8_t)0x00U)        /* 无访问（特权&用户都不可访问） */
#define  MPU_REGION_PRIV_RW         ((uint8_t)0x01U)        /* 仅支持特权读写访问 */
#define  MPU_REGION_PRIV_RW_URO     ((uint8_t)0x02U)        /* 禁止用户写访问（特权可读写访问） */
#define  MPU_REGION_FULL_ACCESS     ((uint8_t)0x03U)        /* 全访问（特权&用户都可访问） */
#define  MPU_REGION_PRIV_RO         ((uint8_t)0x05U)        /* 仅支持特权读访问 */
#define  MPU_REGION_PRIV_RO_URO     ((uint8_t)0x06U)        /* 只读（特权&用户都不可以写） */

/******************************************************************************************/

static uint8_t mpu_convert_bytes_to_pot(uint32_t nbytes);   /* 将nbytes转换为以2为底的指数值 */

void mpu_disable(void); /* 禁止MPU */
void mpu_enable(void);  /* 使能MPU */
void mpu_memory_protection(void);   /* 设置需要保护的存储块 */
uint8_t mpu_set_protection(uint32_t baseaddr, uint32_t size, uint32_t rnum, uint8_t de, uint8_t ap, uint8_t sen, uint8_t cen, uint8_t ben);   /* 设置某个区域的MPU保护 */

#endif








