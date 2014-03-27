// Ping.c
// Runs on LM4C123
// Initialize Ping interface, then generate 5us pulse about 10 times per second
// capture input pulse and record pulse width 
// Miao Qi
// October 27, 2012

#define PB4 					((volatile unsigned long *)0x40005040
#define PB3_0 					((volatile unsigned long *)0x4000503C
#define Temperature				20 

unsigned long Ping_Lasttime[4];
unsigned long Ping_Distance[4];
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
  NVIC_EN0_R = NVIC_EN0_INT1;  // (h) enable interrupt 1 in NVIC
}

//Send pulse to four Ping))) sensors
//happens periodically by using timer
//foreground thread 
//Fs: about 10Hz
//no input and no output
void Ping_pulse(void){
unsigned char delay_count;
	PB4 = 0x10;
	//usually 2 cycle per loop
	//blind-wait
	for(delay_count=0; delay_count<20; delay_count++){}
	PB4 = 0x00;
	OS_kill();
}

//d=c* tIN/2
//d = c * tIN * 12.5ns /2 * (um/us)
//d = c * tIN * (1us/40) /(2*2) * (um/us)
//d = c * tIN / (40*2*2) * um
//compute and update distance array for four sensors
//called when PORTB3-0 capture a value change
//output resolution um
unsigned long Distance(unsigned char SensorNum){
unsigned long distance;
unsigned long tin = OS_TimeDifference(OS_Time(),Ping_Lasttime[SensorNum]);
//ignore underflow
//+0.5: round
unsigned long distance = ((tin/40)*(331+0.6*Temperature+0.5))/4;
}

//put inside PORTB_handler
//input system time, resolution: 12.5ns
//no output
void Ping_measure(void){
unsigned char bits_I = 0;
unsigned long Ping_status = PB3_0;
//check rising edge and record time
for (bits_I=0; bitsI<4;bits_I++)
	{Ping_Lasttime[bits_I] = ((Ping_status&(1<<bits_I)) && ~(Ping_laststatus&(1<<bits_I)))? OS_Time():Ping_Lasttime[bits_I]; 
	 GPIO_PORTB_ICR_R = 1<<bitsI;}
//check falling edge and compute distance
for (bits_I=0; bitsI<4;bits_I++)
	{Ping_Distance[bits_I] = (~(Ping_status&(1<<bits_I)) && (Ping_laststatus&(1<<bits_I)))? Distance(bits_I):Ping_Distance[bits_I]; 
	 GPIO_PORTB_ICR_R = 1<<bitsI;}
Ping_laststatus = Ping_status;
}
