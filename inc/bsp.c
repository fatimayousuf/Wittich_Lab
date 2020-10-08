#include <stdint.h>
#include "inc/tm4c123gh6pm.h"
#include "bsp.h"

/****** BSP Timer ******/
void BSP_Clock_InitFastest(void){
  // 0) configure the system to use RCC2 for advanced features
  //    such as 400 MHz PLL and non-integer System Clock Divisor
  SYSCTL_RCC2_R |= SYSCTL_RCC2_USERCC2;
  // 1) bypass PLL while initializing
  SYSCTL_RCC2_R |= SYSCTL_RCC2_BYPASS2;
  // 2) select the crystal value and oscillator source
  SYSCTL_RCC_R &= ~SYSCTL_RCC_XTAL_M;   // clear XTAL field
  SYSCTL_RCC_R += SYSCTL_RCC_XTAL_16MHZ;// configure for 16 MHz crystal
  SYSCTL_RCC2_R &= ~SYSCTL_RCC2_OSCSRC2_M;// clear oscillator source field
  SYSCTL_RCC2_R += SYSCTL_RCC2_OSCSRC2_MO;// configure for main oscillator source
  // 3) activate PLL by clearing PWRDN
  SYSCTL_RCC2_R &= ~SYSCTL_RCC2_PWRDN2;
  // 4) set the desired system divider and the system divider least significant bit
  SYSCTL_RCC2_R |= SYSCTL_RCC2_DIV400;  // use 400 MHz PLL
  SYSCTL_RCC2_R = (SYSCTL_RCC2_R&~0x1FC00000) // clear system clock divider field
                  + (4<<22);      // configure for 80 MHz clock
  // 5) wait for the PLL to lock by polling PLLLRIS
  while((SYSCTL_RIS_R&SYSCTL_RIS_PLLLRIS)==0){};
  // 6) enable use of PLL by clearing BYPASS
  SYSCTL_RCC2_R &= ~SYSCTL_RCC2_BYPASS2;
  //ClockFrequency = 80000000;
}


/****** I2C *******/
#define MAXRETRIES              5  // number of receive attempts before giving up
void static i2cinit(void){
  SYSCTL_RCGCI2C_R |= 0x0002;      // 1a) activate clock for I2C1
  SYSCTL_RCGCGPIO_R |= 0x0001;     // 1b) activate clock for Port A
  while((SYSCTL_PRGPIO_R&0x01) == 0){};// allow time for clock to stabilize
                                   // 2) no need to unlock PA7-6
  GPIO_PORTA_AMSEL_R &= ~0xC0;     // 3) disable analog functionality on PA7-6
                                   // 4) configure PA7-6 as I2C1
  GPIO_PORTA_PCTL_R = (GPIO_PORTA_PCTL_R&0x00FFFFFF)+0x33000000;
  GPIO_PORTA_ODR_R |= 0x80;        // 5) enable open drain on PA7 only
  GPIO_PORTA_AFSEL_R |= 0xC0;      // 6) enable alt funct on PA7-6
  GPIO_PORTA_DEN_R |= 0xC0;        // 7) enable digital I/O on PA7-6
  I2C1_MCR_R = I2C_MCR_MFE;        // 8) master function enable
  I2C1_MTPR_R = 39;                // 9) configure for 100 kbps clock
  // 20*(TPR+1)*12.5ns = 10us, with TPR=39
}

uint16_t static I2C_Send1(int8_t slave, uint8_t data1){
  while(I2C1_MCS_R&I2C_MCS_BUSY){};// wait for I2C ready
  I2C1_MSA_R = (slave<<1)&0xFE;    // MSA[7:1] is slave address
  I2C1_MSA_R &= ~0x01;             // MSA[0] is 0 for send
  I2C1_MDR_R = data1&0xFF;         // prepare first byte
  I2C1_MCS_R = (0
//                       & ~I2C_MCS_ACK     // no data ack (no data on send)
                       | I2C_MCS_STOP     // generate stop
                       | I2C_MCS_START    // generate start/restart
                       | I2C_MCS_RUN);    // master enable
  while(I2C1_MCS_R&I2C_MCS_BUSY){};// wait for transmission done
                                          // return error bits
  return (I2C1_MCS_R&(I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));
}

