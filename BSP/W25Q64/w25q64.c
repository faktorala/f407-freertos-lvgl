#include "w25q64.h"

/*==============================================================================
 *  W25Q64 驱动实现（STM32F407 + HAL + SPI2）
 *
 *  说明：
 *    1) 片选 CS 是普通 GPIO（PC1），CubeMX 里配的是软件 NSS，所以片选完全由
 *       本驱动用 W25Q64_CS_LOW()/HIGH() 控制。
 *    2) 收发全部用 HAL 的阻塞接口。SPI2 是全双工模式，HAL_SPI_Receive() 内部
 *       会调 HAL_SPI_TransmitReceive()，一边产生时钟一边读 MISO，正好满足
 *       Flash 读数据的要求（MOSI 上发什么无所谓）。
 *    3) 一次 HAL 调用的长度参数是 uint16_t，所以长数据在这里按块分包，
 *       分包期间 CS 保持为低，Flash 会持续输出数据。
 *============================================================================*/

/* ------------------------------ 指令表 ------------------------------------ */
#define W25Q64_CMD_WRITE_ENABLE     0x06U
#define W25Q64_CMD_WRITE_DISABLE    0x04U
#define W25Q64_CMD_READ_SR1         0x05U
#define W25Q64_CMD_READ_SR2         0x35U
#define W25Q64_CMD_READ_SR3         0x15U
#define W25Q64_CMD_WRITE_SR         0x01U
#define W25Q64_CMD_READ_DATA        0x03U
#define W25Q64_CMD_FAST_READ        0x0BU
#define W25Q64_CMD_PAGE_PROGRAM     0x02U
#define W25Q64_CMD_SECTOR_ERASE     0x20U
#define W25Q64_CMD_BLOCK32_ERASE    0x52U
#define W25Q64_CMD_BLOCK64_ERASE    0xD8U
#define W25Q64_CMD_CHIP_ERASE       0xC7U
#define W25Q64_CMD_JEDEC_ID         0x9FU
#define W25Q64_CMD_DEVICE_ID        0x90U
#define W25Q64_CMD_UNIQUE_ID        0x4BU
#define W25Q64_CMD_POWER_DOWN       0xB9U
#define W25Q64_CMD_RELEASE_PD       0xABU

/* ------------------------------ 超时/分包 -------------------------------- */
#define W25Q64_SPI_TIMEOUT_MS       1000U       /* 单次 SPI 收发超时        */
#define W25Q64_BUSY_TIMEOUT_MS      5000U       /* 等待空闲默认超时         */
#define W25Q64_ERASE_SECTOR_MS      2000U
#define W25Q64_ERASE_BLOCK32_MS     5000U
#define W25Q64_ERASE_BLOCK64_MS     10000U
#define W25Q64_ERASE_CHIP_MS        120000UL
#define W25Q64_READ_CHUNK           4096U       /* 单次 HAL 调用的最大字节数 */

/* ------------------------------ 片选控制 -------------------------------- */
#define W25Q64_CS_LOW()     HAL_GPIO_WritePin(W25Q64_CS_PORT, W25Q64_CS_PIN, GPIO_PIN_RESET)
#define W25Q64_CS_HIGH()    HAL_GPIO_WritePin(W25Q64_CS_PORT, W25Q64_CS_PIN, GPIO_PIN_SET)

/* ------------------------------ 底层收发 -------------------------------- */

static uint8_t W25Q64_SpiTx(const uint8_t *buf, uint16_t len)
{
    if (HAL_SPI_Transmit(&hspi2, (uint8_t *)buf, len, W25Q64_SPI_TIMEOUT_MS) != HAL_OK)
    {
        return W25Q64_ERR_SPI;
    }
    return W25Q64_OK;
}

static uint8_t W25Q64_SpiRx(uint8_t *buf, uint16_t len)
{
    if (HAL_SPI_Receive(&hspi2, buf, len, W25Q64_SPI_TIMEOUT_MS) != HAL_OK)
    {
        return W25Q64_ERR_SPI;
    }
    return W25Q64_OK;
}

static uint8_t W25Q64_SendCmd(uint8_t cmd)
{
    return W25Q64_SpiTx(&cmd, 1U);
}

