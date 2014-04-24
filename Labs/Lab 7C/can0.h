// *********Can0.h ***************
// Runs on LM4F120/TM4C123
// Use CAN0 to communicate on CAN bus
// CAN0Rx PE4 (8) I TTL CAN module 0 receive.
// CAN0Tx PE5 (8) O TTL CAN module 0 transmit.

// Jonathan Valvano
// March 22, 2014

/* This example accompanies the books
   Embedded Systems: Real-Time Operating Systems for ARM Cortex-M Microcontrollers, Volume 3,  
   ISBN: 978-1466468863, Jonathan Valvano, copyright (c) 2013

   Embedded Systems: Real Time Interfacing to ARM Cortex M Microcontrollers, Volume 2
   ISBN: 978-1463590154, Jonathan Valvano, copyright (c) 2013

 Copyright 2014 by Jonathan W. Valvano, valvano@mail.utexas.edu
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

#ifndef __CAN0_H__
#define __CAN0_H__
#define CAN_BITRATE             500000

#define CAN    1 //1 = receiver, 0 = transmitter
// reverse these IDs on the other microcontroller
#if CAN
	#define RCV_ID 4
	#define XMT_ID 2
#else
	#define RCV_ID 2
	#define XMT_ID 4
#endif

// 11 bit
typedef enum PackageID_t {
	IRSensor = 1,
	PingSensor = 2,
	Motors = 3,
} PackageID;
// Initialize CAN port
void CAN0_Open(void);

#if CAN // Receiver

// if receive data is ready, gets the data 
// if no receive data is ready, it waits until it is ready
void CAN0_GetMail(PackageID *receiveID, unsigned char data[4]);

#else // Transmitter

// send 4 bytes of data to other microcontroller 
void CAN0_SendData(PackageID sendID, unsigned char data[4]);

#endif

#endif //  __CAN0_H__

