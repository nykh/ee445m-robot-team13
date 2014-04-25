// Ping.c
// Runs on LM4C123
// Initialize Ping interface, then generate 5us pulse about 10 times per second
// capture input pulse and record pulse width 
// Miao Qi
// October 27, 2012

// Modified
// Nicholas Huang
// 2014/4/19
// Use semaphore to synchronize interface
// Four sensors are sampled in turn (every 25 ms)

#include "inc/tm4c123gh6pm.h"
#include "OS.h"
#include "semaphore.h"

//#define Sensors 			    (*((volatile unsigned long *)0x4000503C))
//#define PB3_0             0x0F
#define Sensors 			    (*((volatile unsigned long *)0x4000500C))
#define PB3_0             0x03
#define Temperature				20 
#define NVIC_EN0_INT1			2

#define TimeGap           5 // in 10 ms

#define numSensor    2

Sema4Type	Sema4PingResultAvailable[numSensor], Sema4PingIdle;
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value

unsigned char PingNum=0;

static unsigned long LastStatus;
static unsigned long Starttime[numSensor];
static unsigned long Finishtime[numSensor];
static unsigned char Edge_Valid[numSensor] = {0,}; // flag

static unsigned char Distance_Result[numSensor];
static unsigned char Sensor_fail[numSensor] = {0,};

static void Ping_measure(unsigned char number);

void Ping_Thread(void) {
	while(1) {
		Ping_measure(0);
		Ping_measure(1);
//		Ping_measure(2);
//		Ping_measure(3);
	}
}

//initialize PB4-0
//PB4 set as output to send 5us pulse to all four Ping))) sensors at same time
//PB3-0 set as input to capture input from sensors
void Ping_Init(void){
  /***************** New Style *********************/
  SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1;
  /*************************************************/
  
  LastStatus = 0;           // (b) initialize status
  
  GPIO_PORTB_DIR_R &= ~PB3_0;    // (c) make PB3-0 in
  GPIO_PORTB_AFSEL_R &= ~PB3_0;  //     disable alt funct on PB4-0
  GPIO_PORTB_DEN_R |= PB3_0;     //     enable digital I/O on PB4-0   
  GPIO_PORTB_PCTL_R &= ~0x000FFFFF; // configure PB4-0 as GPIO
  GPIO_PORTB_AMSEL_R =~PB3_0;    //     disable analog functionality on PB
  GPIO_PORTB_PDR_R |= PB3_0;     //     enable pull-down on PF4-0
  
  GPIO_PORTB_IS_R &= ~PB3_0;     // (d) PB3-0 is edge-sensitive
  GPIO_PORTB_IBE_R |= PB3_0;     //      PB3-0 is both edges
  GPIO_PORTB_ICR_R =  PB3_0;     // (e) clear flag3-0
  GPIO_PORTB_IM_R |= PB3_0;      // (f) arm interrupt on PB3-0
  
  NVIC_PRI0_R = (NVIC_PRI0_R&0xFFFF00FF)|0x00004000; // (g) priority 2
  NVIC_EN0_R |= NVIC_EN0_INT1;  // (h) enable interrupt 1 in NVIC
	
  Edge_Valid[0] = Edge_Valid[1] /* = Edge_Valid[2] = Edge_Valid[3] */ = 0;
	
	OS_InitSemaphore(&Sema4PingIdle, 1);
	
  OS_InitSemaphore(&Sema4PingResultAvailable[0], 0);
  OS_InitSemaphore(&Sema4PingResultAvailable[1], 0);
//  OS_InitSemaphore(&Sema4PingResultAvailable[2], 0);
//  OS_InitSemaphore(&Sema4PingResultAvailable[3], 0);
	
  OS_AddThread(Ping_Thread, 128, 1);
}

unsigned char PingValue(unsigned char *mbox, unsigned char pingNum) {
	OS_bWait(&Sema4PingResultAvailable[pingNum]);
	
	*mbox = Distance_Result[pingNum];
	
	return Sensor_fail[pingNum];
}

//Send pulse to four Ping))) sensors
//happens periodically by using timer
//foreground thread 
//Fs: about 40Hz
//no input and no output

// TODO! Decouple PingNum into a parameter

// Must ensure 

static void Ping_measure(unsigned char number){
	long sr;
	unsigned char delay_count;
	static unsigned char bitmask;
	unsigned long tin;
	
	OS_bWait(&Sema4PingIdle);
	
	PingNum = number & 0x03;
	bitmask = 1 << PingNum;
	Edge_Valid[PingNum] = 0;
	
	// Send pulse
	GPIO_PORTB_IM_R &= ~bitmask;
	GPIO_PORTB_DIR_R |= bitmask;
	
	Sensors |= bitmask;
	//blind-wait
	for(delay_count=0; delay_count<100; delay_count++);
	Sensors &= ~bitmask;
	
	GPIO_PORTB_DIR_R &= ~bitmask;
	GPIO_PORTB_IM_R |= bitmask;
	
	OS_Sleep(TimeGap);
	
	sr = StartCritical();
	// Wait for response
	if(Edge_Valid[PingNum]) {
		unsigned long d;
		tin = OS_TimeDifference(Finishtime[PingNum],Starttime[PingNum]);
		d = ((tin*(3310+6*Temperature+5))/16000000); // cm
		if(d > 255) d = 255;
		Distance_Result[PingNum] = (unsigned char) d;
		Sensor_fail[PingNum] = 0;
	} else {
		Sensor_fail[PingNum] = 1;
	}
	EndCritical(sr);
	
	OS_bSignal(&Sema4PingResultAvailable[PingNum]);
	OS_bSignal(&Sema4PingIdle);
}

//put inside PORTB_handler
//input system time, resolution: 12.5ns
//no output

void GPIOPortB_Handler(void){
	unsigned long CurrStatus = Sensors;
	
	//check rising edge and record time	
	if(CurrStatus & ~LastStatus) {
		Starttime[PingNum] = OS_Time();
	}
	
	//check falling edge and record time
	else if(~CurrStatus & LastStatus) {
		Finishtime[PingNum] = OS_Time();
		
		Edge_Valid[PingNum] = 1;
	}

	GPIO_PORTB_ICR_R = PB3_0;
	
	LastStatus = CurrStatus;
}
