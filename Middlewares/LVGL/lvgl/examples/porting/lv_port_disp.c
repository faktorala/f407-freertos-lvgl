/**
 * @file lv_port_disp_template.c
 *
 * LVGL v9 display port for the ST7789 panel (240x280, SPI1 + DMA2 Stream3).
 *
 * 刷屏握手：仍然用 DMA，但 UI 不会再被"等不到的完成中断"卡死。
 *   1. disp_flush() 启动一笔 DMA 后立刻返回。
 *   2. SPI/DMA 完成中断 -> lcd_flush_ready_callback() -> lv_display_flush_ready()。
 *   3. 如果完成信号永远不来（SPI 报错 / BSY 不清 / DMA 错误 ...），
 *      disp_flush_wait() 在 DISP_FLUSH_TIMEOUT_MS 后放弃这一笔并 HAL_SPI_Abort()，
 *      让 LVGL 继续。
 *
 * 没有第 3 步时，LVGL 会在 lv_refr.c 的
 *      while(disp->flushing);
 * 里永久打转（它会忠实地等一个永远不会来的信号），表现就是
 * "屏幕亮着、显示首帧 00:00:00、触摸和秒针都不动"。
 *
 * 注：SPI 传输报错时 HAL 只会调 HAL_SPI_ErrorCallback()，不会再调
 *     HAL_SPI_TxCpltCallback()。那个回调在 BSP\LCD\lcd.c 里已经实现，
 *     它会同样调用 lcd_ready_cb()，所以这里不要重复定义（会链接冲突）。
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_disp.h"
#include <stdbool.h>
#include "lcd.h"
#include "spi.h"            /* hspi1 / HAL_SPI_Abort() */

/*********************
 *      DEFINES
 *********************/
/* 屏幕分辨率：与 BSP\LCD\lcd_init.h 的 USE_HORIZONTAL=0 保持一致（240 x 280）
 * 若改成横屏（USE_HORIZONTAL=2/3），这里要同步改成 280 x 240 */
#define MY_DISP_HOR_RES    240
#define MY_DISP_VER_RES    280

#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */

/* 一笔 DMA 最坏是 240x10 像素 = 4800 字节，21 MHz 下约 1.9 ms。
 * 50 ms 已经是几十倍余量；超过这个时间说明这一笔不可能完成了。 */
#define DISP_FLUSH_TIMEOUT_MS   50U

/* 纯自旋次数上限：即使 HAL_GetTick() 不涨（HAL 时基没跑），也能在这里兜底退出。
   168 MHz 下一次循环约十几条指令，2e6 次 ≈ 100 ms 量级，够用。 */
#define DISP_FLUSH_SPIN_LIMIT  2000000UL

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

static void disp_flush_wait(lv_display_t * disp);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_display_t * s_pending_disp_drv = NULL;
static lv_display_t * g_current_disp_drv = NULL;

/* 诊断计数器：异常时在调试器 Watch 窗口里看这两个值；
 * SPI 报错次数看 lcd.c 里的 g_lcd_spi_err。 */
volatile uint32_t g_lcd_flush_timeout = 0;   /* 等 DMA 完成超时、被迫放弃的次数 */
volatile uint32_t g_lcd_flush_overlap = 0;   /* 上一笔还没收尾、又来了新一笔的次数 */

/**********************
 *      MACROS
 **********************/

/**********************
 *  STATIC HELPERS
 **********************/

/* ST7789 要求高字节先发（MSB first），而 LVGL 的 RGB565 缓冲区是小端序，
   不交换的话整屏颜色会错乱（比如红色显示成蓝色）。 */
