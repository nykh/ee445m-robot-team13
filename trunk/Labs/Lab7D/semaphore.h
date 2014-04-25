#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

struct tcb_s;
struct  Sema4{
  long Value;   // >0 means free, otherwise means busy  
	struct tcb_s	*BlocketListPt;
// add other components here, if necessary to implement blocking
};
typedef struct Sema4 Sema4Type;

extern Sema4Type	Sema4UART;
extern Sema4Type  Sema4CharAvailable;

extern Sema4Type  Sema4fifo;
extern Sema4Type  Sema4MailboxEmpty;
extern Sema4Type  Sema4MailboxFull;

extern Sema4Type Sema4FFTReady;
extern Sema4Type Sema4DataAvailable;

extern Sema4Type Sema4CAN;

#endif  //__SEMAPHORE_H