static uint8_t W25Q64_SendAddr(uint32_t addr)
{
    uint8_t b[3];

    b[0] = (uint8_t)((addr >> 16) & 0xFFU);
    b[1] = (uint8_t)((addr >> 8) & 0xFFU);
    b[2] = (uint8_t)(addr & 0xFFU);

    return W25Q64_SpiTx(b, 3U);
}

/* ------------------------------ 状态 / 忙等待 ---------------------------- */

uint8_t W25Q64_ReadSR(uint8_t index)
{
    uint8_t cmd;
    uint8_t val = 0xFFU;

    switch (index)
    {
        case 1U: cmd = W25Q64_CMD_READ_SR1; break;
        case 2U: cmd = W25Q64_CMD_READ_SR2; break;
        case 3U: cmd = W25Q64_CMD_READ_SR3; break;
        default: return 0xFFU;
    }

    W25Q64_CS_LOW();
    if (W25Q64_SendCmd(cmd) == W25Q64_OK)
    {
        (void)W25Q64_SpiRx(&val, 1U);
    }
    W25Q64_CS_HIGH();

    return val;
}

uint8_t W25Q64_IsBusy(void)
{
    return ((W25Q64_ReadSR(1U) & W25Q64_SR1_BUSY) != 0U) ? 1U : 0U;
}

uint8_t W25Q64_WaitReady(uint32_t timeout_ms)
{
    uint32_t start;

    if (timeout_ms == 0U)
    {
        timeout_ms = W25Q64_BUSY_TIMEOUT_MS;
    }

    start = HAL_GetTick();

    for (;;)
    {
        if ((W25Q64_ReadSR(1U) & W25Q64_SR1_BUSY) == 0U)
        {
            return W25Q64_OK;
        }
        if ((HAL_GetTick() - start) >= timeout_ms)
        {
            return W25Q64_ERR_TIMEOUT;
        }
    }
}

/* ------------------------------ 写使能 ---------------------------------- */

void W25Q64_WriteEnable(void)
{
    W25Q64_CS_LOW();
    (void)W25Q64_SendCmd(W25Q64_CMD_WRITE_ENABLE);
    W25Q64_CS_HIGH();
}

void W25Q64_WriteDisable(void)
{
    W25Q64_CS_LOW();
    (void)W25Q64_SendCmd(W25Q64_CMD_WRITE_DISABLE);
    W25Q64_CS_HIGH();
}

/* ------------------------------ 识别 ------------------------------------ */

uint32_t W25Q64_ReadJedecID(void)
{
    uint8_t b[3] = {0U, 0U, 0U};

    W25Q64_CS_LOW();
    if (W25Q64_SendCmd(W25Q64_CMD_JEDEC_ID) == W25Q64_OK)
    {
        (void)W25Q64_SpiRx(b, 3U);
    }
    W25Q64_CS_HIGH();

    return ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[2];
}

uint16_t W25Q64_ReadDeviceID(void)
{
    uint8_t b[2] = {0U, 0U};
    uint8_t dummy[3] = {0xFFU, 0xFFU, 0xFFU};

    W25Q64_CS_LOW();
    if (W25Q64_SendCmd(W25Q64_CMD_DEVICE_ID) == W25Q64_OK)
    {
        if (W25Q64_SpiTx(dummy, 3U) == W25Q64_OK)
        {
            (void)W25Q64_SpiRx(b, 2U);
        }
    }
    W25Q64_CS_HIGH();

    return (uint16_t)(((uint16_t)b[0] << 8) | (uint16_t)b[1]);
}

uint32_t W25Q64_GetCapacity(void)
{
    uint32_t id = W25Q64_ReadJedecID();
    uint8_t  cap = (uint8_t)(id & 0xFFU);

    if ((id == 0x000000UL) || (id == 0xFFFFFFUL))
    {
        return 0U;                      /* 读不到芯片 */
    }
    return (1UL << cap);                /* 容量编码：0x17 -> 2^23 = 8 MB */
}

