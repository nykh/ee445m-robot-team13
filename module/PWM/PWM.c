// PWM.c
// Runs on TM4C123
// Use PWM0/PB6 to generate pulse-width modulated outputs.
// Daniel Valvano
// September 3, 2013

/* This example accompanies the book
   "Embedded Systems: Real Time Interfacing to ARM Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2013
  Program 6.7, section 6.3.2

 Copyright 2013 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains
 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */
#include "../inc/tm4c123gh6pm.h"
#include "PWM.h"

#define PB7      (*((volatile unsigned long *)0x40005200))
#define PB5      (*((volatile unsigned long *)0x40005080))

// period is 16-bit number of PWM clock cycles in one period (3<=period)
// duty is number of PWM clock cycles output is high  (2<=duty<=period-1)
// PWM clock rate = processor clock rate/SYSCTL_RCC_PWMDIV
//                = BusClock/2 
//                = 50 MHz/2 = 25 MHz (in this example)
void PWM0_Init(unsigned short period, unsigned short duty){
  volatile unsigned long delay;
  /*************** new style ******************/
	SYSCTL_RCGCPWM_R |= SYSCTL_RCGCPWM_R0;   // 1) activate PWM0
  SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R1; // 2) activate port B
  delay = SYSCTL_RCGCGPIO_R;               // allow time to finish activating
	/********************************************/
  GPIO_PORTB_AFSEL_R |= 0x50;           // enable alt funct on PB4,6
	GPIO_PORTB_AFSEL_R &= ~0xA0;					// disable alt funct on PB5,7
	GPIO_PORTB_DIR_R	|= 0xA0;						// set PB5,7 output
  GPIO_PORTB_PCTL_R &= ~0x0F0F0000;     // configure PB6,7 as PWM0
  GPIO_PORTB_PCTL_R |= 0x04040000;
  GPIO_PORTB_AMSEL_R &= ~0xF0;          // disable analog functionality on PB4,5,6,7
	
  GPIO_PORTB_DEN_R |= 0xF0;             // enable digital I/O on PB4,5,6,7
  SYSCTL_RCC_R |= SYSCTL_RCC_USEPWMDIV; // 3) use PWM divider
  SYSCTL_RCC_R &= ~SYSCTL_RCC_PWMDIV_M; //    clear PWM divider field
  SYSCTL_RCC_R += SYSCTL_RCC_PWMDIV_2;  //    configure for /2 divider
  PWM0_0_CTL_R = 0;                     // 4) re-loading mode
	PWM0_1_CTL_R = 0;            //!!         // 4) re-loading mode
  PWM0_0_GENA_R = (PWM_0_GENA_ACTCMPAD_ONE|PWM_0_GENA_ACTLOAD_ZERO);
	PWM0_1_GENA_R = (PWM_0_GENA_ACTCMPAD_ONE|PWM_0_GENA_ACTLOAD_ZERO);
  PWM0_0_LOAD_R = period - 1;           // 15) cycles needed to count down to 0
	PWM0_1_LOAD_R = period - 1;  //!!
  PWM0_0_CMPA_R = duty - 1;             // 6) count value when output rises
	PWM0_1_CMPA_R = duty - 1;    //!!
  PWM0_0_CTL_R |= PWM_0_CTL_ENABLE;     // 7) start PWM0
	PWM0_1_CTL_R |= PWM_1_CTL_ENABLE;  //!!
  PWM0_ENABLE_R |= (PWM_ENABLE_PWM0EN | PWM_ENABLE_PWM2EN);   // enable PWM0
}
// change duty cycle
// duty is number of PWM clock cycles output is high  (2<=duty<=period-1)

void PWM0_0_Duty(int direc, unsigned short speed){
	if(direc){
		PWM0_0_CMPA_R = speed - 1;             // 6) count value when output rises
	}
	else{
		PWM0_0_CMPA_R = PWM0_0_LOAD_R - speed - 1;             // 6) count value when output rises
	}
}

//change direction
void PWM0_0_Direction(int direc){
	if(direc){
		GPIO_PORTB_DATA_R |= 0x10;		// PB4 is direction for left wheel
	}
	else{
		GPIO_PORTB_DATA_R &= ~0x10;
	}
}


//PWM0=PB6,	DIR=PB7	
//PWM2=PB4, DIR=PB5

//for left wheel
void PWM0_0_MotionUpdate(unsigned long speed, unsigned long direction){
	
	if(direction){
		PB7 = 0x80;		// PB4 is direction for left wheel
		PWM0_0_CMPA_R = speed - 1;             // 6) count value when output rises(direction, speed);
	}
	else{
		PB7 = 0;
		PWM0_0_CMPA_R = PWM0_0_LOAD_R - speed - 1;             // 6) count value when output rises
	}
}

//for right wheel
void PWM0_1_MotionUpdate(unsigned long speed, unsigned long direction){
	
	if(direction){
		PB5 = 0x20;		// PB5 is direction for right wheel
		PWM0_1_CMPA_R = speed - 1;             // 6) count value when output rises(direction, speed);
	}
	else{
		PB5 = 0;
		PWM0_1_CMPA_R = PWM0_1_LOAD_R - speed - 1;             // 6) count value when output rises
	}
}
