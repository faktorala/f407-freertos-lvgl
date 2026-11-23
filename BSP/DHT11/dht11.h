#ifndef __DHT11_H
#define __DHT11_H

#include "main.h"
#include "delay.h"

/*==============================================================================
 *  DHT11 温湿度传感器驱动（STM32F407 + HAL + 单总线软件时序）
 *
 *  接线（与 f407watch.ioc、Core/Inc/main.h 保持一致）：
 *      DHT11 DATA  ---  PE10
 *      VCC 3.3V、GND
 *
 *  DATA 是开漏单总线，必须接 4.7k~10k 上拉电阻（多数成品模块已自带）。
 *  驱动内部把 PE10 配成“输入 + 内部上拉”，最小系统上没有外部上拉也能勉强读，
 *  但长线或干扰环境下务必加上拉。
 *
 *  数据格式（传感器一次返回 5 字节）：
 *      raw[0] 湿度整数   raw[1] 湿度小数
 *      raw[2] 温度整数   raw[3] 温度小数
 *      raw[4] 校验和 = (raw[0]+raw[1]+raw[2]+raw[3]) & 0xFF
 *
 *  返回值约定（全模块统一）：
 *      0  = 成功 / 传感器有应答
 *      1  = 校验和错误
 *      2  = 传感器无应答（线路断开、没接上拉、上电未稳定、时序被打断）
 *============================================================================*/

/* ------------------------------ 引脚定义 ---------------------------------- */
/* 端口和引脚号直接引用 CubeMX 在 main.h 里生成的定义，避免两处不一致 */
#define DHT11_PORT          DHT11_GPIO_Port
#define DHT11_GPIO_PIN      DHT11_Pin
#define DHT11_PIN           10U             /* 位序号，必须与 DHT11_Pin 对应 */
/* 编译期自检：DHT11_PIN 与 DHT11_GPIO_PIN 不匹配时直接报错 */
typedef char dht11_pin_index_check[(DHT11_GPIO_PIN >> DHT11_PIN) == 1U ? 1 : -1];

/* 上电后 DHT11 需要大约 1s 才能输出稳定数据。
 * 若 DHT11_Init() 是在系统上电 1s 之后才调用，可以把这里改成 0 省掉等待。 */
#ifndef DHT11_POWER_ON_DELAY_MS
#define DHT11_POWER_ON_DELAY_MS    1000U
#endif

/* 单总线方向切换（函数实现，内部直接操作 MODER/PUPDR） */
void    DHT11_IO_IN(void);
void    DHT11_IO_OUT(void);

/* ------------------------------ 对外接口 ---------------------------------- */
/* 初始化：配置引脚为输入上拉，等待传感器上电稳定，并做一次通信测试。
 * 返回 0 = 传感器正常，非 0 = 无应答。 */
uint8_t DHT11_Init(void);

/* 读一次完整原始数据（5 字节），返回 0 表示数据有效。 */
uint8_t DHT11_Read_Raw(uint8_t raw[5]);

/* 常用接口：只取整数部分。
 * 返回 0 成功，非 0 失败（失败时 *temp / *humi 不被修改）。 */
uint8_t DHT11_Read_Data(uint8_t *temp, uint8_t *humi);

/* 带小数的接口：值 = 整数 + 小数 * 0.1（DHT11 小数位通常为 0）。 */
uint8_t DHT11_Read_Float(float *temp, float *humi);

/* 发送起始信号并等待从机应答，返回 0 = 应答正常。 */
uint8_t DHT11_Check(void);
void    DHT11_Rst(void);

#endif /* __DHT11_H */
