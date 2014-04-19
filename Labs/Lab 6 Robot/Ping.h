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


// Return: the number of times this sensor has failed
unsigned char PingValue(unsigned long *mbox, unsigned char pingNum);

//final distance data
extern unsigned long Ping_Distance_Result[4];


