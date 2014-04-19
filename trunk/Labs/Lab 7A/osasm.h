/*****************************************************************************/
/* OSasm.s: low-level OS commands, written in assembly                       */
// Real Time Operating System 
/*
; This example accompanies the book
;  "Embedded Systems: Real Time Interfacing to the Arm Cortex M3",
;  ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2011
;
;  Programs 6.4 through 6.12, section 6.2
;
;Copyright 2011 by Jonathan W. Valvano, valvano@mail.utexas.edu
;    You may use, edit, run or distribute this file
;    as long as the above copyright notice remains
; THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
; OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
; MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
; VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
; OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
; For more information about my classes, my research, and my books, see
; http://users.ece.utexas.edu/~valvano/
; */

#ifndef __OSASM_H__
#define __OSASM_H__

void OS_DisableInterrupts(void);
void OS_EnableInterrupts(void);

void StartOS(void);

#endif // __OSASM_H__
