#include "sensor_task.h"
#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/rom.h"
#include "drivers/buttons.h"
#include "utils/uartstdio.h"
#include "priorities.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "inc/bsp.h"

//*****************************************************************************
//
// The stack size for the Sensor task.
//
//*****************************************************************************
#define SENSORTASKSTACKSIZE        128         // Stack size in words

//*****************************************************************************
//
// The item size and queue size for the Sensor message queue.
//
//*****************************************************************************
#define SENSOR_ITEM_SIZE           sizeof(uint8_t)
#define SENSOR_QUEUE_SIZE          5

//*****************************************************************************
//
// The queue that holds messages sent to the Sensor task.
//
//*****************************************************************************
xQueueHandle g_pSensorQueue;

//
// Sensors [Accelerometer, Light] listed in alphabetical order.
// Can add more sensors once this works
//
static bool sensors[2] = { false, false };
static uint8_t SensorIndx;

extern xSemaphoreHandle g_pUARTSemaphore;

//*****************************************************************************
//
// This task toggles the user selected sensor. User
// can make the selections by pressing the left and right buttons.
//
//*****************************************************************************
static void SensorTask(void *pvParameters)
{
    portTickType ui32WakeTime;
    uint8_t i8Message;

    // Get the current tick count.
    ui32WakeTime = xTaskGetTickCount();

    // Loop forever.
    while(1)
    {
        // Read the next message, if available on queue.
        if(xQueueReceive(g_pSensorQueue, &i8Message, 0) == pdPASS)
        {
            // If left button, switch to other sensor
            if(i8Message == LEFT_BUTTON)
            {
                // Update to stop reading from current sensor
                sensors[SensorIndx] = false;

                // Update the index to next sensor
                SensorIndx++;
                if(SensorIndx > 1)
                {
                    SensorIndx = 0;
                }

                // Update the buffer to turn on the next sensor
                sensors[SensorIndx] = true;

                if (SensorIndx == 0) {
                    xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
                    UARTprintf("Reading from accelerometer\n");
                    xSemaphoreGive(g_pUARTSemaphore);
                }
                else if (SensorIndx == 1) {
                    xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
                    UARTprintf("Reading from light sensor\n");
                    xSemaphoreGive(g_pUARTSemaphore);
                }

            }

            if(i8Message == RIGHT_BUTTON)
            {
                // Guard UART from concurrent access. Print the currently
                // blinking frequency.
                xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
                UARTprintf("Just pressed right button.\n");
                xSemaphoreGive(g_pUARTSemaphore);
            }
        }

        // Read and print from the selected sensor
        // Accelerometer
        if (sensors[0] == true) {
            // Get a sensor reading
            uint16_t x,y,z;
            BSP_Accelerometer_Input(&x, &y, &z);

            // Guard UART from concurrent access.
            xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);

            // Print the reading
            UARTprintf("[x,y,z] = [%d, %d, %d]\n", x,y,z);
        }

        // Light Sensor
        else if (sensors[1] == true) {
            // Get a sensor reading
            uint32_t light;
            light = BSP_LightSensor_Input();

            // Guard UART from concurrent access.
            xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);

            // Print the reading
            UARTprintf("light = %d\n", light);
        }

        xSemaphoreGive(g_pUARTSemaphore);

    }
}

//*****************************************************************************
//
// Initializes the Sensor task.
//
//*****************************************************************************
int SensorTaskInit(void)
{
    // Initialize the accelerometer and light sensor.
    BSP_Accelerometer_Init();
    BSP_LightSensor_Init();

    // Start reading from the accelerometer
    SensorIndx = 0;
    sensors[SensorIndx] = true;

    // Print the reading
    UARTprintf("Reading from the accelerometer.");

    // Get a sensor reading
    uint16_t x,y,z;
    BSP_Accelerometer_Input(&x, &y, &z);

    // Guard UART from concurrent access.
    xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);

    // Print the reading
    UARTprintf("[x,y,z] = [%d, %d, %d]\n", x,y,z);

    // Create a queue for sending messages to the sensor task.
    g_pSensorQueue = xQueueCreate(SENSOR_QUEUE_SIZE, SENSOR_ITEM_SIZE);

    // Create the sensor task.
    if(xTaskCreate(SensorTask, (const portCHAR *)"Sensor", SENSORTASKSTACKSIZE, NULL,
                   tskIDLE_PRIORITY + PRIORITY_SENSOR_TASK, NULL) != pdTRUE)
    {
        return(1);
    }

    // Success.
    return(0);
}
