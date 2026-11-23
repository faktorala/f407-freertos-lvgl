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
#include "data.h"
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
    lv_init();
    lv_tick_set_cb(osKernelGetTickCount);
    lv_port_disp_init();
    lv_port_indev_init();

    setup_ui(&gui);
    RtcPort_Init(gui.screen.container_1_hour, gui.screen.container_1_min, gui.screen.container_1_sec);
    DataShowInit();
}

