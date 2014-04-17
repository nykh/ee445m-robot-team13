// IR_sensor.c

#include "ir_sensor.h"

#include "PLL.h"
#include "SysTick.h"
#include "OS.h"
#include "FIFO_sema4.h"
#include "semaphore.h"
#include "myadc.h"
#include "ST7735.h"
#include "interpreter.h"

#include "debug.h"

#define SAMPLING_RATE 2000
#define TIMESLICE 2*TIME_1MS  // thread switch time in system time units

Sema4Type	Sema4IRDataAvailable;

static unsigned short data[4];

/*****************************************************/

// Call back function passed to ADC
void  IRCallBack(unsigned short buf[]) {
	data[0] = buf[0];
	data[1] = buf[1];
	data[2] = buf[2];
	data[3] = buf[3];
	
	OS_bSignal(&Sema4IRDataAvailable);
}


void IR_Init(void) {
  OS_InitSemaphore(&Sema4IRDataAvailable, 0);
	
  myADC_Collect4(SAMPLING_RATE, IRCallBack, 64);
}

void IR_getValues (unsigned short *buffer) {
	OS_bWait(&Sema4IRDataAvailable);
	
	buffer[0] = data[0];
	buffer[1] = data[1];
	buffer[2] = data[2];
	buffer[3] = data[3];
}
