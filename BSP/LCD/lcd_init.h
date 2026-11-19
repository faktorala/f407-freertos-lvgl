#ifndef __LCD_INIT_H
#define __LCD_INIT_H

#include "sys.h"

#define USE_HORIZONTAL 0

/* 1 = bit-bang software SPI (use this to rule out SPI1 peripheral/wiring)
 * 0 = hardware SPI1 + DMA (normal mode) */
#define LCD_USE_SOFT_SPI 0   /* 0 = hardware SPI1 + DMA, 1 = bit-bang (debug only) */


#if USE_HORIZONTAL==0||USE_HORIZONTAL==1
#define LCD_W 240
#define LCD_H 280

#else
#define LCD_W 280
#define LCD_H 240
#endif


//-----------------LCD�˿ڶ���---------------- 
#define SCLK_PORT			GPIOA
#define SCLK_PIN			GPIO_PIN_5

#define SDA_PORT			GPIOA
#define SDA_PIN				GPIO_PIN_7

#define RES_PORT			GPIOA
#define RES_PIN				GPIO_PIN_4

#define DC_PORT				GPIOA
#define DC_PIN				GPIO_PIN_3

#define CS_PORT				GPIOA
#define CS_PIN				GPIO_PIN_2

#define BLK_PORT			GPIOA
#define BLK_PIN				GPIO_PIN_6

#define LCD_SCLK_Clr() HAL_GPIO_WritePin(SCLK_PORT,SCLK_PIN,GPIO_PIN_RESET)//SCL=SCLK
#define LCD_SCLK_Set() HAL_GPIO_WritePin(SCLK_PORT,SCLK_PIN,GPIO_PIN_SET)

#define LCD_MOSI_Clr() HAL_GPIO_WritePin(SDA_PORT,SDA_PIN,GPIO_PIN_RESET)//SDA=MOSI
#define LCD_MOSI_Set() HAL_GPIO_WritePin(SDA_PORT,SDA_PIN,GPIO_PIN_SET)

#define LCD_RES_Clr()  HAL_GPIO_WritePin(RES_PORT,RES_PIN,GPIO_PIN_RESET)//RES
#define LCD_RES_Set()  HAL_GPIO_WritePin(RES_PORT,RES_PIN,GPIO_PIN_SET)

#define LCD_DC_Clr()   HAL_GPIO_WritePin(DC_PORT,DC_PIN,GPIO_PIN_RESET)//DC
#define LCD_DC_Set()   HAL_GPIO_WritePin(DC_PORT,DC_PIN,GPIO_PIN_SET)
 		     
#define LCD_CS_Clr()   HAL_GPIO_WritePin(CS_PORT,CS_PIN,GPIO_PIN_RESET)//CS
#define LCD_CS_Set()   HAL_GPIO_WritePin(CS_PORT,CS_PIN,GPIO_PIN_SET)

#define LCD_BLK_Clr()  HAL_GPIO_WritePin(BLK_PORT,BLK_PIN,GPIO_PIN_RESET)//BLK
#define LCD_BLK_Set()  HAL_GPIO_WritePin(BLK_PORT,BLK_PIN,GPIO_PIN_SET)

void LCD_GPIO_Init(void);//��ʼ��GPIO
void LCD_Writ_Bus(u8 dat);//ģ��SPIʱ��
void LCD_WR_DATA8(u8 dat);//д��һ���ֽ�
void LCD_WR_DATA(u16 dat);//д�������ֽ�
void LCD_WR_REG(u8 dat);//д��һ��ָ��
void LCD_Address_Set(u16 x1,u16 y1,u16 x2,u16 y2);//�������꺯��
void LCD_Init(void);//LCD��ʼ��
void LCD_Set_Light(uint8_t dc);
void LCD_Close_Light(void);
void LCD_ST7789_SleepIn(void);
void LCD_ST7789_SleepOut(void);
void LCD_Open_Light(void);
#endif




