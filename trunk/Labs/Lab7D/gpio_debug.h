// debug.h
// Runs on TM4C123GH6PM
// debug instrument
// Nick Huang
// Jan 18, 2014


#ifndef __GPIO_DEBUG_H__
#define __GPIO_DEBUG_H__

#define R         0x02
#define B         0x04
#define G         0x08

#define Colorwheel_Size 8

typedef enum Colorwheel_t {
	RED=R,
	YELLOW=R+G,
	GREEN=G,
	VIOLET=G+B,
	BLUE=B,
	PURPLE=B+R,
	WHITE=R+G+B,
	OFF=0,	
} Colorwheel;


void Debug_PortE_Init(void);
// ***************** debug_LED_Init ****************
// Activate PF1-3 for LED
// Inputs:  
// Outputs: none
void Debug_LED_Init(void);

// ************** debug_LED_heartbeat **************
// Activate Flashes red
// Inputs:  none
// Outputs: none
void Debug_LED_heartbeat(void);

// ************** debug_LED_heartbeat **************
// Activate Flashes the color
// Inputs: color as defined above
// Outputs: none
void Debug_LED(Colorwheel color);

void Debug_SetGreen(int color);

void Debug_SetRed(int color);

void Debug_SetBlue(int color);
#endif
