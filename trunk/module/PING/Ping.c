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

#define Sensors 			    (*((volatile unsigned long *)0x4000503C))
#define PB3_0                   0x0F
#define Temperature				20 
#define NVIC_EN0_INT1			2

#define Ping_Gap                25 // in ms

Sema4Type	Sema4PingSampleAvailable, Sema4PingResultAvailable[4];

unsigned char PingNum=0;

static unsigned long LastStatus;
static unsigned long Starttime[4];
static unsigned long Finishtime[4];

static unsigned long Distance_Result[4];

static void Ping_measure(unsigned char number);

void Ping_Thread(void) {
	while(1) {
		Ping_measure(0);
		OS_Sleep(Ping_Gap);
		Ping_measure(1);
		OS_Sleep(Ping_Gap);
		Ping_measure(2);
		OS_Sleep(Ping_Gap);
		Ping_measure(3);
		OS_Sleep(Ping_Gap);
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
	
  OS_SemaphoreInit(&Sema4PingSampleAvailable, 0);
  OS_SemaphoreInit(&Sema4PingResultAvailable[0], 0);
  OS_SemaphoreInit(&Sema4PingResultAvailable[1], 0);
  OS_SemaphoreInit(&Sema4PingResultAvailable[2], 0);
  OS_SemaphoreInit(&Sema4PingResultAvailable[3], 0);
  OS_AddThread(Ping_Thread, 128, 1);
}

void PingValue(unsigned long &mbox, unsigned char pingNum) {
	OS_bWait(&Sema4PingResultAvailable[pingNum]);
	
	*mbox = Distance_Result[pingNum];
}

//Send pulse to four Ping))) sensors
//happens periodically by using timer
//foreground thread 
//Fs: about 40Hz
//no input and no output

// TODO! Decouple PingNum into a parameter

// Must ensure 

static void Ping_measure(unsigned char number){
	unsigned char delay_count;
	static unsigned char bitmask;
	unsigned long tin
	
	PingNum = number & 0x03;
	bitmask = 1 << PingNum;
	
	// Send pulse
	GPIO_PORTB_IM_R &= ~bitmask;
	GPIO_PORTB_DIR_R |= bitmask;
	
	Sensors |= bitmask;
	//blind-wait
	for(delay_count=0; delay_count<60; delay_count++);
	Sensors &= ~bitmask;
	
	GPIO_PORTB_DIR_R &= ~bitmask;
	GPIO_PORTB_IM_R |= bitmask;

	// Wait for response
	OS_bWait(&Sema4PingSampleAvailable);
		tin = OS_TimeDifference(Finishtime[PingNum],Starttime[PingNum]);
		Distance_Result[PingNum] = (tin*(3310+6*Temperature+5))/1600;
	OS_bSignal(&Sema4PingResultAvailable[PingNum]);
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
		
		OS_bSignal(&Sema4PingSampleAvailable);
	}

	GPIO_PORTB_ICR_R = PB3_0;
	
	LastStatus = CurrStatus;
}