uint8_t W25Q64_ReadUniqueID(uint8_t id[8])
{
    uint8_t dummy[4] = {0xFFU, 0xFFU, 0xFFU, 0xFFU};
    uint8_t ret;

    if (id == NULL)
    {
        return W25Q64_ERR_PARAM;
    }

    W25Q64_CS_LOW();
    ret = W25Q64_SendCmd(W25Q64_CMD_UNIQUE_ID);
    if (ret == W25Q64_OK)
    {
        ret = W25Q64_SpiTx(dummy, 4U);
    }
    if (ret == W25Q64_OK)
    {
        ret = W25Q64_SpiRx(id, 8U);
    }
    W25Q64_CS_HIGH();

    return ret;
}

/* ------------------------------ 擦除 ------------------------------------ */

static uint8_t W25Q64_EraseCommon(uint8_t cmd, uint32_t addr, uint32_t timeout_ms)
{
    uint8_t ret;

    W25Q64_WriteEnable();

    W25Q64_CS_LOW();
    ret = W25Q64_SendCmd(cmd);
    if (ret == W25Q64_OK)
    {
        ret = W25Q64_SendAddr(addr);
    }
    W25Q64_CS_HIGH();

    if (ret != W25Q64_OK)
    {
        return ret;
    }

    return W25Q64_WaitReady(timeout_ms);
}

uint8_t W25Q64_EraseSector(uint32_t addr)
{
    if (addr >= W25Q64_FLASH_SIZE)
    {
        return W25Q64_ERR_ADDR;
    }
    return W25Q64_EraseCommon(W25Q64_CMD_SECTOR_ERASE,
                              addr & ~(W25Q64_SECTOR_SIZE - 1UL),
                              W25Q64_ERASE_SECTOR_MS);
}

uint8_t W25Q64_EraseBlock32(uint32_t addr)
{
    if (addr >= W25Q64_FLASH_SIZE)
    {
        return W25Q64_ERR_ADDR;
    }
    return W25Q64_EraseCommon(W25Q64_CMD_BLOCK32_ERASE,
                              addr & ~(W25Q64_BLOCK32_SIZE - 1UL),
                              W25Q64_ERASE_BLOCK32_MS);
}

uint8_t W25Q64_EraseBlock64(uint32_t addr)
{
    if (addr >= W25Q64_FLASH_SIZE)
    {
        return W25Q64_ERR_ADDR;
    }
    return W25Q64_EraseCommon(W25Q64_CMD_BLOCK64_ERASE,
                              addr & ~(W25Q64_BLOCK64_SIZE - 1UL),
                              W25Q64_ERASE_BLOCK64_MS);
}

uint8_t W25Q64_EraseChip(void)
{
    uint8_t ret;

    W25Q64_WriteEnable();

    W25Q64_CS_LOW();
    ret = W25Q64_SendCmd(W25Q64_CMD_CHIP_ERASE);
    W25Q64_CS_HIGH();

    if (ret != W25Q64_OK)
    {
        return ret;
    }

    return W25Q64_WaitReady(W25Q64_ERASE_CHIP_MS);
}

/* ------------------------------ 读 -------------------------------------- */

static uint8_t W25Q64_ReadCommon(uint8_t cmd, uint32_t addr,
                                 uint8_t *buf, uint32_t len, uint8_t need_dummy)
{
    uint8_t  ret;
    uint8_t  dummy = 0xFFU;
    uint16_t chunk;

    if ((buf == NULL) || (len == 0U))
    {
        return W25Q64_ERR_PARAM;
    }
    if ((addr >= W25Q64_FLASH_SIZE) || (len > (W25Q64_FLASH_SIZE - addr)))
    {
        return W25Q64_ERR_ADDR;         /* 这样写避免 addr + len 的 32 位溢出 */
    }

    W25Q64_CS_LOW();

    ret = W25Q64_SendCmd(cmd);
    if (ret == W25Q64_OK)
    {
        ret = W25Q64_SendAddr(addr);
    }
    if ((ret == W25Q64_OK) && (need_dummy != 0U))
    {
        ret = W25Q64_SpiTx(&dummy, 1U);
    }

    /* 分包连续读：CS 保持低，Flash 会一直往外吐数据 */
    while ((ret == W25Q64_OK) && (len > 0U))
    {
        chunk = (len > W25Q64_READ_CHUNK) ? (uint16_t)W25Q64_READ_CHUNK : (uint16_t)len;
        ret = W25Q64_SpiRx(buf, chunk);
        buf += chunk;
        len -= chunk;
    }

    W25Q64_CS_HIGH();

    return ret;
}

