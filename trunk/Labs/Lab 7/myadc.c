// ADCT0ATrigger.c
// Runs on LM4F120
// Provide a function that initializes Timer0A to trigger ADC
// SS3 conversions and request an interrupt when the conversion
// is complete.
// Daniel Valvano
// October 25, 2012

// April 2, 2014
// Modified
// ADC_Collect only calls the callback function for better flexibility and decoupling
// Nick Huang

// April 16, 2014
// Modified and simplified for the project. Use 4 sequencers.
// PE3-0 are ADC inputs each with one sequencer
// Nick Huang

// Ain0 is on PE3
// Ain1 is on PE2
// Ain2 is on PE1
// Ain3 is on PE0

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to Arm Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2012

 Copyright 2012 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */
 
#include "PLL.h"
#include "inc/tm4c123gh6pm.h"
#include "os.h"

#define NVIC_EN0_INT17          0x00020000  // Interrupt 17 enable

#define PE3_0_M                 0x0F

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

static void (*ADCTask) (unsigned short *);

#define ADC_NOT_DONE  	0
#define ADC_DONE 				1
static unsigned char ADC_Collect_Flag = ADC_NOT_DONE;


#define ADC_SW_TRIGGER    0
#define ADC_TIMER_TRIGGER 1
static void ADCInit(unsigned char channelNum, unsigned char mode, unsigned char prescale, unsigned short period);


// There are many choices to make when using the ADC, and many
// different combinations of settings will all do basically the
// same thing.  For simplicity, this function makes some choices
// for you.  When calling this function, be sure that it does
// not conflict with any other software that may be running on
// the microcontroller.  Particularly, ADC0 sample sequencer 3
// is used here because it only takes one sample, and only one
// sample is absolutely needed.  Sample sequencer 3 generates a
// raw interrupt when the conversion is complete, and it is then
// promoted to an ADC0 controller interrupt.  Hardware Timer0A
// triggers the ADC0 conversion at the programmed interval, and
// software handles the interrupt to process the measurement
// when it is complete.
//
// A simpler approach would be to use software to trigger the
// ADC0 conversion, wait for it to complete, and then process the
// measurement.
//
// This initialization function sets up the ADC according to the
// following parameters.  Any parameters not explicitly listed
// below are not modified:
// Timer0A: enabled
// Mode: 16-bit, down counting
// One-shot or periodic: periodic
// Prescale value: programmable using variable 'prescale' [0:255]
// Interval value: programmable using variable 'period' [0:65535]
// Sample time is busPeriod*(prescale+1)*(period+1)
// Max sample rate: <=125,000 samples/second
// Sequencer 0 priority: 1st (highest)
// Sequencer 1 priority: 2nd
// Sequencer 2 priority: 3rd
// Sequencer 3 priority: 4th (lowest)
// SS3 triggering event: Timer0A
// SS3 1st sample source: programmable using variable 'channelNum' [0:11]
// SS3 interrupts: enabled and promoted to controller

static unsigned int CollectNumberOfSamples;
static unsigned int SamplesCollected;


//------------ADC_Collect------------
// Starts the collection of multiple ADC samples at a certain frequency
// ADC Doesn't need to be initialized
// Input: channelNum should be between 0 and 11
//				fs is the sampling frequency
//				buffer is the array that will hold the results
//				User has to make sure that numberOfSamples is within the capacity of buffer
// Output: None
void myADC_Collect4(unsigned int fs, void  (*task) (unsigned short *), unsigned int numberOfSamples){
	long sr;
	
	// calculate period and presclae based on fs
	int N = 4e8 / (fs * PLL_getDevisor() ) ; // fs /*100-10000 Hz*/; /* 1e3-1e7 */
	int prescale = N / 65535;
	int period = ( (N + ((prescale+1)/2))/(prescale+1)  )- 1;
		
	// raised semaphore
	ADC_Collect_Flag = ADC_NOT_DONE;
	ADCTask = task;
	CollectNumberOfSamples = numberOfSamples;
	SamplesCollected = 0;
	
    sr = StartCritical();
	//Call ADC Init to initialize the timer and ADC
	myADCInit(prescale, period);
    EndCritical(sr);
}

#if 0 // Deprecated
//------------ADC_Open------------
// Initializes ADC for software trigerred sampling
// Input: channelNum should be between 0 and 11
//				If ADC is currently collecting periodic samples, it doesn't initialize
// Output: None
void ADC_Open(unsigned char channelNum){
	ADCInit(channelNum, ADC_SW_TRIGGER, 0, 0);
}

#endif 

