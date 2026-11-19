/**
 * @file lv_port_indev_template.c
 *
 */

/*Copy this file as "lv_port_indev.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"
#include "CST816.h"
/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void touchpad_init(void);
static void touchpad_read(lv_indev_t * indev, lv_indev_data_t * data);
static bool touchpad_is_pressed(void);
static void touchpad_get_xy(int32_t * x, int32_t * y);

/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t * indev_touchpad;

/* touch diagnostics: check in the debugger after boot */
volatile uint8_t g_touch_chipid = 0;   /* non-0x00 / non-0xFF means I2C works */
volatile uint8_t g_touch_ready  = 0;   /* 1 = CST816 answered */
extern CST816_Info	CST816_Instance;
/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(void)
{
    /**
     * Here you will find example implementation of input devices supported by LittelvGL:
     *  - Touchpad
     *  - Mouse (with cursor support)
     *  - Keypad (supports GUI usage only with key)
     *  - Encoder (supports GUI usage only with: left, right, push)
     *  - Button (external buttons to press points on the screen)
     *
     *  The `..._read()` function are only examples.
     *  You should shape them according to your hardware
     */

    /*------------------
     * Touchpad
     * -----------------*/

    /*Initialize your touchpad if you have*/
    touchpad_init();

    /*Register a touchpad input device*/
    indev_touchpad = lv_indev_create();
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, touchpad_read);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*------------------
 * Touchpad
 * -----------------*/

/*Initialize your touchpad*/
static void touchpad_init(void)
{
    /* CST816 needs RST driven high plus a reset pulse, otherwise it stays
       held in reset (MX_GPIO_Init leaves PA15 low) and never answers on I2C.
       This also configures PB4/PB6 as the bit-banged I2C bus. */
    CST816_GPIO_Init();
    CST816_RESET();

    /* disable auto-sleep so the panel keeps scanning for touches */
    CST816_IIC_WriteREG(0xFE, 0x01);      /* DisAutoSleep = 1 */

    g_touch_chipid = CST816_Get_ChipID();
    g_touch_ready  = (g_touch_chipid != 0x00 && g_touch_chipid != 0xFF) ? 1 : 0;
}

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
{
    static int32_t last_x = 0;
    static int32_t last_y = 0;

    /*Save the pressed coordinates and the state*/
    if(touchpad_is_pressed()) {
        touchpad_get_xy(&last_x, &last_y);
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    /*Set the last pressed coordinates*/
    data->point.x = last_x;
    data->point.y = last_y;
}

/*Return true is the touchpad is pressed*/
static bool touchpad_is_pressed(void)
{
    uint8_t fingers = CST816_Get_FingerNum();
    return (fingers != 0x00 && fingers != 0xFF) ? true : false;
}

/*Get the x and y coordinates if the touchpad is pressed*/
static void touchpad_get_xy(int32_t * x, int32_t * y)
{
    /*Your code comes here*/
	CST816_Get_XY_AXIS();
    (*x) = CST816_Instance.X_Pos;
    (*y) = CST816_Instance.Y_Pos;
}


#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
