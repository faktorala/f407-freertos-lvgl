#ifndef __DATA_H
#define __DATA_H

#include "freertos_os2.h"
#include "lvgl.h"

extern uint8_t humi, temp;

typedef enum{
    DHT11 = 0,

}Sensor_type;


typedef struct{
    Sensor_type sensor;
    char name[10];
    char message[10];
    void *argemnet;
}My_msg_t;


void GiveDataToMSGT(My_msg_t *msg);
void DataShowInit(void);

#endif