static void rgb565_swap_bytes(uint8_t * buf, uint32_t px_count)
{
    uint32_t i;
    for(i = 0; i < px_count; i++) {
        uint8_t hi = buf[2 * i];
        buf[2 * i]     = buf[2 * i + 1];
        buf[2 * i + 1] = hi;
    }
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
    lv_display_t * disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    lv_display_set_flush_cb(disp, disp_flush);

    /* 关键：给 LVGL 一个"带超时的等待回调"。
     * 不设它，lv_refr.c 里就是裸的 while(disp->flushing);，
     * DMA 一旦不发完成中断，界面就永久卡死。 */
    lv_display_set_flush_wait_cb(disp, disp_flush_wait);

    /* Example 1
     * One buffer for partial rendering*/
    // LV_ATTRIBUTE_MEM_ALIGN
    // static uint8_t buf_1_1[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];            /*A buffer for 10 rows*/
    // lv_display_set_buffers(disp, buf_1_1, NULL, sizeof(buf_1_1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Example 2
     * Two buffers for partial rendering
     * In flush_cb DMA or similar hardware should be used to update the display in the background.*/
    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t buf_2_1[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];

    LV_ATTRIBUTE_MEM_ALIGN
    static uint8_t buf_2_2[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];
    lv_display_set_buffers(disp, buf_2_1, buf_2_2, sizeof(buf_2_1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Example 3
     * Two buffers screen sized buffer for double buffering.
     * Both LV_DISPLAY_RENDER_MODE_DIRECT and LV_DISPLAY_RENDER_MODE_FULL works, see their comments*/
    // LV_ATTRIBUTE_MEM_ALIGN
    // static uint8_t buf_3_1[MY_DISP_HOR_RES * MY_DISP_VER_RES * BYTE_PER_PIXEL];

    // LV_ATTRIBUTE_MEM_ALIGN
    // static uint8_t buf_3_2[MY_DISP_HOR_RES * MY_DISP_VER_RES * BYTE_PER_PIXEL];
    // lv_display_set_buffers(disp, buf_3_1, buf_3_2, sizeof(buf_3_1), LV_DISPLAY_RENDER_MODE_DIRECT);

}

/* Called from the SPI/DMA completion ISR (and from HAL_SPI_ErrorCallback in lcd.c):
 * this transfer is finished, one way or another. */
void lcd_flush_ready_callback(void)
{
    if(g_current_disp_drv != NULL) {
        lv_display_flush_ready(g_current_disp_drv);
        g_current_disp_drv = NULL;
        s_pending_disp_drv = NULL;
    }
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    /*You code here*/
    LCD_Set_Flush_Complete_Callback(lcd_flush_ready_callback);
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_display_flush_ready()' has to be called when it's finished.*/
static void disp_flush(lv_display_t * disp_drv, const lv_area_t * area, uint8_t * px_map)
{
    /* 送屏前做 RGB565 字节序交换 */
    rgb565_swap_bytes(px_map,
                      (uint32_t)(area->x2 - area->x1 + 1) *
                      (uint32_t)(area->y2 - area->y1 + 1));

    if(!disp_flush_enabled) {
        lv_display_flush_ready(disp_drv);
        return;
    }

    /* 正常情况下进不来：LVGL 双缓冲会在下一次 flush 之前先等上一笔完成。
     * 只有上一笔超时被放弃后才可能残留状态 —— 记一笔并覆盖掉。
     * 注意：这里绝对不能"直接 return 而不通知 LVGL"，那等于让 flushing 永远停在 1。 */
    if(g_current_disp_drv != NULL) {
        g_lcd_flush_overlap++;
        g_current_disp_drv = NULL;
    }

    /* 先登记、再启动 DMA：DMA 完成中断可能在 LCD_Color_Fill() 返回之前就打进来 */
    g_current_disp_drv = disp_drv;
    s_pending_disp_drv = disp_drv;

    /* 兜底：上一次超时/中止后 SPI 可能还停在 BUSY，此时起传输必然失败。
     * 先复位回 READY，否则这一笔注定没有完成中断，又要白等 50 ms。 */
    if(hspi1.State != HAL_SPI_STATE_READY) {
        (void)HAL_SPI_Abort(&hspi1);
    }

    LCD_Color_Fill(area->x1, area->y1, area->x2, area->y2, px_map);
}

/* LVGL 在 lv_refr.c 的 wait_for_flushing() 里调用这里（代替裸的 while(disp->flushing)）。
 * 有超时；超时后把 SPI/DMA 复位回 READY，保证下一笔还能正常开始。
 *
 * 注意：这里不能只靠 HAL_GetTick() 计时 —— 如果 HAL 时基没在跑（uwTick 恒 0），
 * 那样又会变成死等。所以再加一个"纯自旋次数"的上限，双保险。 */
static void disp_flush_wait(lv_display_t * disp)
{
    uint32_t start = HAL_GetTick();
    uint32_t spins = 0;

    while(g_current_disp_drv != NULL) {
        if(((HAL_GetTick() - start) >= DISP_FLUSH_TIMEOUT_MS) ||
           (++spins >= DISP_FLUSH_SPIN_LIMIT)) {
            break;
        }
    }

    if(g_current_disp_drv != NULL) {
        /* 这一笔的完成中断不会来了：放弃它，让 LVGL 继续跑 */
        g_lcd_flush_timeout++;
        g_current_disp_drv = NULL;
        s_pending_disp_drv = NULL;
        (void)HAL_SPI_Abort(&hspi1);   /* 复位 SPI/DMA，避免下一笔继续失败 */
    }

    (void)disp;
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
