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

static unsigned long Ping_Lasttime[4];
static unsigned long Ping_Finishtime[4];
static unsigned char Ping_Update;
static unsigned long Ping_Distance_Result[4];
static unsigned long Ping_Distance_Filter[4][4];
static unsigned long Ping_Index[4];

//initialize PB4-0
//PB4 set as output to send 5us pulse to all four Ping))) sensors at same time
//PB3-0 set as input to capture input from sensors
void Ping_Init(void){
  /***************** New Style *********************/
  SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1;
  /*************************************************/
  
  Ping_laststatus = 0;              // (b) initialize status
  
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

// TODO! Decouple PulseCount into a parameter

void Ping_pulse(void){
	static unsigned long PulseCount=0;
	unsigned char delay_count;
	static unsigned char mask = 0x01 << PulseCount;
	
	GPIO_PORTB_IM_R &= ~mask;
	GPIO_PORTB_DIR_R |= mask;
	
	Sensors |= mask;
	//blind-wait
	for(delay_count=0; delay_count<60; ){delay_count++;}
	Sensors &= ~mask;
	
	GPIO_PORTB_DIR_R &= ~mask;
	GPIO_PORTB_IM_R |= mask;

	PulseCount = (PulseCount + 1) % 4;
}

// Input: data_record is one roww out of a 4 by 4 buffer
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
}
	


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
	for (i=0, mask = 0x01; i<4; i++, mask <<= 1) {
		if(Ping_Update & mask) {
			unsigned long tin = OS_TimeDifference(Ping_Finishtime[i],Ping_Lasttime[i]);
			
			/**************** tuned *******************/
			tin = (tin*(3310+6*Temperature+5))/1600;
			
			Ping_Distance_Filter[i][Ping_Index[i]] = tin;
			Ping_Index[i] = (Ping_Index[i] + 1) % 0x03;
			
			Ping_Distance_Result[i] = median(&Ping_Distance_Filter[i][0]);
			
			Ping_Update &=~mask;
		}
	}
}


//put inside PORTB_handler
//input system time, resolution: 12.5ns
//no output
static unsigned long LastStatus;
void GPIOPortB_Handler(void){
	unsigned char i;
	unsigned char mask;
	unsigned long CurrStatus = Sensors;
	unsigned char change;
	
	//check rising edge and record time
	change = CurrStatus & ~LastStatus;
	for (i=0, mask=0x01; i<4; i++, mask <<= 1) {
		if(change & mask) {
			Ping_Lasttime[i] = OS_Time();
		}
	}
	
	//check falling edge and compute distance
	change = ~CurrStatus & LastStatus;
	for (i=0, mask=0x01; i<4; i++, mask <<= 1) {
		if(change & mask) {
			Ping_Finishtime[i] = OS_Time(); 
			Ping_Update |= mask;
		}
	}
	
	GPIO_PORTB_ICR_R = PB0_3;
	
	LastStatus = CurrStatus;
}
