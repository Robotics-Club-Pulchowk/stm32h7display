/**
 ****************************************************************************************************
 * @file        sdram.h
 * @version     V1.0
 * @brief       SDRAM 驱动代码
 ****************************************************************************************************
 * @attention   Waiken-Smart 慧勤智远
 *
 * 实验平台:    STM32H743IIT6小系统板
 *
 ****************************************************************************************************
 */
 
#ifndef _SDRAM_H
#define _SDRAM_H

#include "sys.h"


#define BANK5_SDRAM_ADDR        ((uint32_t)(0XC0000000))            /* SDRAM开始地址 */


/******************************************************************************************/

uint8_t sdram_send_cmd(uint8_t bankx, uint8_t cmd, uint8_t refresh, uint16_t regval);   /* 向SDRAM发送命令 */
void sdram_init(void);                                                                  /* 初始化SDRAM */
void fmc_sdram_write_buffer(uint8_t *pbuf, uint32_t writeaddr, uint32_t n);             /* 写入SDRAM */
void fmc_sdram_read_buffer(uint8_t *pbuf, uint32_t readaddr, uint32_t n);               /* 读取SDRAM */

#endif




