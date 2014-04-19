// Ping.c
// Runs on LM4C123
// Initialize Ping interface, then generate 5us pulse about 10 times per second
// capture input pulse and record pulse width 
// Miao Qi
// October 27, 2012

#include "inc/tm4c123gh6pm.h"
#include "OS.h"

#define Sensors 			    (*((volatile unsigned long *)0x4000503C))
#define PB3_0                   0x0F
#define Temperature				20 
#define NVIC_EN0_INT1			2

unsigned char PingNum=0;

static unsigned long LastStatus;
static unsigned long Starttime[4];
static unsigned long Finishtime[4];
static unsigned char Update_vect
static unsigned long Distance_Result[4];

//initialize PB4-0
//PB4 set as output to send 5us pulse to all four Ping))) sensors at same time
//PB3-0 set as input to capture input from sensors
void Ping_Init(void){
  /***************** New Style *********************/
  SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1;
  /*************************************************/
  
  LastStatus = 0;           // (b) initialize status
  
  GPIO_PORTB_DIR_R &= ~PB0_3;    // (c) make PB3-0 in
  GPIO_PORTB_AFSEL_R &= ~PB0_3;  //     disable alt funct on PB4-0
  GPIO_PORTB_DEN_R |= PB0_3;     //     enable digital I/O on PB4-0   
  GPIO_PORTB_PCTL_R &= ~0x000FFFFF; // configure PB4-0 as GPIO
  GPIO_PORTB_AMSEL_R =~PB0_3;    //     disable analog functionality on PB
  GPIO_PORTB_PDR_R |= PB0_3;     //     enable pull-down on PF4-0
  
  GPIO_PORTB_IS_R &= ~PB0_3;     // (d) PB3-0 is edge-sensitive
  GPIO_PORTB_IBE_R |= PB0_3;     //      PB3-0 is both edges
  GPIO_PORTB_ICR_R =  PB0_3;     // (e) clear flag3-0
  GPIO_PORTB_IM_R |= PB0_3;      // (f) arm interrupt on PB3-0
  
  NVIC_PRI0_R = (NVIC_PRI0_R&0xFFFF00FF)|0x00004000; // (g) priority 2
  NVIC_EN0_R |= NVIC_EN0_INT1;  // (h) enable interrupt 1 in NVIC
}

//Send pulse to four Ping))) sensors
//happens periodically by using timer
//foreground thread 
//Fs: about 40Hz
//no input and no output

// TODO! Decouple PingNum into a parameter

// Must ensure 

void Ping_pulse(unsigned char number){
	unsigned char delay_count;
	static unsigned char bitmask;
	
	PingNum = number & 0x03;
	bitmask = 1 << PingNum;
	
	GPIO_PORTB_IM_R &= ~bitmask;
	GPIO_PORTB_DIR_R |= bitmask;
	
	Sensors |= bitmask;
	//blind-wait
	for(delay_count=0; delay_count<60; delay_count++);
	Sensors &= ~bitmask;
	
	GPIO_PORTB_DIR_R &= ~bitmask;
	GPIO_PORTB_IM_R |= bitmask;

	
}

/* // Input: data_record is one roww out of a 4 by 4 buffer
static unsigned long median(unsigned long *data_record){
	unsigned long buffer[4];
	//compare the oldest two data
	if((data_record[0]<data_record[1])
		{buffer[0]=*data_record; buffer[1]=data_record[1];}		
	else
		{buffer[1]=*data_record; buffer[0]=data_record[1];}
	//compare the third data
	if(buffer[0]<data_record[2]){
		if(buffer[1]<data_record[2]){buffer[2]=data_record[2];}
		else{buffer[2]=buffer[1]; buffer[1]=data_record[2];}
	}
	else{buffer[2]=buffer[1]; buffer[1]=buffer[0];buffer[0]=data_record[2];}
	//compare the forth data
	//ingore the forth data when it is the laragest
	if(buffer[2]>data_record[3]){
			//ingore the forth data when it is the smallest
			if(buffer[0]>data_record[3]){buffer[2]=buffer[1];buffer[1]=buffer[0];}
			else{buffer[2]=data_record[3];}
	}
	
	return (buffer[1]+buffer[2])/2;
} */
	


//d=c* tIN/2
//d = c * tIN * 12.5ns /2 * (um/us)
//d = c * tIN * (1us/40) /(2*2) * (um/us)
//d = c * tIN / (40*2*2) * um
//ignore underflow
//+0.5: round
//return distance = ((tin/40)*(331+0.6*Temperature+0.5))/4;
//compute and update distance array for four sensors
//called when PORTB3-0 capture a value change
//output resolution um
void Distance(void){
	unsigned char i;
	unsigned char mask;
	if(!Update_vect) return;
	
	for (i=0, mask = 0x01; i<4; i++, mask <<= 1) {
		if(Update_vect & mask) {
			unsigned long tin = OS_TimeDifference(Finishtime[i],Starttime[i]);
		
			Distance_Result[i] = (tin*(3310+6*Temperature+5))/1600;
			
			Update_vect &=~mask;
		}
	}
}


//put inside PORTB_handler
//input system time, resolution: 12.5ns
//no output

void GPIOPortB_Handler(void){
	unsigned char mask = 1<<PingNum;
	unsigned long CurrStatus = Sensors;
	
	//check rising edge and record time	
		if(CurrStatus & ~LastStatus) {
			Starttime[PingNum] = OS_Time();
		}
	
	//check falling edge and record time
		else if(~CurrStatus & LastStatus) {
			Finishtime[PingNum] = OS_Time(); 
			
			Update_vect |= mask;
		}
	
	GPIO_PORTB_ICR_R = PB0_3;
	
	LastStatus = CurrStatus;
}
