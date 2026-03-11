/*
 * UserTasks.c
 *
 *  Created on: Jan 5, 2026
 *      Author: stas2
 */
#include "main.h"
#include "cmsis_os.h"
#include "stm32f429i_discovery_lcd.h"
#include "math.h"
#include "UserTasks.h"

#define DAC_MAX_AMPLITUDE 2047
#define DAC_ZERO_LEVEL    2047
#define WAVE_FREQ_HZ      40000
#define FLAG_RTC_TRIGGER  0x01
#define FLAG_WAVE_TRIGGER 0x01

int freq1 = 200;

osThreadId_t Task_RTC_Handle;
const osThreadAttr_t User_Task_RTC_attributes = {
  .name = "User_Task_RTC",
  .priority = (osPriority_t)osPriorityNormal,
  .stack_size = 1024
};

osThreadId_t Task_LCD_Handle;
const osThreadAttr_t User_Task_LCD_attributes = {
  .name = "User_Task_LCD",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 1024
};

osThreadId_t Task_ADC_Handle;
const osThreadAttr_t User_Task_ADC_attributes = {
  .name = "User_Task_ADC",
  .priority = (osPriority_t) osPriorityNormal,
  .stack_size = 1024
};

osThreadId_t Task_WaveOut_Handle;
const osThreadAttr_t User_Task_WaveOut_attributes = {
  .name = "User_Task_WaveOut",
  .priority = (osPriority_t) osPriorityRealtime7,
  .stack_size = 1024
};
osMessageQueueId_t Queue_LCDHandle;
const osMessageQueueAttr_t Queue_LCD_attributes = {
  .name = "Queue_LCD"
};



void startTasks(){
	  Queue_LCDHandle = osMessageQueueNew(16, sizeof(LCD_Message_t), &Queue_LCD_attributes);

	  Task_RTC_Handle = osThreadNew(Task_RTC, NULL, &User_Task_RTC_attributes );
	  Task_LCD_Handle = osThreadNew(Task_LCD, NULL, &User_Task_LCD_attributes );
	  Task_WaveOut_Handle = osThreadNew(Task_WaveOut, NULL, &User_Task_WaveOut_attributes );
	  Task_ADC_Handle = osThreadNew(Task_ADC, NULL, &User_Task_ADC_attributes );
}




void Task_WaveOut(void *argument)
{

    uint32_t phase_increment = 0;
    //uint32_t phase_increment2 = 0;
    float sineValue1;
    float sineValue2;
    //uint16_t sinInt1;
	//uint16_t sinInt2;
    uint16_t dac_value;
    const float PI2 = 6.28;
    //uint16_t freq_old=0;
    //uint16_t counter =0;
    HAL_DAC_Start(&hdac, DAC_CHANNEL_2);

    for(;;)
    {
        osThreadFlagsWait(FLAG_WAVE_TRIGGER, osFlagsWaitAny, osWaitForever);
        HAL_GPIO_WritePin(GPIOG, GPIO_PIN_14, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET);
        phase_increment++;
        if(phase_increment>WAVE_FREQ_HZ/freq1)
        {
        	phase_increment=0;
        }




        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);

        sineValue1 = sinf(phase_increment*PI2*freq1/WAVE_FREQ_HZ);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        sineValue2 = sinf(phase_increment2*PI2*300/WAVE_FREQ_HZ);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

        dac_value = (uint16_t)(DAC_ZERO_LEVEL + DAC_MAX_AMPLITUDE *(sineValue1+ sineValue2)/2 );
        //sinInt1 = mySinus(phase_increment*freq1/200);
        //sinInt2 = mySinus(phase_increment2*300/200);
        //dac_value = (sinInt1+sinInt2)/2 ;


        // Write to Hardware
        HAL_DAC_SetValue(&hdac, DAC_CHANNEL_2, DAC_ALIGN_12B_R, dac_value);

        // [Profiling] Clear PC12 (End of computation)
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET);
    }
}


void Task_ADC(void *argument)
{
    uint16_t raw_adc;
    uint16_t calc_freq;
    LCD_Message_t msg;
    msg.type = MSG_FREQ_UPDATE;
    uint16_t last_sent_freq = 0.0f;

    for(;;)
    {
        osDelay(100);

        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
        {
            raw_adc = HAL_ADC_GetValue(&hadc1);

            calc_freq = 200 + raw_adc * 200/ 4095;

            freq1 = calc_freq;


            if (abs(calc_freq - last_sent_freq) >= 1)
            {
                msg.data.freq_val = calc_freq;
                if(osMessageQueuePut(Queue_LCDHandle, &msg, 0, 0) == osOK)
                {
                    last_sent_freq = calc_freq;
                }
            }
        }
        HAL_ADC_Stop(&hadc1);
    }
}


void Task_RTC(void *argument)
{
    uint8_t min = 0;
    uint8_t sec = 0;
    uint8_t deci = 0;

    LCD_Message_t msg;
    msg.type = MSG_TIME_UPDATE;

    for(;;)
    {

        osThreadFlagsWait(FLAG_RTC_TRIGGER, osFlagsWaitAny, osWaitForever);

        deci++;
        if (deci >= 10) {
            deci = 0;
            sec++;
            if (sec >= 60) {
                sec = 0;
                min++;
            }
        }


        if (deci == 0) {
            HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_SET);
        } else if (deci == 1) {
            HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_RESET);
        }

        msg.data.time.min = min;
        msg.data.time.sec = sec;
        msg.data.time.deci = deci;
        osMessageQueuePut(Queue_LCDHandle, &msg, 0, 0);
    }
}


void Task_LCD(void *argument)
{
    LCD_Message_t msg;
    char str[40];
    char freq[40];
    // Initialize LCD
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(LCD_BACKGROUND_LAYER, LCD_FRAME_BUFFER);
    BSP_LCD_SelectLayer(LCD_BACKGROUND_LAYER);
    BSP_LCD_Clear(LCD_COLOR_WHITE);
    BSP_LCD_SetBackColor(LCD_COLOR_WHITE);
    BSP_LCD_SetTextColor(LCD_COLOR_BLACK);

    BSP_LCD_SetFont(&Font20);

    BSP_LCD_SetFont(&Font16);

    for(;;)
    {
        // Block until message arrives

        if (osMessageQueueGet(Queue_LCDHandle, &msg, NULL, osWaitForever) == osOK)
        {

            if (msg.type == MSG_TIME_UPDATE)
            {
                sprintf(str, "Time: %02d:%02d.%d", msg.data.time.min, msg.data.time.sec, msg.data.time.deci);
                BSP_LCD_DisplayStringAtLine(0, (uint8_t*)str);
            }
            else if (msg.type == MSG_FREQ_UPDATE)
            {

                sprintf(freq, "Freq: %03u.0f Hz  ", (unsigned int)msg.data.freq_val);
                BSP_LCD_DisplayStringAtLine(2, (uint8_t*)freq);
            }
        }
    }
}

