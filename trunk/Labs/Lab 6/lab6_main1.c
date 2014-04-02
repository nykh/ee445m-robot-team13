#include "PLL.h"
#include "SysTick.h"
#include "OS.h"
#include "semaphore.h"
#include "adc.h"
#include "ST7735.h"
#include "interpreter.h"
#include "can0.h"

#include "gpio_debug.h"

#define SAMPLING_RATE 2000
#define TIMESLICE 2*TIME_1MS  // thread switch time in system time units

/************************ Debug info ***********************/
unsigned int NumCreated;

/********************** Public functions *******************/


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
				ST7735_Message(0,1,"ULS0: ", ((unsigned short *)canData)[0]);
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

  OS_InitSemaphore(&Sema4UART, 1);
	
  OS_Init();
  
  NumCreated = 0;
  //NumCreated += OS_AddThread(&Interpreter,128,1); 
  NumCreated += OS_AddThread(&NetworkReceive,128,1); 
  
  OS_Launch(TIMESLICE);
}