//------------ADCInit------------
// Private function that will initialize ADC or timer
// Input: channelNum should be between 0 and 11
//				mode can be 0 (software trigger) or 1 (periodic)
//				prescale and period are used in case of periodic sampling
// Output: None
static void myADCInit(unsigned char prescale, unsigned short period) {
	volatile unsigned long delay;
	/********************** New style *******************************/	
	SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R4;
	delay = SYSCTL_RCGCGPIO_R;      // 2) allow time for clock to stabilize
	delay = SYSCTL_RCGCGPIO_R;
	delay = SYSCTL_RCGCGPIO_R;
	delay = SYSCTL_RCGCGPIO_R;
	delay = SYSCTL_RCGCGPIO_R;
	delay = SYSCTL_RCGCGPIO_R;
	/****************************************************************/
	
	GPIO_PORTE_DIR_R &= ~PE3_0_M;  // 3.0) make PE3-0 input
    GPIO_PORTE_AFSEL_R |= PE3_0_M; // 4.0) enable alternate function on PE3-0
    GPIO_PORTE_DEN_R &= ~PE3_0_M;  // 5.0) disable digital I/O on PE3-0
    GPIO_PORTE_AMSEL_R |= PE3_0_M; // 6.0) enable analog functionality on PE3-0
	
	// Timer Trigger
	/********************** New style *******************************/ 		
	SYSCTL_RCGCADC_R |= SYSCTL_RCGCADC_R0;   // 7) activate ADC0 (legacy code)
	delay = SYSCTL_RCGCADC_R;         // 8) allow time for clock to stabilize
	delay = SYSCTL_RCGCADC_R;
	delay = SYSCTL_RCGCADC_R;
	delay = SYSCTL_RCGCADC_R;
	delay = SYSCTL_RCGCADC_R;
	delay = SYSCTL_RCGCADC_R;
	/****************************************************************/
	
	ADC0_PC_R &= ~0xF;                  // 9) clear max sample rate field
	ADC0_PC_R |= 0x1;                    //    configure for 125K samples/sec
	ADC0_SSPRI_R = 0x3210;              // 10) Sequencer 3 is lowest priority
	ADC0_ACTSS_R &= ~ADC_ACTSS_ASEN2;   // 11) disable sample sequencer 2
	
	/********************** New style *******************************/ 
	SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R0;    // activate timer0 (legacy code
	delay = SYSCTL_RCGCTIMER_R;                   // allow time to finish activating
	delay = SYSCTL_RCGCTIMER_R;
	delay = SYSCTL_RCGCTIMER_R;
	delay = SYSCTL_RCGCTIMER_R;
	/****************************************************************/
	
	TIMER0_CTL_R &= ~TIMER_CTL_TAEN;          // disable timer0A during setup
	TIMER0_CTL_R |= TIMER_CTL_TAOTE;          // enable timer0A trigger to ADC
	TIMER0_CFG_R = TIMER_CFG_16_BIT;          // configure for 16-bit timer mode
	// **** timer0A initialization ****
	TIMER0_TAMR_R = TIMER_TAMR_TAMR_PERIOD;   // configure for periodic mode, default down-count settings
	TIMER0_TAPR_R = prescale;                 // prescale value for trigger
	TIMER0_TAILR_R = period;                  // start value for trigger
	TIMER0_IMR_R &= ~TIMER_IMR_TATOIM;        // disable timeout (rollover) interrupt
	TIMER0_CTL_R |= TIMER_CTL_TAEN;           // enable timer0A 16-b, periodic, no interrupts
	
	// **** ADC initialization ****
	ADC0_EMUX_R &= ~ADC_EMUX_EM2_M; 
	// clear SS3 trigger select field
	ADC0_EMUX_R |= ADC_EMUX_EM2_TIMER;
	// configure for timer trigger event
	ADC0_SSMUX2_R = ADC0_SSMUX2_R &~0xFFFF | 0x3210;
	// clear SS3 1st sample input select field
	// configure for 'channelNum' as first sample input
	ADC0_SSCTL2_R = (0                        // settings for 1st sample:
					 & ~ADC_SSCTL2_TS0        // read pin specified by ADC0_SSMUX2_R
					 | ADC_SSCTL2_IE0         // raw interrupt asserted here
					 | ADC_SSCTL2_END0        // sample is end of sequence (hardwired)
					 & ~ADC_SSCTL2_D0);       // differential mode not used
	ADC0_IM_R |= ADC_IM_MASK2;                // enable SS2 interrupts
	ADC0_ACTSS_R |= ADC_ACTSS_ASEN2;          // enable sample sequencer 3
	// **** interrupt initialization ****     // ADC2=priority 2
	NVIC_PRI4_R = (NVIC_PRI4_R&0xFFFFFF00)|(2<<5); // bits 5-7
	NVIC_EN0_R = NVIC_EN0_INT16;              // enable interrupt 16 in NVIC
}

#if 0 // Deprecated
//------------ADC_In------------
// Public function that will sample one instance of ADC
// Input: None
// 				ADC Must be initialized before using ADC_Open
//				If ADC is collecting samples periodically, the function return 0xFFFF
// Output: None
unsigned short ADC_In(void){  unsigned short result;
  ADC1_PSSI_R = 0x0008;            // 1) initiate SS3
  while((ADC1_RIS_R&0x08)==0){};   // 2) wait for conversion done
  result = ADC1_SSFIFO3_R&0xFFF;   // 3) read result
  ADC1_ISC_R = 0x0008;             // 4) acknowledge completion
  return result;
}

//------------ADC_Status------------
// Public function that will return the status of periodic sampling
// Input: None
// Output: returns 0 if collecting is done or never started, 1 if ADC is currently sampling
unsigned char ADC_Status(void) {
	return ADC_Collect_Flag;
}

#endif

//------------ADC0Seq2_Handler------------
// ADC interrupt handler for periodic sampling
// It will put the new data is the data array
void ADC0Seq2_Handler(void){
  unsigned short ADCvalue[4];
  ADC0_ISC_R = ADC_ISC_IN2;                 // acknowledge ADC sequence 3 completion
  ADCvalue[0] = ADC0_SSFIFO2_R&ADC_SSFIFO3_DATA_M;
  ADCvalue[1] = ADC0_SSFIFO2_R&ADC_SSFIFO3_DATA_M;
  ADCvalue[2] = ADC0_SSFIFO2_R&ADC_SSFIFO3_DATA_M;
  ADCvalue[3] = ADC0_SSFIFO2_R&ADC_SSFIFO3_DATA_M;
  
  ADCTask(ADCvalue);
}
