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

void Blink(void) {
	Debug_LED_heartbeat();
}

#define PF4                     (*((volatile unsigned long *)0x40025040)) // SW1
#define PF0                     (*((volatile unsigned long *)0x40025004)) // SW2

void MessageSend(void){
	unsigned char data[4];
	data[0] = PF4;
	data[1] = PF0;
	CAN0_SendData(data);
}

int main(void) {
  PLL_Init();
  
  ST7735_InitR(INITR_REDTAB);
	ST7735_SetRotation(1);
  ST7735_FillScreen(0); // set screen to black

  OS_InitSemaphore(&Sema4UART, 1);
	OS_InitSemaphore(&Sema4FFTReady, 0);
	OS_InitSemaphore(&Sema4DataAvailable, 0);
	
	
  OS_Init();
  OS_Fifo_Init(128);
  
  NumCreated = 0;
  NumCreated += OS_AddThread(&Interpreter,128,1); 
  NumCreated += OS_AddThread(&NetworkReceive,128,2); 
	
	OS_AddPeriodicThread(&Blink, TIME_1MS*1000, 3);
  OS_AddPeriodicThread(&MessageSend, TIME_1MS*1000, 3);
  
  OS_Launch(TIMESLICE);
}
