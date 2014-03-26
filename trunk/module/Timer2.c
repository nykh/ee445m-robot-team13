// Timer2A.c
// Runs on LM4F120
// Use Timer0A in periodic mode to request interrupts at a particular
// period.
// Daniel Valvano
// May 13, 2013

/* This example accompanies the book
   "Embedded Systems: Introduction to ARM Cortex M Microcontrollers"
   ISBN: 978-1469998749, Jonathan Valvano, copyright (c) 2013
   Volume 1, Program 9.8

  "Embedded Systems: Real Time Interfacing to ARM Cortex M Microcontrollers",
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2013
   Volume 2, Program 7.5, example 7.6

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

#include "inc/tm4c123gh6pm.h"

#define NVIC_EN0_INT23          0x00800000  // Interrupt 23 enable

#define TIMER_CFG_16_BIT        0x00000004  // 16-bit timer configuration,
                                            // function is controlled by bits
                                            // 1:0 of GPTMTAMR and GPTMTBMR
#define TIMER_TAMR_TACDIR       0x00000010  // GPTM Timer A Count Direction
#define TIMER_TAMR_TAMR_PERIOD  0x00000002  // Periodic Timer mode
#define TIMER_CTL_TAEN          0x00000001  // GPTM TimerA Enable
#define TIMER_IMR_TATOIM        0x00000001  // GPTM TimerA Time-Out Interrupt
                                            // Mask
#define TIMER_ICR_TATOCINT      0x00000001  // GPTM TimerA Time-Out Raw
                                            // Interrupt
#define TIMER_TAILR_TAILRL_M    0x0000FFFF  // GPTM TimerA Interval Load
                                            // Register Low
#define SYSCTL_RCGC1_R          (*((volatile unsigned long *)0x400FE104))
#define SYSCTL_RCGC1_TIMER0     0x00010000  // timer 0 Clock Gating Control

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode
void (*PeriodicTask)(void);   // user function

// ***************** Timer0A_Init ****************
// Activate Timer0A interrupts to run user task periodically
// Inputs:  task is a pointer to a user function
//          period in usec
// Outputs: none
void Timer2A_Init(void(*task)(void), unsigned short period){long sr;
  sr = StartCritical(); 
  SYSCTL_RCGC1_R |= SYSCTL_RCGC1_TIMER2;  // 0) activate timer0
  PeriodicTask = task;             // user function (this line also allows time to finish activating)
  TIMER2_CTL_R &= ~TIMER_CTL_TAEN;        // 1) disable timer2A during setup
  TIMER2_CFG_R = TIMER_CFG_16_BIT;  // 2) configure for 16-bit timer mode
  TIMER2_TAMR_R = TIMER_TAMR_TAMR_PERIOD;     // 3) configure for periodic mode, default down-count settings
  TIMER2_TAILR_R = period-1;       // 4) reload value
  TIMER2_TAPR_R = 49;              // 5) 1us timer0A
  TIMER2_ICR_R = TIMER_ICR_TATOCINT;       // 6) clear timer0A timeout flag
  TIMER2_IMR_R |= TIMER_IMR_TATOIM;      // 7) arm timeout interrupt
  NVIC_PRI5_R = (NVIC_PRI5_R&0x00FFFFFF)|0x40000000; // 8) priority 2
  NVIC_EN0_R = NVIC_EN0_INT23;     // 9) enable interrupt 23 in NVIC
  TIMER2_CTL_R |= TIMER_CTL_TAEN;      // 10) enable timer0A
  EndCritical(sr);
}

void Timer2A_Handler(void){
  TIMER2_ICR_R = TIMER_ICR_TATOCINT;// acknowledge timer0A timeout
  (*PeriodicTask)();                // execute user task
}
