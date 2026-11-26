/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lvgl.h"
#include "dht11.h"
#include "data.h"
#include "print.h"
#include "w25q64.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for lvgl_defaultTas */
osThreadId_t lvgl_defaultTasHandle;
const osThreadAttr_t lvgl_defaultTas_attributes = {
  .name = "lvgl_defaultTas",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for GetDataTask */
osThreadId_t GetDataTaskHandle;
const osThreadAttr_t GetDataTask_attributes = {
  .name = "GetDataTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for ShowTask */
osThreadId_t ShowTaskHandle;
const osThreadAttr_t ShowTask_attributes = {
  .name = "ShowTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for DataQueue */
osMessageQueueId_t DataQueueHandle;
const osMessageQueueAttr_t DataQueue_attributes = {
  .name = "DataQueue"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void LvglDefaultTask(void *argument);
void GetStartTask(void *argument);
void ShowStartTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* Hook prototypes */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName);

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, signed char *pcTaskName)
{
  __disable_irq();
    for (;;) { }  
   /* Run time stack overflow checking is performed if
   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
   called if a stack overflow is detected. */
}
/* USER CODE END 4 */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */

  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of DataQueue */
  DataQueueHandle = osMessageQueueNew (16, sizeof(My_msg_t), &DataQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of lvgl_defaultTas */
  lvgl_defaultTasHandle = osThreadNew(LvglDefaultTask, NULL, &lvgl_defaultTas_attributes);

  /* creation of GetDataTask */
  GetDataTaskHandle = osThreadNew(GetStartTask, NULL, &GetDataTask_attributes);

  /* creation of ShowTask */
  ShowTaskHandle = osThreadNew(ShowStartTask, NULL, &ShowTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  my_printf_init();
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_LvglDefaultTask */
/**
  * @brief  Function implementing the lvgl_defaultTas thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_LvglDefaultTask */
void LvglDefaultTask(void *argument)
{
  /* USER CODE BEGIN LvglDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    lv_timer_handler();
    osDelay(5);
  }
  /* USER CODE END LvglDefaultTask */
}

/* USER CODE BEGIN Header_GetStartTask */
/**
* @brief Function implementing the GetDataTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_GetStartTask */
void GetStartTask(void *argument)
{
  /* USER CODE BEGIN GetStartTask */
  DHT11_Init();
  My_msg_t dht_msg = {.sensor = DHT11};
  if (W25Q64_Init() != W25Q64_OK) {
    my_printf("W25Q64 not found, id=0x%06X\r\n", W25Q64_ReadJedecID());
    
  } else {
    my_printf("W25Q64 ok, id=0x%06X, cap=%u KB\r\n",
              W25Q64_ReadJedecID(), W25Q64_GetCapacity() / 1024U);
    uint8_t back[30];
    W25Q64_Read(0x000000, back, sizeof(back));
    my_printf("data : %s", back);
  }
  /* Infinite loop */
  for(;;)
  {
    DHT11_Read_Data(&temp, &humi);
    GiveDataToMSGT(&dht_msg);
    osMessageQueuePut(DataQueueHandle, &dht_msg, NULL, 100);
    osDelay(1000);
  }
  /* USER CODE END GetStartTask */
}

/* USER CODE BEGIN Header_ShowStartTask */
/**
* @brief Function implementing the ShowTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_ShowStartTask */
void ShowStartTask(void *argument)
{
  /* USER CODE BEGIN ShowStartTask */
  My_msg_t dht_msg;
  /* Infinite loop */
  for(;;)
  {
    osMessageQueueGet(DataQueueHandle, &dht_msg, NULL, 100);
    my_printf("humi: %d, temp : %d \n", dht_msg.message[0], dht_msg.message[1]);
    osDelay(1000);
  }
  /* USER CODE END ShowStartTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

