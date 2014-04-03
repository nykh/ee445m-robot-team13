// Ping.c
// Runs on LM4C123
// Initialize Ping interface, then generate 5us pulse about 10 times per second
// capture input pulse and record pulse width 
// Miao Qi
// October 27, 2012

#include "inc/tm4c123gh6pm.h"
#include "OS.h"

#define PB4 						(*((volatile unsigned long *)0x40005040))
#define PB3_0 					(*((volatile unsigned long *)0x4000503C))
#define Temperature				20 
#define NVIC_EN0_INT1			2

unsigned long Ping_Lasttime[4];
unsigned long Ping_Finishtime[4];
unsigned char Ping_Update;
unsigned long Ping_Distance_Result[4];
unsigned long Ping_Distance_Filter[4][4];
//unsigned long Ping_Distance_cal[10];
unsigned long Ping_Index[4];
unsigned long Ping_laststatus;

//initialize PB4-0
//PB4 set as output to send 5us pulse to all four Ping))) sensors at same time
//PB3-0 set as input to capture input from sensors
void Ping_Init(void){
                                // (a) activate clock for port F
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOB;
  Ping_laststatus = 0;             // (b) initialize status
  GPIO_PORTB_DIR_R &= ~0x0F;    // (c) make PB3-0 in
  GPIO_PORTB_DIR_R |=  0x10;    // (c) make PB4 out
  GPIO_PORTB_AFSEL_R &= ~0x1F;  //     disable alt funct on PB4-0
  GPIO_PORTB_DEN_R |= 0x1F;     //     enable digital I/O on PB4-0   
  GPIO_PORTB_PCTL_R &= ~0x000FFFFF; // configure PB4-0 as GPIO
  GPIO_PORTB_AMSEL_R = 0;       //     disable analog functionality on PB
  GPIO_PORTB_PDR_R |= 0x1F;     //     enable pull-down on PF4-0
  GPIO_PORTB_IS_R &= ~0x0F;     // (d) PB3-0 is edge-sensitive
  GPIO_PORTB_IBE_R |= 0x0F;    //      PB3-0 is both edges
  GPIO_PORTB_ICR_R =  0x0F;      // (e) clear flag3-0
  GPIO_PORTB_IM_R |= 0x0F;      // (f) arm interrupt on PB3-0
  NVIC_PRI0_R = (NVIC_PRI0_R&0xFFFF00FF)|0x00004000; // (g) priority 2
  NVIC_EN0_R |= NVIC_EN0_INT1;  // (h) enable interrupt 1 in NVIC
}

extern unsigned char SendPulse;
extern unsigned long PulseCount;
//Send pulse to four Ping))) sensors
//happens periodically by using timer
//foreground thread 
//Fs: about 10Hz
//no input and no output

void Ping_pulse(void){
unsigned char delay_count;
	GPIO_PORTB_DEN_R |= 0x10;
	GPIO_PORTB_DEN_R &= ~0x0F;
	PB4 = 0x10;
	//blind-wait
	for(delay_count=0; delay_count<60; ){delay_count++;}
	PB4 = 0x00;
	GPIO_PORTB_DEN_R &= ~0x10;
	GPIO_PORTB_DEN_R |=  0x0F;
}

unsigned long median(unsigned long *data_record){
unsigned long buffer[4];
//compare the oldest two data
if((*data_record)<*(data_record+1))
	{buffer[0]=*data_record; buffer[1]=*(data_record+1);}		
else
	{buffer[1]=*data_record; buffer[0]=*(data_record+1);}
//compare the third data
if(buffer[0]<*(data_record+2)){
	if(buffer[1]<*(data_record+2)){buffer[2]=*(data_record+2);}
	else{buffer[2]=buffer[1]; buffer[1]=*(data_record+2);}
	}
else{buffer[2]=buffer[1]; buffer[1]=buffer[0];buffer[0]=*(data_record+2);}
//compare the forth data
//ingore the forth data when it is the laragest
if(buffer[2]>*(data_record+3)){
		//ingore the forth data when it is the smallest
		if(buffer[0]>*(data_record+3)){buffer[2]=buffer[1];buffer[1]=buffer[0];}}
		else{buffer[2]=*(data_record+3);}
return (buffer[1]+buffer[2])>>1;
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
unsigned char bits_I = 0;
unsigned long tin;
for (bits_I=0; bits_I<4;bits_I++)
	if(Ping_Update&(1<<bits_I))
		{tin = OS_TimeDifference(Ping_Finishtime[bits_I],Ping_Lasttime[bits_I]);
		tin = ((tin/40)*(331+0.6*Temperature+0.5))/4;
		Ping_Distance_Filter[bits_I][Ping_Index[bits_I]&0x3] = tin;
		Ping_Index[bits_I]++;
		Ping_Distance_Result[bits_I] = median(&Ping_Distance_Filter[bits_I][0]);
//		Ping_Distance_Result[bits_I] = tin;
		Ping_Update &= ~(1<<bits_I);
		}
}


//put inside PORTB_handler
//input system time, resolution: 12.5ns
//no output
//void GPIOPortB_Handler(void){
void Ping_measure(void){
unsigned char bits_I = 0;
unsigned long Ping_status;
 Ping_status	= PB3_0;
	//check rising edge and record time
for (bits_I=0; bits_I<4;bits_I++)
	{Ping_Lasttime[bits_I] = ((Ping_status&(1<<bits_I)) && ~(Ping_laststatus&(1<<bits_I)))? OS_Time():Ping_Lasttime[bits_I]; 
	 GPIO_PORTB_ICR_R = 1<<bits_I;}
//check falling edge and compute distance
for (bits_I=0; bits_I<4;bits_I++)
	{Ping_Finishtime[bits_I] = (~(Ping_status&(1<<bits_I)) && (Ping_laststatus&(1<<bits_I)))? OS_Time():Ping_Finishtime[bits_I]; 
	 GPIO_PORTB_ICR_R = 1<<bits_I;
	 Ping_Update |= 1<<bits_I;}
Ping_laststatus = Ping_status;
}
