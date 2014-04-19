#include "PLL.h"
#include "SysTick.h"
#include "OS.h"
#include "semaphore.h"
#include "myadc.h"
#include "ST7735.h"
#include "interpreter.h"
#include "can0.h"
#include "gpio_debug.h"
#include "ir_sensor.h"
#include "ping.h"
#include "motor.h"

#define SAMPLING_RATE 2000
#define TIMESLICE 2*TIME_1MS  // thread switch time in system time units

#define MAIN 1		//1 = receiver, 0 = transmitter
/************************ Debug info ***********************/
unsigned int NumCreated;

/********************** Public functions *******************/

#if MAIN
static long CurrentSpeed0 = 0;
static long CurrentSpeed1 = 0;
static long RefSpeed0 = 0;
static long RefSpeed1 = 0;

void NetworkReceive(void) {
	PackageID receiveID;
	unsigned char canData[4];
	Debug_LED_Init();
	CAN0_Open();
	Motor_Init(5000);
	Motor_0_MotionUpdate(0, 1);
	Motor_1_MotionUpdate(0, 1);
	RefSpeed0 = RefSpeed1 = 3000;
	ST7735_Message(0,4,"Test", 0);
	while (1) {
		CAN0_GetMail(&receiveID, canData);	
		switch(receiveID) {
			unsigned short sensor1;
			case IRSensor0:
				sensor1 = ((unsigned short *)canData)[0];
				ST7735_Message(0,0,"IR0: ", sensor1);
				if (sensor1 > 1000) {
					RefSpeed0 = 12000 - (sensor1-1000)*30;
					if (RefSpeed0 & 0x80000000) {
						RefSpeed0 = 0;
					}
					RefSpeed1 = 12000;
				}else {
					RefSpeed0 = RefSpeed1 = 12000;
				}
			break;
			case UltraSonic:
				ST7735_Message(0,1,"ULS0: ", ((unsigned long *)canData)[0]);
			break;
			default:
			break;
		}
	}
}

#define INC_STEP  300
static long IncrementalController( long ref,  long curr) {
	if (curr > ref ) {
		if (curr > ref + INC_STEP ) {
		  return (curr-INC_STEP);
		} else {
			return ref;
		}
	} else {
		if (curr < ref - INC_STEP) {
			return (curr+INC_STEP);
		} else {
			return ref;
		}
	}
}

void Controller(void) {
	CurrentSpeed0 = IncrementalController(RefSpeed0, CurrentSpeed0);
	CurrentSpeed1 = IncrementalController(RefSpeed1, CurrentSpeed1);
	Motor_0_MotionUpdate(CurrentSpeed0, 1);
	Motor_1_MotionUpdate(CurrentSpeed1, 1);
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
  NumCreated += OS_AddPeriodicThread(&Controller, 100*TIME_1MS, 3); 
  
  OS_Launch(TIMESLICE);
}

#else 

void IRSensorSend(void) {	
	unsigned short IRvalues[4];
	unsigned long sonarValues[4];
	unsigned char CanData[4];
	
	IR_getValues(IRvalues);
	
	((unsigned short*)CanData)[0] = IRvalues[3];
	
	CAN0_SendData(IRSensor0, CanData);
	
//	Ping_getData (sonarValues);
//	((unsigned long*)CanData)[0] = sonarValues[0];
//	CAN0_SendData(UltraSonic, CanData);
}

int main(void) {
  PLL_Init();
  
  OS_Init();
	IR_Init();
	
	Debug_LED_Init();
	
  NumCreated = 0;
  //NumCreated += OS_AddThread(&Interpreter,128,1); 
  NumCreated += OS_AddPeriodicThread(&IRSensorSend, 100*TIME_1MS, 3); 
  CAN0_Open();
//	Ping_Init();
  OS_Launch(TIMESLICE);
}

#endif
