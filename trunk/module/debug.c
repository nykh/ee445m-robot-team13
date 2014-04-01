// debug.c
// Runs on TM4C123GH6PM
// debug instrument
// Nick Huang
// Jan 18, 2014

#include "inc/tm4c123gh6pm.h"
#include "debug.h"

#define PF1       (*((volatile unsigned long *)0x40025008))
#define PF2       (*((volatile unsigned long *)0x40025010))
#define PF3       (*((volatile unsigned long *)0x40025020))
#define LEDS      (*((volatile unsigned long *)0x40025038))

void Debug_LED_Init() { volatile unsigned long delay;
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOF; // activate port F
  delay = SYSCTL_RCGC2_R;          // allow time to finish activating
  GPIO_PORTF_DIR_R |= 0x0E;        // make PF3-1 output (PF3-1 built-in LEDs)
  GPIO_PORTF_AFSEL_R &= ~0x0E;     // disable alt funct on PF3-1
  GPIO_PORTF_DEN_R |= 0x0E;        // enable digital I/O on PF3-1
                                   // configure PF3-1 as GPIO
  GPIO_PORTF_PCTL_R = (GPIO_PORTF_PCTL_R&0xFFFFF0FF)+0x00000000;
  GPIO_PORTF_AMSEL_R = 0;          // disable analog functionality on PF
  LEDS = OFF;                        // turn all LEDs off
}

void Debug_PortE_Init(void){ unsigned long volatile delay;
  SYSCTL_RCGC2_R |= 0x10;       // activate port E
  delay = SYSCTL_RCGC2_R;        
  delay = SYSCTL_RCGC2_R;         
  GPIO_PORTE_DIR_R |= 0x3F;    // make PE3-0 output heartbeats
  GPIO_PORTE_AFSEL_R &= ~0x3F;   // disable alt funct on PE3-0
  GPIO_PORTE_DEN_R |= 0x3F;     // enable digital I/O on PE3-0
  GPIO_PORTE_PCTL_R = ~0x00FFFFFF;
  GPIO_PORTE_AMSEL_R &= ~0x3F;;      // disable analog functionality on PF
}

void Debug_LED_heartbeat() {
	LEDS ^= RED;
}

void Debug_LED(Colorwheel color) {
	LEDS = color;
}
