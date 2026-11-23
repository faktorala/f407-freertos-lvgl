#include "dht11.h"

/*==============================================================================
 *  DHT11 驱动（STM32F407 / HAL）
 *
 *  与原 F1 例程的差异：
 *    1) F1 用 GPIOx->CRL/CRH 配置输入输出，F4 没有这两个寄存器，
 *       这里改为直接改写 MODER / PUPDR（比调用 HAL_GPIO_Init 快得多，
 *       单总线时序要求微秒级，方向切换越快越好）。
 *    2) 数据引脚由 PA11 改为 PE10，与 f407watch.ioc 中的 DHT11 一致。
 *    3) 读取 40 bit 期间关中断，避免 FreeRTOS 调度、SysTick、LVGL 等
 *       中断把位时序拉长导致误码。
 *============================================================================*/

/* PE10 在 MODER / PUPDR 中占 bit[11:10] */
#define DHT11_CFG_POS       (DHT11_PIN * 2U)
#define DHT11_CFG_MSK       (3UL << DHT11_CFG_POS)

/* ---------------------------- 底层引脚操作 -------------------------------- */

static void DHT11_SetPin(uint8_t state)
{
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_GPIO_PIN,
                      (state != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static uint8_t DHT11_ReadPin(void)
{
    return ((DHT11_PORT->IDR & DHT11_GPIO_PIN) != 0U) ? 1U : 0U;
}

/* 推挽输出：MODER[1:0] = 01 */
void DHT11_IO_OUT(void)
{
    DHT11_PORT->MODER = (DHT11_PORT->MODER & ~DHT11_CFG_MSK) | (1UL << DHT11_CFG_POS);
}

/* 输入 + 上拉：MODER[1:0] = 00，PUPDR[1:0] = 01 */
void DHT11_IO_IN(void)
{
    DHT11_PORT->MODER = (DHT11_PORT->MODER & ~DHT11_CFG_MSK);
    DHT11_PORT->PUPDR = (DHT11_PORT->PUPDR & ~DHT11_CFG_MSK) | (1UL << DHT11_CFG_POS);
}

/* ---------------------------- 单总线时序 ---------------------------------- */

/* 主机起始信号：拉低 >= 18ms，再拉高 20~40us 后释放总线 */
void DHT11_Rst(void)
{
    DHT11_IO_OUT();
    DHT11_SetPin(0U);
    delay_ms(20);
    DHT11_SetPin(1U);
    delay_us(30);
}

/* 等待从机应答：约 80us 低 + 80us 高，返回 0 表示应答正常 */
uint8_t DHT11_Check(void)
{
    uint8_t retry = 0U;

    DHT11_IO_IN();

    while ((DHT11_ReadPin() == 1U) && (retry < 100U))   /* 等总线被从机拉低 */
    {
        retry++;
        delay_us(1);
    }
    if (retry >= 100U)
    {
        return 1U;
    }

    retry = 0U;
    while ((DHT11_ReadPin() == 0U) && (retry < 100U))   /* 等 80us 低电平结束 */
    {
        retry++;
        delay_us(1);
    }
    if (retry >= 100U)
    {
        return 1U;
    }

    return 0U;
}

/* 读 1 bit：每位都是 50us 低电平 + 高电平（26~28us 表示 0，约 70us 表示 1） */
static uint8_t DHT11_Read_Bit(void)
{
    uint8_t retry = 0U;

    while ((DHT11_ReadPin() == 1U) && (retry < 100U))   /* 等上一位的高电平结束 */
    {
        retry++;
        delay_us(1);
    }

    retry = 0U;
    while ((DHT11_ReadPin() == 0U) && (retry < 100U))   /* 等本位的 50us 低电平结束 */
    {
        retry++;
        delay_us(1);
    }

    delay_us(40);                                       /* 40us 后仍为高 => 数据 1 */
    return DHT11_ReadPin();
}

/* 读 1 字节，高位在前 */
static uint8_t DHT11_Read_Byte(void)
{
    uint8_t i;
    uint8_t dat = 0U;

    for (i = 0U; i < 8U; i++)
    {
        dat <<= 1;
        dat |= DHT11_Read_Bit();
    }
    return dat;
}

/* ---------------------------- 对外接口 ------------------------------------ */

uint8_t DHT11_Read_Raw(uint8_t raw[5])
{
    uint8_t  i;
    uint8_t  acked = 0U;
    uint32_t primask;

    for (i = 0U; i < 5U; i++)
    {
        raw[i] = 0U;
    }

    /* 40 bit 传输约 4ms，期间关中断保证时序不被拉长 */
    primask = __get_PRIMASK();
    __disable_irq();

    DHT11_Rst();
    if (DHT11_Check() == 0U)
    {
        for (i = 0U; i < 5U; i++)
        {
            raw[i] = DHT11_Read_Byte();
        }
        acked = 1U;
    }

    __set_PRIMASK(primask);

    if (acked == 0U)
    {
        return 2U;      /* 传感器无应答 */
    }

    if (raw[4] != (uint8_t)(raw[0] + raw[1] + raw[2] + raw[3]))
    {
        return 1U;      /* 校验和错误 */
    }

    return 0U;
}

uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi)
{
    uint8_t raw[5];
    uint8_t ret = DHT11_Read_Raw(raw);

    if (ret != 0U)
    {
        return ret;
    }

    if (humi != NULL)
    {
        *humi = raw[0];
    }
    if (temp != NULL)
    {
        *temp = raw[2];
    }

    return 0U;
}

uint8_t DHT11_Read_Float(float *temp, float *humi)
{
    uint8_t raw[5];
    uint8_t ret = DHT11_Read_Raw(raw);

    if (ret != 0U)
    {
        return ret;
    }

    if (humi != NULL)
    {
        *humi = (float)raw[0] + ((float)raw[1] * 0.1f);
    }
    if (temp != NULL)
    {
        *temp = (float)raw[2] + ((float)raw[3] * 0.1f);
    }

    return 0U;
}

uint8_t DHT11_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* 先输出高电平把总线释放，再切成输入上拉 */
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_GPIO_PIN, GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = DHT11_GPIO_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);

#if (DHT11_POWER_ON_DELAY_MS > 0U)
    delay_ms(DHT11_POWER_ON_DELAY_MS);      /* 上电稳定等待 */
#endif

    /* 做一次完整通信测试，返回 0 = 传感器在位 */
    DHT11_Rst();
    return DHT11_Check();
}
