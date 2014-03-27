#include "PLL.h"
#include "SysTick.h"
#include "OS.h"
#include "semaphore.h"
#include "adc.h"
#include "ST7735.h"
#include "interpreter.h"

#include "debug.h"

#define SAMPLING_RATE 2000
#define TIMESLICE 2*TIME_1MS  // thread switch time in system time units

/************************ Debug info ***********************/
unsigned int NumCreated;

// Procedure 5
// Test the bandwidth limiting factor
long DataLost;

/********************** Public functions *******************/
void cr4_fft_64_stm32(void *pssOUT, void *pssIN, unsigned short Nbin);


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

// Newton's method
// s is an integer
// sqrt(s) is an integer
unsigned long sqrt(unsigned long s){
	unsigned long t;         // t*t will become s
	int n;                   // loop counter to make sure it stops running
  t = s/10+1;            // initial guess 
  for(n = 16; n; --n){   // guaranteed to finish
    t = ((t*t+s)/t)/2;  
  }
  return t; 
}


// Consumer
// Foreground thread that takes in data from FIFO, apply filter, and record data
// If trigger Capture is set, it will perform a 64-point FFT on the recorded data
// and store result in fft_output[]
// Block when the FIFO is empty
#define FFT_LENGTH 64

#define NOW_USING   1
#define STOP_USING  0
#define NOT_USING  -1
char Filter_Use = NOT_USING;

#define DO_CAPTURE  200
#define NO_CAPTURE  0
short Capture = NO_CAPTURE;

#define TIME      1
#define FREQUENCY 2
char DisplayMode = TIME;

#define NOW        0
#define CONTINUOUS 1
#define BUTTON     2
char TriggerMode = NOW;

unsigned long fft_output[FFT_LENGTH];

long recording[2*FFT_LENGTH];
unsigned char puti = 0;
unsigned char geti = 0;

void dummy (unsigned short a) {}
	
void Consumer(void) {	
  ADC_Collect(0, SAMPLING_RATE, dummy, 64);
  while (1) {
		// Get data, will block if FIFO is empty
		long data = OS_Fifo_Get();
			
		// Choosing whether to apply the filter
		if(Filter_Use == NOW_USING) data = Filter(data);
		else if(Filter_Use == STOP_USING) {
			Filter_Use = NOT_USING;
			Filter_ClearMemo();
		}
		
		// record data
		recording[puti] = recording[puti+FFT_LENGTH] = data;
		puti = (puti + 1) % FFT_LENGTH;
		
		if(puti == geti) { // a data point is just put in, so in this case it is overrun
		  DataLost += 1;
		  geti = (geti + 1) % FFT_LENGTH; // discard the oldest value
		} else {
			if (DisplayMode == TIME ) {			
				OS_Signal(&Sema4DataAvailable);
			}
		}
		
		if(Capture == DO_CAPTURE) {
			short real; long imag;
			int k;
			Capture = NO_CAPTURE; // Acknowledge
			cr4_fft_64_stm32(fft_output, &recording[puti-FFT_LENGTH+1], FFT_LENGTH);
			for (k=0;k<FFT_LENGTH;k++) {
				real = (short) (fft_output[k] & 0x0000FFFF);
				imag= fft_output[k] >> 16;
				fft_output[k] = sqrt((long)(real)*(long)(real)+imag*imag);
			}
			if (DisplayMode == FREQUENCY)
				OS_Signal(&Sema4DataAvailable);
		}
		
		if(TriggerMode == CONTINUOUS) ++Capture;
	}
}


/****** Background Button Task ***/
// In Button Triggering mode, it trigger the Consumer to perform one fft
void ButtonTask(void) {
	if(TriggerMode == BUTTON) Capture = DO_CAPTURE;
}



#undef DO_CAPTURE
#undef NO_CAPTURE

#undef NOW_USING
#undef STOP_USING
#undef NOT_USING

#undef NOW
#undef CONTINUOUS
#undef BUTTON

//************ Foreground Thread Display ****************
#define ON 1
#define OFF 0
char DisplayIsOn = ON;
void Display (void) {
	static char LastMode = FREQUENCY;
	while (1) {
		OS_Wait(&Sema4DataAvailable);
		if (DisplayMode == TIME) {		// DisplayMode is in time Domain
			if (LastMode != DisplayMode) {
				ST7735_FillScreen(0); // set screen to black
				ST7735_DrawFastVLine(19,10,100,ST7735_Color565(255,255,41));
				ST7735_DrawFastHLine(19,110,140,ST7735_Color565(255,255,41));
				ST7735_PlotClear(0,4095); 
			}	
			if (DisplayIsOn){
				ST7735_PlotPoint(recording[geti]);
				ST7735_PlotNext();
			}
			geti = (geti + 1) % FFT_LENGTH; 
			LastMode = TIME;
		} else if(DisplayMode == FREQUENCY) {	// DisplayMode is in frequency Domain
			int i;
			/* Display the frequency */
			if (LastMode != FREQUENCY) {
				ST7735_FillScreen(0); // set screen to black
				ST7735_DrawFastVLine(19,10,100,ST7735_Color565(255,255,41));
				ST7735_DrawFastHLine(19,110,140,ST7735_Color565(255,255,41));
				ST7735_PlotClear(0,4095); 
			}
			 ST7735_FillRect(21, 10, 140, 99, ST7735_BLACK);
			if (DisplayIsOn){
				ST7735_PlotReset();
				for (i=0;i<FFT_LENGTH;i++) {
					ST7735_PlotBar(fft_output[i]);
					ST7735_PlotNext();
				}
			}
			LastMode = FREQUENCY;
		}
	}
}
#undef ON
#undef OFF
#undef TIME
#undef FREQUENCY


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
  NumCreated += OS_AddThread(&Consumer, 256, 3);
  NumCreated += OS_AddThread(&Display,128,3); 
  NumCreated += OS_AddThread(&Interpreter,128,1); 
  
	NumCreated += OS_AddSW1Task(&ButtonTask, 1);
	
  //OS_AddSW1Task(&SW1Push,2);
  
  OS_Launch(TIMESLICE);
}