uint16_t static I2C_Send2(int8_t slave, uint8_t data1, uint8_t data2){
  while(I2C1_MCS_R&I2C_MCS_BUSY){};// wait for I2C ready
  I2C1_MSA_R = (slave<<1)&0xFE;    // MSA[7:1] is slave address
  I2C1_MSA_R &= ~0x01;             // MSA[0] is 0 for send
  I2C1_MDR_R = data1&0xFF;         // prepare first byte
  I2C1_MCS_R = (0
//                       & ~I2C_MCS_ACK     // no data ack (no data on send)
//                       & ~I2C_MCS_STOP    // no stop
                       | I2C_MCS_START    // generate start/restart
                       | I2C_MCS_RUN);    // master enable
  while(I2C1_MCS_R&I2C_MCS_BUSY){};// wait for transmission done
                                          // check error bits
  if((I2C1_MCS_R&(I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR)) != 0){
    I2C1_MCS_R = (0                // send stop if nonzero
//                       & ~I2C_MCS_ACK     // no data ack (no data on send)
                       | I2C_MCS_STOP     // stop
//                       & ~I2C_MCS_START   // no start/restart
//                       & ~I2C_MCS_RUN     // master disable
                        );
                                          // return error bits if nonzero
    return (I2C1_MCS_R&(I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));
  }
  I2C1_MDR_R = data2&0xFF;         // prepare second byte
  I2C1_MCS_R = (0
//                       & ~I2C_MCS_ACK     // no data ack (no data on send)
                       | I2C_MCS_STOP     // generate stop
//                       & ~I2C_MCS_START   // no start/restart
                       | I2C_MCS_RUN);    // master enable
  while(I2C1_MCS_R&I2C_MCS_BUSY){};// wait for transmission done
                                          // return error bits
  return (I2C1_MCS_R&(I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));
}

uint16_t static I2C_Send3(int8_t slave, uint8_t data1, uint8_t data2, uint8_t data3){
  while(I2C1_MCS_R&I2C_MCS_BUSY){};// wait for I2C ready
  I2C1_MSA_R = (slave<<1)&0xFE;    // MSA[7:1] is slave address
  I2C1_MSA_R &= ~0x01;             // MSA[0] is 0 for send
  I2C1_MDR_R = data1&0xFF;         // prepare first byte
  I2C1_MCS_R = (0
//                       & ~I2C_MCS_ACK     // no data ack (no data on send)
//                       & ~I2C_MCS_STOP    // no stop
                       | I2C_MCS_START    // generate start/restart
                       | I2C_MCS_RUN);    // master enable
  while(I2C1_MCS_R&I2C_MCS_BUSY){};// wait for transmission done
                                          // check error bits
  if((I2C1_MCS_R&(I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR)) != 0){
    I2C1_MCS_R = (0                // send stop if nonzero
//                       & ~I2C_MCS_ACK     // no data ack (no data on send)
                       | I2C_MCS_STOP     // stop
//                       & ~I2C_MCS_START   // no start/restart
//                       & ~I2C_MCS_RUN     // master disable
                       );
                                          // return error bits if nonzero
    return (I2C1_MCS_R&(I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));
  }
  I2C1_MDR_R = data2&0xFF;         // prepare second byte
  I2C1_MCS_R = (0
//                       & ~I2C_MCS_ACK     // no data ack (no data on send)
//                       & ~I2C_MCS_STOP    // no stop
//                       & ~I2C_MCS_START   // no start/restart
                       | I2C_MCS_RUN);    // master enable
  while(I2C1_MCS_R&I2C_MCS_BUSY){};// wait for transmission done
                                          // check error bits
  if((I2C1_MCS_R&(I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR)) != 0){
    I2C1_MCS_R = (0                // send stop if nonzero
//                       & ~I2C_MCS_ACK     // no data ack (no data on send)
                       | I2C_MCS_STOP     // stop
//                       & ~I2C_MCS_START   // no start/restart
//                       & ~I2C_MCS_RUN   // master disable
                        );
                                          // return error bits if nonzero
    return (I2C1_MCS_R&(I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));
  }
  I2C1_MDR_R = data3&0xFF;         // prepare third byte
  I2C1_MCS_R = (0
//                       & ~I2C_MCS_ACK     // no data ack (no data on send)
                       | I2C_MCS_STOP     // generate stop
//                       & ~I2C_MCS_START   // no start/restart
                       | I2C_MCS_RUN);    // master enable
  while(I2C1_MCS_R&I2C_MCS_BUSY){};// wait for transmission done
                                          // return error bits
  return (I2C1_MCS_R&(I2C_MCS_DATACK|I2C_MCS_ADRACK|I2C_MCS_ERROR));
}

