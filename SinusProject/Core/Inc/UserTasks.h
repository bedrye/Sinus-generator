#include "stm32f4xx_hal.h"

#include "cmsis_os.h"
#include <stdio.h>


extern osThreadId_t Task_RTC_Handle;
extern osThreadId_t Task_WaveOut_Handle;
extern osMessageQueueId_t Queue_LCDHandle;

typedef enum {
    MSG_TIME_UPDATE,
    MSG_FREQ_UPDATE
} MsgType_t;

typedef struct {
    MsgType_t type;
    union {
        struct {
            uint8_t min;
            uint8_t sec;
            uint8_t deci;
        } time;
        uint16_t freq_val;
    } data;
} LCD_Message_t;

void Error_Handler(void);

void startTasks();
void Task_RTC(void *argument);
void Task_LCD(void *argument);
void Task_WaveOut(void *argument);
void Task_ADC(void *argument);
