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

#define FILTER_LENGTH 51
const long ScaleFactor = 16384;
const long H[51]={-11,10,9,-5,1,0,-19,6,48,-12,-92,
     17,155,-20,-243,22,370,-24,-559,24,881,-24,-1584,24,4932,
     8578,4932,24,-1584,-24,881,24,-559,-24,370,22,-243,-20,155,
     17,-92,-12,48,6,-19,0,1,-5,9,10,-11};


long x[2*FILTER_LENGTH]; // this MACQ needs twice the size of FILTER_LENGTH
unsigned char n = FILTER_LENGTH-1; // to let pre-increment start at x[FILTER_LENGTH]


// Filter_ClearMemo
// this is called when one stops using the filter
// such that when filter is being used again the filter memory will be fresh
void Filter_ClearMemo(void) {
  unsigned char i;
  for(i = 0; i < 2*FILTER_LENGTH; ++i) x[i] = 0;
  n = FILTER_LENGTH-1; // to let pre-increment start at x[FILTER_LENGTH]
}

// Filter
// Digital FIR filter, assuming fs=1 Hz
// Coefficients generated with FIRdesign64.xls
// 
// y[i]= (h[0]*x[i]+h[1]*x[i-1]+…+h[63]*x[i-63])/256;
long Filter(long data) {
  long y = 0;
  unsigned char i;
  
  if(++n == 2*FILTER_LENGTH) n = FILTER_LENGTH;
  x[n] = x[n-FILTER_LENGTH] = data;
  
  // Assuming there is no overflow
  for(i = 0; i < FILTER_LENGTH; ++i){
		y += H[i]*x[n-i];
	}
  y /= ScaleFactor;
  
  return y;
}

void Consumer(void) {
	unsigned char canData[4];
	Debug_LED_Init();
	CAN0_Open();
	while (1) {
		CAN0_GetMail(canData);
		(canData[0])?Debug_SetGreen(1):Debug_SetGreen(0);
		(canData[1])?Debug_SetBlue(1):Debug_SetBlue(0);
	}
}

void Blink(void) {
	Debug_LED_heartbeat();
}

#define PF4                     (*((volatile unsigned long *)0x40025040)) // SW1
#define PF0                     (*((volatile unsigned long *)0x40025004)) // SW2

void SW1Push(void){
	unsigned char data[4];
	data[0] = PF4;
	data[1] = PF0;
	CAN0_SendData(data);
}

int main(void) {
  PLL_Init();
  
  //ST7735_InitR(INITR_REDTAB);
	//ST7735_SetRotation(1);
  //ST7735_FillScreen(0); // set screen to black

  OS_InitSemaphore(&Sema4UART, 1);
	OS_InitSemaphore(&Sema4FFTReady, 0);
	OS_InitSemaphore(&Sema4DataAvailable, 0);
	
	
  OS_Init();
  OS_Fifo_Init(128);
  
  NumCreated = 0;
  NumCreated += OS_AddThread(&Interpreter,128,1); 
  NumCreated += OS_AddThread(&Consumer,128,2); 
	
	OS_AddPeriodicThread(&Blink, TIME_1MS*1000, 3);
  OS_AddSW1Task(&SW1Push,2);
  
  OS_Launch(TIMESLICE);
}
