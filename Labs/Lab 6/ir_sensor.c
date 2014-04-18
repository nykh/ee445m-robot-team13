// IR_sensor.c

#include "ir_sensor.h"

#include "PLL.h"
#include "SysTick.h"
#include "OS.h"
#include "FIFO_sema4.h"
#include "semaphore.h"
#include "adc.h"
#include "ST7735.h"
#include "interpreter.h"

#include "debug.h"

#define SAMPLING_RATE 2000
#define TIMESLICE 2*TIME_1MS  // thread switch time in system time units

/********************** Data Structure *******************/
#define FIFOSIZE   64         // size of the FIFOs (must be power of 2)
#define FIFOSUCCESS 1         // return value on success
#define FIFOFAIL    0         // return value on failure
                              // create index implementation FIFO (see FIFO.h)
	long StartCritical(void);    // previous I bit, disable interrupts
	void EndCritical(long sr);    // restore I bit to previous value
	
AddIndexSema4Fifo(IR1, FIFOSIZE, unsigned short, FIFOSUCCESS, FIFOFAIL)
AddCallBackFunction(IR1)

#if LAB_DEMO == 7
AddIndexSema4Fifo(IR2, FIFOSIZE, unsigned short, FIFOSUCCESS, FIFOFAIL)
AddCallBackFunction(IR2)
AddIndexSema4Fifo(IR3, FIFOSIZE, unsigned short, FIFOSUCCESS, FIFOFAIL)
AddCallBackFunction(IR3)
AddIndexSema4Fifo(IR4, FIFOSIZE, unsigned short, FIFOSUCCESS, FIFOFAIL)
AddCallBackFunction(IR4)
#endif

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

static FilterType filter1 = {{0},FILTER_LENGTH-1};

#if LAB_DEMO == 7

static FilterType filter2;
filter2.index = FILTER_LENGTH-1; // to let pre-increment start at x[FILTER_LENGTH]
static FilterType filter3;
filter3.index = FILTER_LENGTH-1; // to let pre-increment start at x[FILTER_LENGTH]
static FilterType filter4;
filter4.index = FILTER_LENGTH-1; // to let pre-increment start at x[FILTER_LENGTH]

#endif

// Filter_ClearMemo
// this is called when one stops using the filter
// such that when filter is being used again the filter memory will be fresh
//static void Filter_ClearMemo(FilterType *f) {
//  unsigned char i;
//  for(i = 0; i < 2*FILTER_LENGTH; ++i) f->x[i] = 0;
//  f->index = FILTER_LENGTH-1; // to let pre-increment start at x[FILTER_LENGTH]
//}

// Filter
// Digital FIR filter, assuming fs=1 Hz
// Coefficients generated with FIRdesign64.xls
// 
// y[i]= (h[0]*x[i]+h[1]*x[i-1]+…+h[63]*x[i-63])/256;
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

/*****************************************************/

// Consumer
// Foreground thread that takes in data from FIFO, apply filter, and record data
// If trigger Capture is set, it will perform a 64-point FFT on the recorded data
// and store result in fft_output[]
// Block when the FIFO is empty
#define NOW_USING   1
#define STOP_USING  0
#define NOT_USING  -1
char Filter_Use = NOT_USING;
	
static void Consumer(void) {	
  ADC_Collect(3, SAMPLING_RATE, IR1CallBack, 64);

#if LAB_DEMO == 7
  ADC_Collect(1, SAMPLING_RATE, IR2CallBack, 64);
  ADC_Collect(2, SAMPLING_RATE, IR3CallBack, 64);
  ADC_Collect(3, SAMPLING_RATE, IR4CallBack, 64);
#endif

  while (1) {
		// Get data, will block if FIFO is empty
		unsigned short data;
		IR1Fifo_Get(&data);
			
		// Choosing whether to apply the filter
		IRsensor1 = Filter(&filter1, data);
		
		
	}
}


void IR_Init(void) {
  OS_InitSemaphore(&Sema4DataAvailable, 0);
  
  IR1Fifo_Init();
  
#if LAB_DEMO == 7
  IR2Fifo_Init();
  IR3Fifo_Init();
  IR4Fifo_Init();
#endif

  OS_AddThread(&Consumer, 256, 3);
}

void IR_getValues (unsigned short *buffer) {
	buffer[0] = IRsensor1;
	#if LAB_DEMO == 7
	buffer[1] = IRsensor2;
	buffer[2] = IRsensor3;
	buffer[3] = IRsensor4;
	#endif
}
