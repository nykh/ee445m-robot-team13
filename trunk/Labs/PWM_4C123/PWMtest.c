// PWMtest.c
// Runs on TM4C123
// Use PWM0/PB6 to generate a 1000 Hz square wave with 50% duty cycle.
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
 
#include "PLL.h"
#include "PWM.h"

void WaitForInterrupt(void);  // low power mode

int main(void){
  PLL_Init();                      // bus clock at 50 MHz
  PWM0_Init(25000, 22500);         // initialize PWM0, 1000 Hz, 50% duty
//  PWM0_Duty(2500);    // 10%
//  PWM0_Duty(11250);   // 45%
//  PWM0_Duty(20000);   // 80%
	
	
	
	PWM0_1_MotionUpdate(0,1);
	//right
	PWM0_0_MotionUpdate(0,1);
	//left
	
//  PWM0_Init(2500, 1250);         // initialize PWM0, 10000 Hz, 50% duty
//  PWM0_Init(1000, 900);          // initialize PWM0, 25000 Hz, 90% duty
//  PWM0_Init(1000, 100);          // initialize PWM0, 25000 Hz, 10% duty
//  PWM0_Init(3, 2);               // initialize PWM0, 8.333 MHz, 66% duty
  while(1){
    WaitForInterrupt();
  }
}
