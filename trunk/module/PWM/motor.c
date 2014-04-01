// motor.c
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
#include <tm4c123gh6pm.h>

#define PB4                     (*((volatile unsigned long *)0x40005040))

// period is 16-bit number of PWM clock cycles in one period (3<=period)
// duty is number of PWM clock cycles output is high  (2<=duty<=period-1)
// PWM clock rate = processor clock rate/SYSCTL_RCC_PWMDIV
//                = BusClock/2 
//                = 50 MHz/2 = 25 MHz (in this example)
void Motor_Init(unsigned short period){   volatile unsigned long delay;
  SYSCTL_RCGC0_R |= SYSCTL_RCGC0_PWM0;  // 1) activate PWM0
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOB; // 2) activate port B
  delay = SYSCTL_RCGC2_R;               // allow time to finish activating
  GPIO_PORTB_AFSEL_R |= 0xC0;           // enable alt funct on PB6,7
  GPIO_PORTB_AFSEL_R &= ~0x30;			// disable alt funct on PB4,5
  GPIO_PORTB_PCTL_R &= ~0xFF000000;     // configure PB6,7 as PWM0
  GPIO_PORTB_PCTL_R |= 0x44000000;
  GPIO_PORTB_AMSEL_R &= ~0xF0;          // disable analog functionality on PB4,5,6,7
  GPIO_PORTB_DEN_R |= 0xF0;             // enable digital I/O on PB4,5,6,7
  SYSCTL_RCC_R |= SYSCTL_RCC_USEPWMDIV; // 3) use PWM divider
  SYSCTL_RCC_R &= ~SYSCTL_RCC_PWMDIV_M; //    clear PWM divider field
  SYSCTL_RCC_R += SYSCTL_RCC_PWMDIV_2;  //    configure for /2 divider
  PWM0_0_CTL_R = 0;                     // 4) re-loading mode
  PWM0_0_GENA_R = (PWM_0_GENA_ACTCMPAD_ONE|PWM_0_GENA_ACTLOAD_ZERO);
  PWM0_0_LOAD_R = period - 1;           // 5) cycles needed to count down to 0
  PWM0_0_CMPA_R = 0;                    // 6) count value when output rises
  PWM0_0_CTL_R |= PWM_0_CTL_ENABLE;     // 7) start PWM0
  PWM0_ENABLE_R |= PWM_ENABLE_PWM0EN;   // enable PWM0
}
// change duty cycle
// duty is number of PWM clock cycles output is high  (2<=duty<=period-1)

void Motor_0_MotionUpdate(unsigned long duty, unsigned char direction){
	PWM0_0_CMPA_R = duty - 1;           // 6) count value when output rises
	PB4 = (direction != 0)<<4;          // PB4 controls the direction of one wheel
}

void Motor_0_Duty(unsigned short duty){
  PWM0_0_CMPA_R = duty - 1;             // 6) count value when output rises
}

//change direction
void Motor_0_Direction(unsigned char direction){
	PB4 = (direction != 0)<<4;          // PB4 controls the direction of one wheel
}
