#ifndef __W25Q64_H
#define __W25Q64_H

#include "main.h"
#include "spi.h"

/*==============================================================================
 *  W25Q64  SPI NOR Flash 驱动（8 MB / 64 Mbit）
 *
 *  硬件连接（与 f407watch.ioc、Core/Inc/main.h 一致）：
 *      SCK   ---  PB10   (SPI2_SCK)
 *      MISO  ---  PC2    (SPI2_MISO)
 *      MOSI  ---  PC3    (SPI2_MOSI)
 *      CS    ---  PC1    (W25Q64_CS，普通 GPIO 软件片选)
 *      VCC 3.3V、GND
 *
 *  SPI2 配置（CubeMX 已生成）：主机、全双工、8 bit、MSB 先出、
 *      CPOL = 0 / CPHA = 0（模式 0，W25Q64 要求）、软件 NSS、
 *      当前 21 MHz（APB1 42 MHz / 2）。
 *
 *  存储结构：
 *      容量      8 MB
 *      页        256 字节   —— 写的最小单位，一次最多写一页且不能跨页
 *      扇区      4 KB       —— 擦除的最小单位
 *      块        32 KB / 64 KB
 *
 *  ★ NOR Flash 的特性：只能把 1 写成 0，要想把 0 变回 1 必须先擦除。
 *    所以“改写”某个区域的正确流程是：擦除扇区 -> 再写入。
 *    另：写入前该区域必须是已擦除状态，否则写进去的数据是原值与新值的按位与。
 *
 *  返回值约定：0 = 成功，非 0 = 错误码（见下方 W25Q64_ERR_xxx）。
 *  所有接口都是阻塞式调用，可在任务中直接使用；若多个任务同时访问，
 *  请在外层用互斥量保护（本驱动不做总线互斥）。
 *============================================================================*/

/* ------------------------------ 引脚定义 ---------------------------------- */
/* 端口/引脚号直接引用 CubeMX 在 main.h 里生成的定义，避免两处不一致 */
#define W25Q64_CS_PORT          W25Q64_CS_GPIO_Port
#define W25Q64_CS_PIN           W25Q64_CS_Pin

/* ------------------------------ 容量参数 ---------------------------------- */
#define W25Q64_FLASH_SIZE       (8UL * 1024UL * 1024UL)          /* 8 MB        */
#define W25Q64_PAGE_SIZE        256UL                            /* 页          */
#define W25Q64_SECTOR_SIZE      4096UL                           /* 扇区        */
#define W25Q64_BLOCK32_SIZE     (32UL * 1024UL)                  /* 32 KB 块    */
#define W25Q64_BLOCK64_SIZE     (64UL * 1024UL)                  /* 64 KB 块    */
#define W25Q64_SECTOR_COUNT     (W25Q64_FLASH_SIZE / W25Q64_SECTOR_SIZE)

#define W25Q64_MANUFACTURER     0xEFU                            /* Winbond     */
#define W25Q64_JEDEC_ID         0xEF4017UL                       /* W25Q64 的 JEDEC ID */

/* ------------------------------ 返回码 ------------------------------------ */
#define W25Q64_OK               0U      /* 成功                          */
#define W25Q64_ERR_SPI          1U      /* SPI 收发失败/超时             */
#define W25Q64_ERR_ID           2U      /* 读到的 ID 非法（没接/没供电） */
#define W25Q64_ERR_TIMEOUT      3U      /* 等待芯片空闲超时              */
#define W25Q64_ERR_PARAM        4U      /* 参数非法（空指针/长度 0）     */
#define W25Q64_ERR_ADDR         5U      /* 地址越界或未对齐              */

/* ------------------------------ 状态寄存器位 ------------------------------ */
#define W25Q64_SR1_BUSY         0x01U
#define W25Q64_SR1_WEL          0x02U

/* ------------------------------ 初始化 / 识别 ----------------------------- */
/* 初始化：拉高片选、唤醒芯片、读 JEDEC ID 校验通信是否正常。
 * 必须在 MX_SPI2_Init() 之后调用。返回 0 表示芯片在位。 */
uint8_t  W25Q64_Init(void);

/* 读 JEDEC ID（3 字节），W25Q64 正常应为 0xEF4017 */
uint32_t W25Q64_ReadJedecID(void);

/* 读厂商/器件 ID（0x90 指令，返回 16 位：(厂商 << 8) | 器件） */
uint16_t W25Q64_ReadDeviceID(void);

/* 按 JEDEC ID 推算容量（字节），读不到返回 0 */
uint32_t W25Q64_GetCapacity(void);

/* 读唯一 ID（8 字节） */
uint8_t  W25Q64_ReadUniqueID(uint8_t id[8]);

/* ------------------------------ 状态 / 忙等待 ----------------------------- */
/* 读状态寄存器：index = 1 / 2 / 3，其它值返回 0xFF */
uint8_t  W25Q64_ReadSR(uint8_t index);

/* 芯片是否处于忙状态（正在写入/擦除） */
uint8_t  W25Q64_IsBusy(void);

/* 等待芯片空闲，timeout_ms 毫秒超时（传 0 使用默认值） */
uint8_t  W25Q64_WaitReady(uint32_t timeout_ms);

/* ------------------------------ 写使能 ------------------------------------ */
/* 每次写/擦除前芯片都要求先发写使能，驱动内部已自动处理，
 * 这两个函数只在需要手动控制时使用。 */
void     W25Q64_WriteEnable(void);
void     W25Q64_WriteDisable(void);

/* ------------------------------ 擦除 -------------------------------------- */
uint8_t  W25Q64_EraseSector(uint32_t addr);      /* 4 KB，addr 会向下对齐 */
uint8_t  W25Q64_EraseBlock32(uint32_t addr);     /* 32 KB                  */
uint8_t  W25Q64_EraseBlock64(uint32_t addr);     /* 64 KB                  */
uint8_t  W25Q64_EraseChip(void);                 /* 整片擦除，耗时几十秒   */

/* ------------------------------ 读 ---------------------------------------- */
/* 普通读（0x03），len 不限长度，内部自动分包 */
uint8_t  W25Q64_Read(uint32_t addr, uint8_t *buf, uint32_t len);

/* 快速读（0x0B），带一个空字节，速度略快 */
uint8_t  W25Q64_ReadFast(uint32_t addr, uint8_t *buf, uint32_t len);

/* ------------------------------ 写 ---------------------------------------- */
/* 写一页（最多 256 字节，不能跨页；跨页请用 W25Q64_Write） */
uint8_t  W25Q64_PageProgram(uint32_t addr, const uint8_t *buf, uint16_t len);

/* 任意长度写入：自动按页拆分，每页自动发写使能并等待完成。
 * 注意：调用前目标区域必须已擦除。len 无上限。 */
uint8_t  W25Q64_Write(uint32_t addr, const uint8_t *buf, uint32_t len);

/* ------------------------------ 低功耗 ------------------------------------ */
void     W25Q64_PowerDown(void);                 /* 掉电，读 ID 前需唤醒 */
void     W25Q64_WakeUp(void);

#endif /* __W25Q64_H */
