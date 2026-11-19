/*
* Copyright 2026 NXP
* NXP Proprietary. This software is owned or controlled by NXP and may only be used strictly in
* accordance with the applicable license terms. By expressly accepting such terms or by downloading, installing,
* activating and/or otherwise using the software, you are agreeing that you have read, and that you agree to
* comply with and are bound by, such license terms.  If you do not agree to be bound by the applicable license
* terms, then you may not retain, install, activate or otherwise use the software.
*/


/*********************
 *      INCLUDES
 *********************/
#include <stdio.h>
#include "lvgl.h"
#include "custom.h"
#include "lcd_init.h"      /* LCD_W / LCD_H 以及 LCD_Init 等原型 */

/*********************
 *      DEFINES
 *********************/
gg_ui_t gui;

/* LCD 阻塞式发送的失败计数，定义在 BSP/LCD/lcd_init.c：
 * 只要有一次 HAL_SPI_Transmit 超时/失败就会 +1。用它判断 SPI 到底通不通。 */
extern volatile uint32_t g_lcd_spi_err;
/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**
 * Create a demo application
 */

void custom_init(gg_ui_t *ui)
{
    /* Add your codes here */
    LCD_Init();
    LCD_Set_Light(100);

    /* ==================== 临时显示自检（测完可以删掉） ====================
     * 目的：完全不经过 LVGL / DMA，用阻塞方式直接往屏里刷纯色，用来区分
     *       "屏、SPI、接线" 和 "LVGL/DMA 刷屏" 哪一段有问题。
     *
     * 判读：
     *   上半屏红 + 下半屏蓝        -> 屏、SPI、接线都正常，问题在 LVGL/DMA 那条路
     *   只有左上角那一小块红       -> 说明自检第 1 步之后 g_lcd_spi_err != 0，SPI 发送失败
     *   完全没有颜色（全黑/全白） -> LCD_Init 或 SPI/接线有问题（看 g_lcd_spi_err）
     *
     * 先刷一个 16x16 的小方块探路：如果 SPI 不通，每像素最多等 1 ms 超时，
     * 只浪费 ~0.25 s，不会卡很久。
     * ==================================================================== */
    LCD_Fill(0, 0, 16, 16, RED);

    if (g_lcd_spi_err == 0U)
    {
        LCD_Fill(0, 0, LCD_W, LCD_H / 2, RED);          /* 上半屏红 */
        LCD_Fill(0, LCD_H / 2, LCD_W, LCD_H, BLUE);     /* 下半屏蓝 */
    }
    HAL_Delay(300);

    lv_init();
    lv_tick_set_cb(osKernelGetTickCount);
    lv_port_disp_init();
    lv_port_indev_init();

    setup_ui(&gui);
    RtcPort_Init(gui.screen.container_1_hour, gui.screen.container_1_min, gui.screen.container_1_sec);
}