uint8_t W25Q64_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    return W25Q64_ReadCommon(W25Q64_CMD_READ_DATA, addr, buf, len, 0U);
}

uint8_t W25Q64_ReadFast(uint32_t addr, uint8_t *buf, uint32_t len)
{
    return W25Q64_ReadCommon(W25Q64_CMD_FAST_READ, addr, buf, len, 1U);
}

/* ------------------------------ 写 -------------------------------------- */

uint8_t W25Q64_PageProgram(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    uint8_t ret;
    uint32_t page_offset;

    if ((buf == NULL) || (len == 0U))
    {
        return W25Q64_ERR_PARAM;
    }
    if ((addr >= W25Q64_FLASH_SIZE) || (len > (W25Q64_FLASH_SIZE - addr)))
    {
        return W25Q64_ERR_ADDR;
    }

    page_offset = addr % W25Q64_PAGE_SIZE;
    if ((page_offset + len) > W25Q64_PAGE_SIZE)
    {
        return W25Q64_ERR_ADDR;         /* 跨页，请改用 W25Q64_Write */
    }

    W25Q64_WriteEnable();

    W25Q64_CS_LOW();
    ret = W25Q64_SendCmd(W25Q64_CMD_PAGE_PROGRAM);
    if (ret == W25Q64_OK)
    {
        ret = W25Q64_SendAddr(addr);
    }
    if (ret == W25Q64_OK)
    {
        ret = W25Q64_SpiTx(buf, len);
    }
    W25Q64_CS_HIGH();

    if (ret != W25Q64_OK)
    {
        return ret;
    }

    return W25Q64_WaitReady(W25Q64_BUSY_TIMEOUT_MS);
}

uint8_t W25Q64_Write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint8_t  ret;
    uint32_t page_offset;
    uint16_t chunk;
    const uint8_t *p = buf;

    if ((buf == NULL) || (len == 0U))
    {
        return W25Q64_ERR_PARAM;
    }
    if ((addr >= W25Q64_FLASH_SIZE) || (len > (W25Q64_FLASH_SIZE - addr)))
    {
        return W25Q64_ERR_ADDR;
    }

    while (len > 0U)
    {
        page_offset = addr % W25Q64_PAGE_SIZE;
        chunk = (uint16_t)(W25Q64_PAGE_SIZE - page_offset);
        if (chunk > len)
        {
            chunk = (uint16_t)len;
        }

        ret = W25Q64_PageProgram(addr, p, chunk);
        if (ret != W25Q64_OK)
        {
            return ret;
        }

        addr += chunk;
        p    += chunk;
        len  -= chunk;
    }

    return W25Q64_OK;
}

/* ------------------------------ 低功耗 ---------------------------------- */

void W25Q64_PowerDown(void)
{
    W25Q64_CS_LOW();
    (void)W25Q64_SendCmd(W25Q64_CMD_POWER_DOWN);
    W25Q64_CS_HIGH();
}

void W25Q64_WakeUp(void)
{
    uint8_t dummy = 0xFFU;

    W25Q64_CS_LOW();
    (void)W25Q64_SendCmd(W25Q64_CMD_RELEASE_PD);
    (void)W25Q64_SpiTx(&dummy, 1U);     /* 唤醒后需要 tRES1 时间，时钟推动即可 */
    W25Q64_CS_HIGH();
}

/* ------------------------------ 初始化 ---------------------------------- */

uint8_t W25Q64_Init(void)
{
    uint32_t id;

    /* 上电/复位后先把片选拉高，让芯片处于空闲状态 */
    W25Q64_CS_HIGH();

    /* 芯片可能停留在掉电模式，先唤醒；tRES1 = 3us，这里多等 1ms 更稳 */
    W25Q64_WakeUp();
    HAL_Delay(1U);

    id = W25Q64_ReadJedecID();

    /* 全 0 或全 1 都说明 SPI 没通（MISO 一直被拉低或一直悬空上拉） */
    if ((id == 0x000000UL) || (id == 0xFFFFFFUL))
    {
        return W25Q64_ERR_ID;
    }

    return W25Q64_OK;
}
