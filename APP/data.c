#include "data.h"

uint8_t humi, temp;

void GiveDataToMSGT(My_msg_t *msg) {
    if (msg == NULL) {
        return;
    }
    if (msg->sensor == DHT11) {
        msg->message[0] = humi;
        msg->message[1] = temp;
        msg->message[2] = '\0';
    }
}


#include "custom.h"


static void DataUpdate_timer_cb(lv_timer_t * timer)
{
    (void)timer;

    /* screen_data 是懒加载的：没左滑进去过，这些指针就都是 NULL */
    if (gui.screen_data.screen == NULL) {
        return;
    }

    lv_label_set_text_fmt(gui.screen_data.container_1_label_humi, "%02u", humi);
    lv_label_set_text_fmt(gui.screen_data.container_1_label_temp, "%02u", temp);
}

void DataShowInit(void)
{
    lv_timer_create(DataUpdate_timer_cb, 1000, NULL);
}
