// Ping.h
// Runs on LM4C123
// Initialize Ping interface, then generate 5us pulse about 10 times per second
// capture input pulse and record pulse width 
// Miao Qi
// October 27, 2012

//initialize PB4-0
//PB4 set as output to send 5us pulse to all four Ping))) sensors at same time
//PB3-0 set as input to capture input from sensors
void Ping_Init(void);

//Send pulse to four Ping))) sensors
//happens periodically by using timer
//added in interrupt handler as foreground thread 
// measure each of the four sensors in turn
//Fs: about 40Hz
//no input and no output
void Ping_pulse(void);

//put inside PORTB_handler
//input system time, resolution: 12.5ns
//no output
void Ping_measure(void);

//compute and update distance array for four sensors
//check the new data and update distance info
//runs in foreground
//output resolution um
void Distance(void);

//final distance data
extern unsigned long Ping_Distance_Result[4];


