#include "PLL.h"
#include "SysTick.h"
#include "OS.h"
#include "semaphore.h"
#include "adc.h"
#include "ST7735.h"
#include "interpreter.h"
#include "can0.h"
#include "gpio_debug.h"
#include "ir_sensor.h"
#include "ping.h"

#define SAMPLING_RATE 2000
#define TIMESLICE 2*TIME_1MS  // thread switch time in system time units

#define MAIN 0		//1 = receiver, 0 = transmitter
/************************ Debug info ***********************/
unsigned int NumCreated;

/********************** Public functions *******************/

#if MAIN

void NetworkReceive(void) {
	PackageID receiveID;
	unsigned char canData[4];
	Debug_LED_Init();
	CAN0_Open();
	while (1) {
		CAN0_GetMail(&receiveID, canData);
		switch(receiveID) {
			case IRSensor0:
				ST7735_Message(0,0,"IR0: ", ((unsigned short *)canData)[0]);
			break;
			case UltraSonic:
				ST7735_Message(0,1,"ULS0: ", ((unsigned long *)canData)[0]);
			break;
			default:
			break;
		}
	}
}

int main(void) {
  PLL_Init();
  
  ST7735_InitR(INITR_REDTAB);
	ST7735_SetRotation(1);
  ST7735_FillScreen(0); // set screen to black

  //OS_InitSemaphore(&Sema4UART, 1);
  OS_Init();
  
  NumCreated = 0;
  //NumCreated += OS_AddThread(&Interpreter,128,1); 
  NumCreated += OS_AddThread(&NetworkReceive,128,1); 
  
  OS_Launch(TIMESLICE);
}

#else 

void IRSensorSend(void) {
	unsigned short IRvalues[4];
	unsigned long sonarValues[4];
	unsigned char CanData[4];
	IR_getValues(IRvalues);
	((unsigned short*)CanData)[0] = IRvalues[0];
	CAN0_SendData(IRSensor0, CanData);
	Ping_getData (sonarValues);
	((unsigned long*)CanData)[0] = sonarValues[0];
	CAN0_SendData(UltraSonic, CanData);
}

int main(void) {
  PLL_Init();
  
  OS_Init();
	IR_Init();
	
  NumCreated = 0;
  //NumCreated += OS_AddThread(&Interpreter,128,1); 
  NumCreated += OS_AddPeriodicThread(&IRSensorSend, 100*TIME_1MS, 3); 
  CAN0_Open();
	Ping_Init();
  OS_Launch(TIMESLICE);
}

#endif
