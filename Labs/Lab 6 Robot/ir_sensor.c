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

#include "gpio_debug.h"

#define SAMPLING_RATE         2000 // in unit of Hz
#define TIMESLICE 2*TIME_1MS  // thread switch time in system time units

static unsigned short data[4];

/*****************************************************/


/*****************************************************/

/*********************** Filter ****************************/
#define FILTER_LENGTH 51
const long ScaleFactor = 16384;
const long H[51]={-11,10,9,-5,1,0,-19,6,48,-12,-92,
     17,155,-20,-243,22,370,-24,-559,24,881,-24,-1584,24,4932,
     8578,4932,24,-1584,-24,881,24,-559,-24,370,22,-243,-20,155,
     17,-92,-12,48,6,-19,0,1,-5,9,10,-11};

typedef struct {
	// this MACQ needs twice the size of FILTER_LENGTH
	long x[2*FILTER_LENGTH];
	unsigned char index;
} FilterType;

static unsigned short IRsensor1;

static FilterType filter0 = {{0},FILTER_LENGTH-1};
static FilterType filter1 = {{0},FILTER_LENGTH-1};
static FilterType filter2 = {{0},FILTER_LENGTH-1};
static FilterType filter3 = {{0},FILTER_LENGTH-1};

// Filter
// Digital FIR filter, assuming fs=1 Hz
// Coefficients generated with FIRdesign64.xls
// 
// y[i]= (h[0]*x[i]+h[1]*x[i-1]+¡K+h[63]*x[i-63])/256;
static unsigned short Filter(FilterType *f, unsigned short data) {
  long y = 0;
  unsigned char i;
  
  if(++f->index == 2*FILTER_LENGTH) f->index = FILTER_LENGTH;
  f->x[f->index] = f->x[f->index-FILTER_LENGTH] = data;
  
  // Assuming there is no overflow
  for(i = 0; i < FILTER_LENGTH; ++i){
		y += H[i]*f->x[f->index-i];
	}
  y /= ScaleFactor;
  
  return y;
}

/********************* Calibration detail ******************/
#define Calibtable_len       13
static const short Calitable[Calibtable_len+1] = {
	2930,
	2020,
	1600,
	1300,
	1110,
	980,
	860,
	780,
	720,
	660,
	620,
	560,
	540,
	
	-1, // min
};

// Calidiff[i] = Calitable[i] - Calitable[i+1]
static const short Calidiff[Calibtable_len-1] = {
	910,
	420,
	300,
	190,
	130,
	120,
	80,
	60,
	60,
	40,
	60,
	20,

};

static unsigned char calibrate(unsigned short adcval) {
	int i;
	
	for(i = 0; adcval <= Calitable[i] ; i++);
	// now, C[i] < adcval <= C[i+1]
	
	// saturation
	if (i == 0) return 10;
	else if(i == Calibtable_len) return 70;
	else return (unsigned char) (10+5*(i-1) + 5*(adcval - Calitable[i])/(Calidiff[i-1]));
}



// Call back function passed to ADC
void  IRCallBack(unsigned short buf[]) {
	
	data[0] = buf[0];
	data[1] = buf[1];
	data[2] = buf[2];
	data[3] = buf[3];
}


void IR_Init(void) {
	
  myADC_Collect4(SAMPLING_RATE, IRCallBack);
}

void IR_getValues (unsigned char *buffer) {
	
	buffer[0] = calibrate(Filter(&filter0, data[0]));
	buffer[1] = calibrate(Filter(&filter1, data[1]));
	buffer[2] = calibrate(Filter(&filter2, data[2]));
	buffer[3] = calibrate(Filter(&filter3, data[3]));
}
