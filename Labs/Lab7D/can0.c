// can0.c
// Runs on LM4F120/TM4C123
// Use CAN0 to communicate on CAN bus PE4 and PE5
// 

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
// MCP2551 Pin1 TXD  ---- CAN0Tx PE5 (8) O TTL CAN module 0 transmit
// MCP2551 Pin2 Vss  ---- ground
// MCP2551 Pin3 VDD  ---- +5V with 0.1uF cap to ground
// MCP2551 Pin4 RXD  ---- CAN0Rx PE4 (8) I TTL CAN module 0 receive
// MCP2551 Pin5 VREF ---- open (it will be 2.5V)
// MCP2551 Pin6 CANL ---- to other CANL on network 
// MCP2551 Pin7 CANH ---- to other CANH on network 
// MCP2551 Pin8 RS   ---- ground, Slope-Control Input (maximum slew rate)
// 120 ohm across CANH, CANL on both ends of network

#include "hw_can.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "hw_types.h"
#include "can.h"
#include "debug.h"
#include "interrupt.h"

#include "os.h"
#include "semaphore.h"
#include "FIFO.h"

#include "can0.h"
#include "inc/tm4c123gh6pm.h"

#define NULL 0

#if CAN // Receiver

// FIFO linkage from background to foreground
// Two-index implementation of the transmit FIFO
// can hold 0 to TXFIFOSIZE elements
#define RCVFIFOSIZE    16 // must be a power of 2
#define RCVFIFOSUCCESS 1
#define RCVFIFOFAIL    0

typedef struct {
	PackageID RCVID;
	unsigned char RCVData[4];
} RCVPacket;

AddIndexFifo(RCV,RCVFIFOSIZE,RCVPacket,RCVFIFOSUCCESS,RCVFIFOFAIL)

#endif

//*****************************************************************************
//
// The CAN controller interrupt handler.
//
//*****************************************************************************
void CAN0_Handler(void){ unsigned char data[4];
  unsigned long ulIntStatus, ulIDStatus;
  int i;
  tCANMsgObject xTempMsgObject;
  xTempMsgObject.pucMsgData = data;
  ulIntStatus = CANIntStatus(CAN0_BASE, CAN_INT_STS_CAUSE); // cause?
  if(ulIntStatus & CAN_INT_INTID_STATUS){  // receive?
    ulIDStatus = CANStatusGet(CAN0_BASE, CAN_STS_NEWDAT);
    for(i = 0; i < 32; i++){    //test every bit of the mask
      if( (0x1 << i) & ulIDStatus){  // if active, get data
				#if CAN
				RCVPacket newpacket;
				#endif
				
				CANMessageGet(CAN0_BASE, (i+1), &xTempMsgObject, true);
				#if CAN
				newpacket.RCVID = (PackageID) xTempMsgObject.ulMsgID;
				newpacket.RCVData[0] = data[0];
				newpacket.RCVData[1] = data[1];
				newpacket.RCVData[2] = data[2];
				newpacket.RCVData[3] = data[3];
				
				RCVFifo_Put(newpacket);
				
				OS_Signal(&Sema4CAN);
				#endif
      }
    }
  }
  CANIntClear(CAN0_BASE, ulIntStatus);  // acknowledge
}

//Set up a message object.  Can be a TX object or an RX object.
void static CAN0_Setup_Message_Object( unsigned long MessageID, \
                                unsigned long MessageFlags, \
                                unsigned long MessageLength, \
                                unsigned char * MessageData, \
                                unsigned long ObjectID, \
                                tMsgObjType eMsgType){
  tCANMsgObject xTempObject;
  xTempObject.ulMsgID = MessageID;          // 11 or 29 bit ID
  xTempObject.ulMsgLen = MessageLength;
  xTempObject.pucMsgData = MessageData;
  xTempObject.ulFlags = MessageFlags;
  CANMessageSet(CAN0_BASE, ObjectID, &xTempObject, eMsgType);
}
// Initialize CAN port
void CAN0_Open(void){unsigned long volatile delay; 
  
  #if CAN // Receiver
	RCVFifo_Init();
  #endif
  
  OS_InitSemaphore(&Sema4CAN, 0);
	
  SYSCTL_RCGCCAN_R |= 0x00000001;  // CAN0 enable bit 0
  SYSCTL_RCGCGPIO_R |= 0x00000010;  // RCGC2 portE bit 4
  for(delay=0; delay<100; delay++){};
  GPIO_PORTE_AFSEL_R |= 0x30; //PORTE AFSEL bits 5,4
// PORTE PCTL 88 into fields for pins 5,4
  GPIO_PORTE_PCTL_R = (GPIO_PORTE_PCTL_R&0xFF00FFFF)|0x00880000;
  GPIO_PORTE_DEN_R |= 0x30;
  GPIO_PORTE_DIR_R |= 0x20;
      
  CANInit(CAN0_BASE);
  CANBitRateSet(CAN0_BASE, 80000000, CAN_BITRATE);
  CANEnable(CAN0_BASE);
// make sure to enable STATUS interrupts
  CANIntEnable(CAN0_BASE, CAN_INT_MASTER | CAN_INT_ERROR | CAN_INT_STATUS);
// Set up filter to receive these IDs
// in this case there is just one type, but you could accept multiple ID types
  //CAN0_Setup_Message_Object(RCV_ID, MSG_OBJ_RX_INT_ENABLE, 4, NULL, RCV_ID, MSG_OBJ_TYPE_RX);
	#if CAN
  CAN0_Setup_Message_Object((unsigned long) (IRSensor), MSG_OBJ_RX_INT_ENABLE, 4, NULL, (unsigned long) (IRSensor), MSG_OBJ_TYPE_RX);
  CAN0_Setup_Message_Object((unsigned long) (PingSensor), MSG_OBJ_RX_INT_ENABLE, 4, NULL, (unsigned long) (PingSensor), MSG_OBJ_TYPE_RX);
	#endif
  
  NVIC_EN1_R = (1 << (INT_CAN0 - 48)); //IntEnable(INT_CAN0);
  
  return;
}

#if CAN // Receiver

// if receive data is ready, gets the data 
// if no receive data is ready, it waits until it is ready
void CAN0_GetMail(PackageID *receiveID, unsigned char data[4]){
	RCVPacket get;
	
  OS_Wait(&Sema4CAN);
	RCVFifo_Get(&get);
		*receiveID = get.RCVID;
		data[0] = get.RCVData[0];
		data[1] = get.RCVData[1];
		data[2] = get.RCVData[2];
		data[3] = get.RCVData[3];
}

#else // Transmitter

// send 4 bytes of data to other microcontroller 
void CAN0_SendData(PackageID sendID, unsigned char data[4]){
// in this case there is just one type, but you could accept multiple ID types
  CAN0_Setup_Message_Object((unsigned long) sendID, NULL, 4, data, (unsigned long) sendID, MSG_OBJ_TYPE_TX);
}

#endif