uint16_t static I2C_Recv2(int8_t slave){
  uint8_t data1,data2;
  int retryCounter = 1;
  do{
    while(I2C1_MCS_R&I2C_MCS_BUSY){};// wait for I2C ready
    I2C1_MSA_R = (slave<<1)&0xFE;    // MSA[7:1] is slave address
    I2C1_MSA_R |= 0x01;              // MSA[0] is 1 for receive
    I2C1_MCS_R = (0
                         | I2C_MCS_ACK      // positive data ack
 //                        & ~I2C_MCS_STOP    // no stop
                         | I2C_MCS_START    // generate start/restart
                         | I2C_MCS_RUN);    // master enable
    while(I2C1_MCS_R&I2C_MCS_BUSY){};// wait for transmission done
    data1 = (I2C1_MDR_R&0xFF);       // MSB data sent first
    I2C1_MCS_R = (0
//                         & ~I2C_MCS_ACK     // negative data ack (last byte)
                         | I2C_MCS_STOP     // generate stop
//                         & ~I2C_MCS_START   // no start/restart
                         | I2C_MCS_RUN);    // master enable
    while(I2C1_MCS_R&I2C_MCS_BUSY){};// wait for transmission done
    data2 = (I2C1_MDR_R&0xFF);       // LSB data sent last
    retryCounter = retryCounter + 1;        // increment retry counter
  }                                         // repeat if error
  while(((I2C1_MCS_R&(I2C_MCS_ADRACK|I2C_MCS_ERROR)) != 0) && (retryCounter <= MAXRETRIES));
  return (data1<<8)+data2;                  // usually returns 0xFFFF on error
}

/****** ACCELEROMETER *******/
static void  adcinit(void){
  SYSCTL_RCGCADC_R |= 0x00000001;  // 1) activate ADC0
  while((SYSCTL_PRADC_R&0x01) == 0){};// 2) allow time for clock to stabilize
                                   // 3-7) GPIO initialization in more specific functions
  ADC0_PC_R &= ~0xF;               // 8) clear max sample rate field
  ADC0_PC_R |= 0x1;                //    configure for 125K samples/sec
  ADC0_SSPRI_R = 0x3210;           // 9) Sequencer 3 is lowest priority
                                   // 10-15) sample sequencer initialization in more specific functions
}
void BSP_Accelerometer_Init(void){
  adcinit();
  SYSCTL_RCGCGPIO_R |= 0x00000008; // 1) activate clock for Port D
  while((SYSCTL_PRGPIO_R&0x08) == 0){};// allow time for clock to stabilize
                                   // 2) no need to unlock PD2-0
  GPIO_PORTD_AMSEL_R |= 0x07;      // 3) enable analog on PD2-0
                                   // 4) configure PD2-0 as ?? (skip this line because PCTL is for digital only)
  GPIO_PORTD_DIR_R &= ~0x07;       // 5) make PD2-0 input
  GPIO_PORTD_AFSEL_R |= 0x07;      // 6) enable alt funct on PD2-0
  GPIO_PORTD_DEN_R &= ~0x07;       // 7) enable analog functionality on PD2-0
  adcinit();                       // 8-9) general ADC initialization
  ADC0_ACTSS_R &= ~0x0004;         // 10) disable sample sequencer 2
  ADC0_EMUX_R &= ~0x0F00;          // 11) seq2 is software trigger
  ADC0_SSMUX2_R = 0x0567;          // 12) set channels for SS2
  ADC0_SSCTL2_R = 0x0600;          // 13) no D0 END0 IE0 TS0 D1 END1 IE1 TS1 D2 TS2, yes IE2 END2
  ADC0_IM_R &= ~0x0004;            // 14) disable SS2 interrupts
  ADC0_ACTSS_R |= 0x0004;          // 15) enable sample sequencer 2
}
void BSP_Accelerometer_Input(uint16_t *x, uint16_t *y, uint16_t *z){
  ADC0_PSSI_R = 0x0004;            // 1) initiate SS2
  while((ADC0_RIS_R&0x04)==0){};   // 2) wait for conversion done
  *x = ADC0_SSFIFO2_R>>2;          // 3a) read first result
  *y = ADC0_SSFIFO2_R>>2;          // 3b) read second result
  *z = ADC0_SSFIFO2_R>>2;          // 3c) read third result
  ADC0_ISC_R = 0x0004;             // 4) acknowledge completion
}

