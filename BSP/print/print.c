
#include "print.h"

/* ========== 全局定义 ========== */
#define MY_PRINTF_BUF_SIZE   128        // 缓冲区大小，可根据需要调整
static osMutexId_t printfMutex = NULL;
static char printfBuf[MY_PRINTF_BUF_SIZE];  // 静态缓冲区，避免占用任务栈

/* ========== 初始化函数（在 osKernelInitialize 之后、启动调度器之前调用） ========== */
void my_printf_init(void)
{
    if (printfMutex == NULL) {
        printfMutex = osMutexNew(NULL);   // 动态创建互斥量
    }
}

/* ========== my_printf 实现 ========== */
int my_printf(const char *fmt, ...)
{
    va_list args;
    int len;

  
    if (osMutexAcquire(printfMutex, osWaitForever) == osOK) {
        va_start(args, fmt);
        len = vsnprintf(printfBuf, MY_PRINTF_BUF_SIZE, fmt, args);
        va_end(args);
        if (len <= 0) {
            return len;   
        }
        uint16_t send_len = (len >= MY_PRINTF_BUF_SIZE) ? (MY_PRINTF_BUF_SIZE - 1) : (uint16_t)len;
        HAL_UART_Transmit(&huart1, (uint8_t *)printfBuf, send_len, 0xFFFF);
        osMutexRelease(printfMutex);
    }

    return len;                            // 与标准 printf 返回语义一致
}