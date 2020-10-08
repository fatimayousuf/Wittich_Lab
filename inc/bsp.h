/*
 * bsp.h
 *
 *  Created on: Sep 30, 2020
 *      Author: fatimayousuf
 */

#ifndef INC_BSP_H_
#define INC_BSP_H_

// BSP Timer
void BSP_Clock_InitFastest(void);

// Accelerometer
static void  adcinit(void);
void BSP_Accelerometer_Input(uint16_t *x, uint16_t *y, uint16_t *z);
void BSP_Accelerometer_Init(void);

//Light sensor
void BSP_LightSensor_Init(void);
uint32_t BSP_LightSensor_Input(void);
void BSP_LightSensor_Start(void);
int BSP_LightSensor_End(uint32_t *light);

#endif /* INC_BSP_H_ */
