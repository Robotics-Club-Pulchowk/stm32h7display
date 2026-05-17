/**
 ****************************************************************************************************
 * @file        usart.c
 * @brief       USART1 driver with DMA TX.
 ****************************************************************************************************
 */

#include "usart.h"
#include "string.h"

UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_tx;

#define USART1_DMA_TX_BUF_SIZE        256U
#define USART1_BLOCKING_TX_BUF_SIZE   64U
#define USART1_DMA_WAIT_TIMEOUT_MS    100U

static volatile uint8_t g_usart1_tx_busy = 0U;
static uint8_t g_usart1_tx_buf[USART1_DMA_TX_BUF_SIZE];

static uint8_t usart1_tx_is_busy(void)
{
    if (g_usart1_tx_busy != 0U)
    {
        return 1U;
    }

    return ((HAL_UART_GetState(&huart1) & HAL_UART_STATE_BUSY_TX) == HAL_UART_STATE_BUSY_TX) ? 1U : 0U;
}

void usart1_init(uint32_t baud)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (baud == 0U)
    {
        return;
    }

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = baud;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }

    if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
    {
        Error_Handler();
    }

    hdma_usart1_tx.Instance = DMA1_Stream0;
    hdma_usart1_tx.Init.Request = DMA_REQUEST_USART1_TX;
    hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart1_tx.Init.Mode = DMA_NORMAL;
    hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_usart1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_LINKDMA(&huart1, hdmatx, hdma_usart1_tx);

    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 1);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

void usart1_send_char(char c)
{
    usart1_send_bytes((const uint8_t *)&c, 1U);
}

void usart1_send_string(const char *str)
{
    usart1_send_bytes((const uint8_t *)str, (uint16_t)strlen(str));
}

void usart1_send_bytes(const uint8_t *data, uint16_t len)
{
    HAL_StatusTypeDef dma_status;
    uint8_t blocking_tx_buf[USART1_BLOCKING_TX_BUF_SIZE];
    uint32_t tickstart;

    if ((data == NULL) || (len == 0U))
    {
        return;
    }

    tickstart = HAL_GetTick();
    while (usart1_tx_is_busy() != 0U)
    {
        if ((HAL_GetTick() - tickstart) >= USART1_DMA_WAIT_TIMEOUT_MS)
        {
            (void)HAL_UART_AbortTransmit(&huart1);
            g_usart1_tx_busy = 0U;
            break;
        }
    }

    if (len <= USART1_DMA_TX_BUF_SIZE)
    {
        memcpy(g_usart1_tx_buf, data, len);
        g_usart1_tx_busy = 1U;

        dma_status = HAL_UART_Transmit_DMA(&huart1, g_usart1_tx_buf, len);
        if (dma_status == HAL_OK)
        {
            return;
        }

        g_usart1_tx_busy = 0U;
    }

    while (len > 0U)
    {
        uint16_t chunk_len = len;
        if (chunk_len > USART1_BLOCKING_TX_BUF_SIZE)
        {
            chunk_len = USART1_BLOCKING_TX_BUF_SIZE;
        }

        memcpy(blocking_tx_buf, data, chunk_len);
        (void)HAL_UART_Transmit(&huart1, blocking_tx_buf, chunk_len, HAL_MAX_DELAY);

        data += chunk_len;
        len -= chunk_len;
    }
}

int usart1_recv_ready(void)
{
    return (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE_RXFNE) != RESET) ? 1 : 0;
}

char usart1_recv_char(void)
{
    uint8_t ch = 0;
    (void)HAL_UART_Receive(&huart1, &ch, 1, HAL_MAX_DELAY);
    return (char)ch;
}

int __io_putchar(int ch)
{
    uint8_t c = (uint8_t)ch;
    if (c == '\n')
    {
        const uint8_t cr = '\r';
        usart1_send_bytes(&cr, 1U);
    }
    usart1_send_bytes(&c, 1U);
    return ch;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        g_usart1_tx_busy = 0U;
    }
}