/****** LIGHT SENSOR *******/
void BSP_LightSensor_Init(void){
  i2cinit();
                                   // 1) activate clock for Port A (done in i2cinit())
                                   // allow time for clock to stabilize (done in i2cinit())
                                   // 2) no need to unlock PA5
  GPIO_PORTA_AMSEL_R &= ~0x20;     // 3) disable analog on PA5
                                   // 4) configure PA5 as GPIO
  GPIO_PORTA_PCTL_R = (GPIO_PORTA_PCTL_R&0xFF0FFFFF)+0x00000000;
  GPIO_PORTA_DIR_R &= ~0x20;       // 5) make PA5 input
  GPIO_PORTA_AFSEL_R &= ~0x20;     // 6) disable alt funct on PA5
  GPIO_PORTA_DEN_R |= 0x20;        // 7) enable digital I/O on PA5
}

void static lightsensorstart(uint8_t slaveAddress){
  // configure Low Limit Register (0x02) for:
  // INT pin active after each conversion completes
  I2C_Send3(slaveAddress, 0x02, 0xC0, 0x00);
  // configure Configuration Register (0x01) for:
  // 15-12 RN         range number         1100b = automatic full-scale setting mode
  // 11    CT         conversion time         1b = 800 ms
  // 10-9  M          mode of conversion     01b = single-shot
  // 8     OVF        overflow flag field     0b (read only)
  // 7     CRF        conversion ready field  0b (read only)
  // 6     FH         flag high field         0b (read only)
  // 5     FL         flag low field          0b (read only)
  // 4     L          latch                   1b = latch interrupt if measurement exceeds programmed ranges
  // 3     POL        polarity                0b = INT pin reports active low
  // 2     ME         mask exponent           0b = do not mask exponent (more math later)
  // 1-0   FC         fault count            00b = 1 fault triggers interrupt
  I2C_Send3(slaveAddress, 0x01, 0xCA, 0x10);
  I2C_Recv2(slaveAddress);         // read Configuration Register to reset conversion ready
}

int32_t static lightsensorend(uint8_t slaveAddress){
  uint16_t raw, config;
  I2C_Send1(slaveAddress, 0x00);   // pointer register 0x00 = Result Register
  raw = I2C_Recv2(slaveAddress);
  // force the INT pin to clear by clearing and resetting the latch bit of the Configuration Register (0x01)
  I2C_Send1(slaveAddress, 0x01);   // pointer register 0x01 = Configuration Register
  config = I2C_Recv2(slaveAddress);// current Configuration Register
  I2C_Send3(slaveAddress, 0x01, (config&0xFF00)>>8, (config&0x00FF)&~0x0010);
  I2C_Send3(slaveAddress, 0x01, (config&0xFF00)>>8, (config&0x00FF)|0x0010);
  return (1<<(raw>>12))*(raw&0x0FFF);
}

#define LIGHTINT  (*((volatile uint32_t *)0x40004080))  /* PA5 */
int LightBusy = 0;                 // 0 = idle; 1 = measuring
uint32_t BSP_LightSensor_Input(void){
  uint32_t light;
  LightBusy = 1;
  lightsensorstart(0x44);
  while(LIGHTINT == 0x20){};       // wait for conversion to complete
  light = lightsensorend(0x44);
  LightBusy = 0;
  return light;
}

void BSP_LightSensor_Start(void){
  if(LightBusy == 0){
    // no measurement is in progress, so start one
    LightBusy = 1;
    lightsensorstart(0x44);
  }
}

int BSP_LightSensor_End(uint32_t *light){
  uint32_t lightLocal;
  if(LightBusy == 0){
    // no measurement is in progress, so start one
    LightBusy = 1;
    lightsensorstart(0x44);
    return 0;                      // measurement needs more time to complete
  } else{
    // measurement is in progress
    if(LIGHTINT == 0x20){
      return 0;                    // measurement needs more time to complete
    } else{
      lightLocal = lightsensorend(0x44);
      *light = lightLocal;
      LightBusy = 0;
      return 1;                    // measurement is complete; pointer valid
    }
  }
}
