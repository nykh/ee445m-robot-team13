#include "os.h"
#include "semaphore.h"
#include "heap.h"
#include "FIFO.h"

#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "inc/tm4c123gh6pm.h"
#include "osasm.h"

#include "io.h"

/************************* Hardware use ********************************/
#define NVIC_EN2_INT70          0x00000040  // Interrupt 35 enable
#define NVIC_EN1_INT35          0x00000008  // Interrupt 35 enable
#define NVIC_EN0_INT30          0x40000000  // Interrupt 30 enable
#define NVIC_EN0_INT23          0x00800000  // Interrupt 23 enable
#define NVIC_EN0_INT21					0x00200000	// Interrupt 21 enable

#define PF4                     (*((volatile unsigned long *)0x40025040)) // SW1
#define PF0                     (*((volatile unsigned long *)0x40025004)) // SW2
#define SW1                     0x10 // PF4
#define SW2                     0x01 // PF0

/************************* Fifo and Mailbox ********************************/
#define FIFOSIZE   64         // size of the FIFOs (must be power of 2)
#define FIFOSUCCESS 1         // return value on success
#define FIFOFAIL    0         // return value on failure
                              // create index implementation FIFO (see FIFO.h)
AddIndexFifo(OS, FIFOSIZE, unsigned long, FIFOSUCCESS, FIFOFAIL)

unsigned long Mailbox;

/************************** System Performance Measurement ***********************/

#ifdef __Performance_Measurement__


	#ifdef __Jitter_Measurement__
	// Measure jitter of periodic tasks
	#define JITTERSIZE 64

	unsigned long Period1;
	long MaxJitter1;             // largest time jitter between interrupts in usec
	long MinJitter1;             // smallest time jitter between interrupts in usec
	unsigned long const JitterSize1=JITTERSIZE;
	unsigned long JitterHistogram1[JITTERSIZE]={0,};

	unsigned long Period2;
	long MaxJitter2;             // largest time jitter between interrupts in usec
	long MinJitter2;             // smallest time jitter between interrupts in usec
	unsigned long const JitterSize2=JITTERSIZE;
	unsigned long JitterHistogram2[JITTERSIZE]={0,};
	#endif

	#ifdef __Critical_Interval_Measurement__

	void DisableInterrupts_check(void); // Disable interrupts
	void EnableInterrupts_check(void);  // Enable interrupts
	long StartCritical_check(void);    // previous I bit, disable interrupts
	void EndCritical_check(long sr);    // restore I bit to previous value

	#endif

	#ifdef __Profiling__
	
	#define PROFILELENGTH 100
	
	#define Profile_PendSV_Trigger 1
	#define Profile_SysTick_Starts 2
	#define Profile_SysTick_End    3
	#define Profile_Timer1_Starts  4
	#define Profile_Timer1_End     5
	#define Profile_Timer2_Starts  6
	#define Profile_Timer2_End     7
	#define Profile_Timer3_Starts  8
	#define Profile_Timer3_End     9
	#define Profile_Timer4_Starts  10
	#define Profile_Timer4_End     11
	
	typedef struct {
		unsigned char value;
		unsigned long time;
	} TimeStamp;
	
	TimeStamp Profile[PROFILELENGTH];
	TimeStamp *ProfilePt = Profile;
	
	void timestamp(unsigned char value);
	
	#endif
#endif

/************************* Datastructure for OS Round-robin scheduler ********************************/

#define NUM_PRIORITY 8
#define STACKSIZE_MAX  200
#define STACKSIZE_MIN  64

typedef struct tcb_s {
	long *sp;
	struct tcb_s *next;
	struct tcb_s *prev;
	long *stack;
	unsigned int id;
	unsigned long sleepCount;
	unsigned char priority;
} TCB;

static unsigned long uniqid = 0;

TCB *RunPt, *NextPt, *SleepPt;
TCB *PriPt[NUM_PRIORITY];

static unsigned long Timer=0;

/************************* Datastructure for Background thread scheduling ********************************/

// dummy
static void dummy(void) {}

static void (*PeriodicTask1)(void) = dummy;   // user function 1
static void (*PeriodicTask2)(void) = dummy;   // user function 2

static char LastPF0 = 1;
static char LastPF4 = 1;

static void (*SW1Task) (void) = dummy;        // user function 3
static void (*SW2Task) (void) = dummy;        // user function 4

/************************* Macros and private functions for OS ********************************/
#define DLLINK(curr, pt) {   \
	curr->next = pt->next;     \
	pt->next = curr;           \
	curr->prev = pt;           \
	curr->next->prev = curr;   \
}

#define DLLINKLAST(curr, pt) {   \
	curr->next = pt;               \
	pt->prev->next = curr;         \
	curr->prev = pt->prev;         \
	pt->prev = curr;               \
}

#define DLUNLINK(pt) {        \
	pt->prev->next = pt->next;  \
	pt->next->prev = pt->prev;  \
}

void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

static void updateNextPt (int startLevel);
static void triggerPendSV (void);

static void dummyThread(void) {
	while(1);
}

static void SetInitialStack(TCB * tcb, unsigned long stacksize ){
	tcb->sp = &tcb->stack[stacksize-16];
  tcb->stack[stacksize-1] = 0x01000000;   // thumb bit
  tcb->stack[stacksize-3] = 0x14141414;   // R14
  tcb->stack[stacksize-4] = 0x12121212;   // R12
  tcb->stack[stacksize-5] = 0x03030303;   // R3
  tcb->stack[stacksize-6] = 0x02020202;   // R2
  tcb->stack[stacksize-7] = 0x01010101;   // R1
  tcb->stack[stacksize-8] = 0x00000000;   // R0
  tcb->stack[stacksize-9] = 0x11111111;   // R11
  tcb->stack[stacksize-10] = 0x10101010;  // R10
  tcb->stack[stacksize-11] = 0x09090909;  // R9
  tcb->stack[stacksize-12] = 0x08080808;  // R8
  tcb->stack[stacksize-13] = 0x07070707;  // R7
  tcb->stack[stacksize-14] = 0x06060606;  // R6
  tcb->stack[stacksize-15] = 0x05050505;  // R5
  tcb->stack[stacksize-16] = 0x04040404;  // R4
}

// ******** OS_Init ************
// initialize operating system, disable interrupts until OS_Launch
// initialize OS controlled I/O: serial, ADC, systick, select switch and timer2 
// input:  none
// output: none
void OS_Init(void) {  volatile unsigned long delay;
  OS_DisableInterrupts();   // set processor clock to 50 MHz
//  SysCtlClockSet(SYSCTL_SYSDIV_4 | SYSCTL_USE_PLL |
//                 SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);
  NVIC_ST_CTRL_R = 0;         // disable SysTick during setup
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
  NVIC_SYS_PRI3_R =(NVIC_SYS_PRI3_R&0x00FFFFFF)|0xC0000000; // SysTick priority 6
  NVIC_SYS_PRI3_R =(NVIC_SYS_PRI3_R&0xFF00FFFF)|0x00E00000; // PendSV priority 7
	
	//Activate Timer1 for periodic interrupt for Waking up Threads
  SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R1;  // 0) activate timer1
	delay = SYSCTL_RCGCTIMER_R;
  TIMER1_CTL_R &= ~TIMER_CTL_TAEN;        // 1) disable timer1A during setup
  TIMER1_CFG_R = TIMER_CFG_32_BIT_TIMER;  // 2) configure for 16-bit timer mode
  TIMER1_TAMR_R = TIMER_TAMR_TAMR_PERIOD;     // 3) configure for periodic mode, default down-count settings
  TIMER1_TAILR_R = 800000-1;       // 4) reload value
  TIMER1_ICR_R = TIMER_ICR_TATOCINT;       // 6) clear timer0A timeout flag
  TIMER1_IMR_R |= TIMER_IMR_TATOIM;      // 7) arm timeout interrupt
  NVIC_PRI5_R = (NVIC_PRI5_R&0xFFFF00FF)|0x00008000; // 8) priority 4
  NVIC_EN0_R = NVIC_EN0_INT21;     // 9) enable interrupt 23 in NVIC
  TIMER1_CTL_R |= TIMER_CTL_TAEN;      // 10) enable 

	//Activate Timer2 for system timer
  SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R2;  // 0) activate timer2
  delay = SYSCTL_RCGCTIMER_R;             // user function (this line also allows time to finish activating)
  TIMER2_CTL_R &= ~TIMER_CTL_TAEN;        // 1) disable timer2A during setup
  TIMER2_CFG_R = TIMER_CFG_32_BIT_TIMER;  // 2) configure for 16-bit timer mode
  TIMER2_TAMR_R = TIMER_TAMR_TAMR_PERIOD;     // 3) configure for periodic mode, default down-count settings
  TIMER2_TAILR_R = 0xFFFFFFFF;       // 4) reload value
	TIMER2_CTL_R = TIMER_CTL_TAEN;

	//Activate PF0 Edge Triggered Interrupt
	SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R5;
	
	GPIO_PORTF_LOCK_R = GPIO_LOCK_KEY; // Unlock to access CR
	GPIO_PORTF_CR_R |= (SW1|SW2);      // enable commit for PF4 and PF0
	GPIO_PORTF_LOCK_R = 0;             // Relock
  
	GPIO_PORTF_DIR_R &= ~(SW1|SW2);    // (c) make PF4 in (built-in button)
  GPIO_PORTF_AFSEL_R &= ~(SW1|SW2);  //     disable alt funct on PF0, PF4
  GPIO_PORTF_DEN_R |= (SW1|SW2);     //     enable digital I/O on PF0, PF4   
  GPIO_PORTF_PCTL_R &= ~0x000F000F; // configure PF0, PF4 as GPIO
  GPIO_PORTF_AMSEL_R = 0;       //     disable analog functionality on PF
  GPIO_PORTF_PUR_R |= (SW1|SW2);     //     enable weak pull-up on PF0, PF4
  GPIO_PORTF_IS_R &= ~(SW1|SW2);     // (d) PF0, PF4 is edge-sensitive
  GPIO_PORTF_IBE_R |= (SW1|SW2);    //      PF0, PF4 is not both edges
  GPIO_PORTF_IEV_R &= ~(SW1|SW2);    //     PF0, PF4 falling edge event
  GPIO_PORTF_ICR_R = (SW1|SW2);      // (e) clear flag0, 4
  GPIO_PORTF_IM_R |= (SW1|SW2);      // (f) arm interrupt on PF0, PF4
	
	Heap_Init();
	
  OS_AddThread(dummyThread,32,NUM_PRIORITY-1);
}

// ******** OS_InitSemaphore ************
// initialize semaphore 
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, long value){
	semaPt->Value = value;
	semaPt->BlocketListPt = 0;
}

// ******** OS_Wait ************
// decrement semaphore and spin/block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt){
	#ifdef __Critical_Interval_Measurement__
	long sr = StartCritical_check();
	#else
	long sr = StartCritical();
	#endif
	
	semaPt->Value -= 1;
	
	if(semaPt->Value < 0) {
		// Block
		// Unlink from RunPt
		if (RunPt->next == RunPt) {
			PriPt[RunPt->priority] = NULL;
		} else {
			DLUNLINK(RunPt);
			PriPt[RunPt->priority] = RunPt->next;
		}
		
		// Link to semaphore linked list
		// Link to the last one in order to guarantee bounded waiting time
		if(semaPt->BlocketListPt) {
			DLLINKLAST(RunPt, semaPt->BlocketListPt);
		} else {
			RunPt->next = RunPt->prev = RunPt;
			semaPt->BlocketListPt = RunPt;
		}
		updateNextPt(RunPt->priority);
		triggerPendSV(); // this enables interrupts
	}
	
	#ifdef __Critical_Interval_Measurement__
	EndCritical_check(sr);
	#else
	EndCritical(sr);
	#endif
}

// ******** OS_Signal ************
// increment semaphore, wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt){
	#ifdef __Critical_Interval_Measurement__
	long sr = StartCritical_check();
	#else
	long sr = StartCritical();
	#endif	
	
	TCB *current = semaPt->BlocketListPt;
	semaPt->Value += 1;
	
	if(semaPt->Value <= 0) {
		// UnBlock
		// Unlink from semaphore linked list
		if (current->next == current) {
			semaPt->BlocketListPt = NULL;
		} else {
			DLUNLINK(current);
			semaPt->BlocketListPt = current->next;
		}
		
		// Link to PriPt
		if(PriPt[current->priority]) {
			DLLINK(current, PriPt[current->priority]);
		} else {
			current->next = current->prev = current;
			PriPt[current->priority] = current;
		}
		updateNextPt(0);
		triggerPendSV();
	}

	#ifdef __Critical_Interval_Measurement__
	EndCritical_check(sr);
	#else
	EndCritical(sr);
	#endif
}

// ******** OS_bWait ************
// if the semaphore is 0 then spin/block
// if the semaphore is 1, then clear semaphore to 0
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt){
	OS_Wait(semaPt);
}

// ******** OS_bSignal ************
// set semaphore to 1, wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *semaPt){
	#ifdef __Critical_Interval_Measurement__
	long sr = StartCritical_check();
	#else
	long sr = StartCritical();
	#endif
	
	TCB *current = semaPt->BlocketListPt;
	if(semaPt->Value < 1) semaPt->Value += 1; // Doesn't allow semaphore value to go over 1
	
	if(semaPt->Value <= 0) {
		// UnBlock
		// Unlink from semaphore linked list
		if (current->next == current) {
			semaPt->BlocketListPt = NULL;
		} else {
			DLUNLINK(current);
			semaPt->BlocketListPt = current->next;
		}
		
		// Link to PriPt
		if(PriPt[current->priority]) {
			DLLINK(current, PriPt[current->priority]);
		} else {
			current->next = current->prev = current;
			PriPt[current->priority] = current;
		}	
		updateNextPt(0);
		triggerPendSV();
	}

	#ifdef __Critical_Interval_Measurement__
	EndCritical_check(sr);
	#else
	EndCritical(sr);
	#endif
}

//******** OS_AddThread *************** 
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority (0 is highest)
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields
int OS_AddThread(void(*task)(void),
								 unsigned long stackSize,
								 unsigned long priority) {
	TCB *tcb; long sr;
	
	if (priority >= NUM_PRIORITY)	priority = NUM_PRIORITY-1;
	
	stackSize = (stackSize < STACKSIZE_MIN) ? STACKSIZE_MIN : ( (stackSize > STACKSIZE_MAX) ? STACKSIZE_MAX : stackSize ) ;							 
	
	#ifdef __Critical_Interval_Measurement__
	sr = StartCritical_check();
	#else
	sr = StartCritical();
	#endif
	
	
	
	if (!(tcb = (TCB *) Heap_Malloc(sizeof(TCB)))){
		return NULL;
	};
	if (!(tcb->stack = (long *) Heap_Malloc (sizeof(long)*stackSize))){
		Heap_Free (tcb);
		return NULL;
	};
	SetInitialStack(tcb, stackSize);
	
	// Add new tcb to the dllist
	if(PriPt[priority]) {
		DLLINKLAST(tcb, PriPt[priority]);
	} else {
		tcb->next = tcb->prev = tcb;
		PriPt[priority] = tcb;
	}
	
	tcb->stack[stackSize-2] = (long) (task);
	tcb->sleepCount = 0;
	tcb->priority = priority;
	tcb->id = OS_Id();
	updateNextPt(0);
	
	if(!RunPt) RunPt = NextPt;

	updateNextPt(0);
	triggerPendSV(); // this enables interrupts
	
	#ifdef __Critical_Interval_Measurement__
	EndCritical_check(sr);
	#else
	EndCritical(sr);
	#endif
	
	return 1;
}

//******** OS_Id *************** 
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero 
unsigned long OS_Id(void) {
	return uniqid++;
}

//******** OS_AddPeriodicThread *************** 
// add a background periodic task
// typically this function receives the highest priority
// Inputs: pointer to a void/void background function
//         period given in system time units
//         priority 0 is highest, 5 is lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal	 OS_AddThread
// You are free to select the time resolution for this function
// This task does not have a Thread ID
// In lab 2, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, this command will be called 0 1 or 2 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
#define IDLE       0
#define ONE_IN_USE 1
int OS_AddPeriodicThread(void(*task)(void), unsigned long period, unsigned long priority){
	long sr; volatile unsigned long delay;
	static char currentTimer = 0;
	if (priority > 5) {
		priority = 5;
	}

	#ifdef __Critical_Interval_Measurement__
	sr = StartCritical_check();
	#else
	sr = StartCritical();
	#endif	
	
	switch(currentTimer) {
		case IDLE:	
			SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R3;
			//SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R3;  // 0) activate timer3
			delay = SYSCTL_RCGCTIMER_R;
			delay = SYSCTL_RCGCTIMER_R;
			PeriodicTask1 = task;             // user function (this line also allows time to finish activating)
			TIMER3_CTL_R &= ~TIMER_CTL_TAEN;        // 1) disable timer2A during setup
			TIMER3_CFG_R = TIMER_CFG_32_BIT_TIMER;  // 2) configure for 16-bit timer mode
			TIMER3_TAMR_R = TIMER_TAMR_TAMR_PERIOD;     // 3) configure for periodic mode, default down-count settings
			TIMER3_TAILR_R = period-1;       // 4) reload value
			//xTIMER2_TAPR_R                 // 5) prescaler is of no use in 32bit mode
			TIMER3_ICR_R = TIMER_ICR_TATOCINT;       // 6) clear timer2A timeout flag
			TIMER3_IMR_R |= TIMER_IMR_TATOIM;      // 7) arm timeout interrupt
			NVIC_PRI8_R = (NVIC_PRI8_R&0x00FFFFFF)|((priority)<<29); // 8) setting priority 
			NVIC_EN1_R |= NVIC_EN1_INT35;  // (h) enable interrupt 30 in NVIC
			TIMER3_CTL_R |= TIMER_CTL_TAEN;      // 10) enable timer2A
			currentTimer++;
		
			#ifdef __Jitter_Measurement__
			Period1 = period;
			#endif
		
			break;
		case ONE_IN_USE:
			SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R4;	// 0) activate timer4
			delay = SYSCTL_RCGCTIMER_R;
			delay = SYSCTL_RCGCTIMER_R;
			PeriodicTask2 = task;             // user function (this line also allows time to finish activating)
			TIMER4_CTL_R &= ~TIMER_CTL_TAEN;        // 1) disable timer2A during setup
			TIMER4_CFG_R = TIMER_CFG_32_BIT_TIMER;  // 2) configure for 16-bit timer mode
			TIMER4_TAMR_R = TIMER_TAMR_TAMR_PERIOD;     // 3) configure for periodic mode, default down-count settings
			TIMER4_TAILR_R = period-1;       // 4) reload value
			//xTIMER2_TAPR_R                 // 5) prescaler is of no use in 32bit mode
			TIMER4_ICR_R = TIMER_ICR_TATOCINT;       // 6) clear timer2A timeout flag
			TIMER4_IMR_R |= TIMER_IMR_TATOIM;      // 7) arm timeout interrupt
			NVIC_PRI17_R = (NVIC_PRI17_R&0xFF00FFFF)|((priority)<<21); // 8) setting priority 
			NVIC_EN2_R |= NVIC_EN2_INT70;  // (h) enable interrupt 30 in NVIC
			TIMER4_CTL_R |= TIMER_CTL_TAEN;      // 10) enable timer2A
			currentTimer++;
		  
			#ifdef __Jitter_Measurement__
			Period2 = period;
			#endif
		
			break;
		default:
			break;
	}

	#ifdef __Critical_Interval_Measurement__
	EndCritical_check(sr);
	#else
	EndCritical(sr);
	#endif	
	
	return 1;
}
#undef IDLE
#undef ONE_IN_USE

//******** OS_AddButtonTask *************** 
// add a background task to run whenever the Select button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is highest, 5 is lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal	 OS_AddThread
// This task does not have a Thread ID
// In labs 2 and 3, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads

#define IDLE        0 
#define ONE_IN_USE  1
#define BOTH_IN_USE 2
static unsigned char buttonuse = IDLE;
static unsigned long sw1priority;
static unsigned long sw2priority;

int OS_AddSW1Task(void(*task)(void), unsigned long priority){
	SW1Task = task;	
	sw1priority = (priority & 0x07)<<21;
	
	if(buttonuse == IDLE) {
		NVIC_EN0_R |= NVIC_EN0_INT30;  // enable interrupt 30 in NVIC
	}
	
	NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|sw1priority;
	
	buttonuse += 1;
	
	return 1;
}

int OS_AddSW2Task(void(*task)(void), unsigned long priority){
	SW2Task = task;	
	sw2priority = (priority & 0x07)<<21;
	
	if(buttonuse == IDLE) {
		NVIC_EN0_R |= NVIC_EN0_INT30;  // enable interrupt 30 in NVIC
	}
	
	NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|sw2priority;
	
	buttonuse += 1;
	
	return 1;
}

int OS_RemoveSW1Task(void) {
	if(SW1Task == dummy) {
		return 0;
	}
	
	if(--buttonuse) { // Another button is in use
		NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|sw2priority; // restore priority to the other one
	} else {
		NVIC_EN0_R &= ~NVIC_EN0_INT30;  // disable interrupt 30 in NVIC		
	}
	
	SW1Task = dummy;
	return 1;
}

int OS_RemoveSW2Task(void) {
	if(SW2Task == dummy) {
		return 0;
	}
	
	if(--buttonuse) { // Another button is in use
		NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|sw1priority; // restore priority to the other one
	} else {
		NVIC_EN0_R &= ~NVIC_EN0_INT30;  // disable interrupt 30 in NVIC		
	}
	
	SW2Task = dummy;
	return 1;
}

#ifdef DEPRECATE
int OS_AddButtonTask(void(*task)(void), unsigned long priority) {
	OS_AddSW1Task(task, priority);
}
#endif

#undef IDLE
#undef ONE_IN_USE
#undef BOTH_IN_USE

//******** OS_AddDownTask *************** 
// add a background task to run whenever the Down arror button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is highest, 5 is lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed user task will run to completion and return
// This task can not spin block loop sleep or kill
// It can call issue OS_Signal, it can call OS_AddThread
// This task does not have a Thread ID
// In lab 2, this function can be ignored
// In lab 3, this command will be called will be called 0 or 1 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddDownTask(void(*task)(void), unsigned long priority);

// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(unsigned long sleepTime){
	TCB *tcb = RunPt;
	
	#ifdef __Critical_Interval_Measurement__
	DisableInterrupts_check();
	#else
	DisableInterrupts();
	#endif
	
	// Unlink from RunPt list
	if (RunPt->next == RunPt) {
		PriPt[RunPt->priority] = NULL;
		updateNextPt(RunPt->priority);	
	} else {
		DLUNLINK(RunPt);
		PriPt[RunPt->priority] = RunPt->next;
		NextPt = RunPt->next;
	}
	
	// Link to SleepPt list
	if(SleepPt) {
		DLLINK(tcb, SleepPt);
	} else {
		tcb->next = tcb->prev = tcb;
		SleepPt = tcb;
	}
	tcb->sleepCount = sleepTime;
	
	triggerPendSV();
	
	#ifdef __Critical_Interval_Measurement__
	EnableInterrupts_check();
	#else
	EnableInterrupts();
	#endif
}

// ******** OS_Kill ************
// kill the currently running thread, release its TCB memory
// input:  none
// output: none
void OS_Kill(void){
	
	#ifdef __Critical_Interval_Measurement__
	DisableInterrupts_check();
	#else
	DisableInterrupts();
	#endif
	
	
	Heap_Free(RunPt->stack);
	if (RunPt->next == RunPt) {
		PriPt[RunPt->priority] = NULL;
	} else {
		DLUNLINK(RunPt);
		PriPt[RunPt->priority] = RunPt->next;
	}
	Heap_Free(RunPt);
	updateNextPt(0);
	triggerPendSV();

	#ifdef __Critical_Interval_Measurement__
	EnableInterrupts_check();
	#else
	EnableInterrupts();
	#endif	
	
}

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void){
  NVIC_ST_CURRENT_R = 0;      // any write to current clears it
	NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PENDSTSET; // Trigger SysTick 
}
 
// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none 
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128
void OS_Fifo_Init(unsigned long size) {
	OSFifo_Init();
	OS_InitSemaphore(&Sema4fifo, 0);
}

// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting 
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers 
//  this function can not disable or enable interrupts
int OS_Fifo_Put(unsigned long data) {
	if(OSFifo_Put(data) == FIFOSUCCESS) {
		OS_Signal(&Sema4fifo);
		return FIFOSUCCESS;
	} else {
		return FIFOFAIL;
	}
}

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
unsigned long OS_Fifo_Get(void) {
	unsigned long data;
	OS_Wait(&Sema4fifo);
	OSFifo_Get(&data);
	return data;
}

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty 
//          zero or less than zero  if a call to OS_Fifo_Get will spin or block
long OS_Fifo_Size(void) {
	return OSFifo_Size();
}

// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void) {
	OS_InitSemaphore(&Sema4MailboxEmpty, 1);
	OS_InitSemaphore(&Sema4MailboxFull, 0);	
}

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void OS_MailBox_Send(unsigned long data) {
  OS_bSignal(&Sema4MailboxFull);
  OS_bWait(&Sema4MailboxEmpty);
	Mailbox = data;
}

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
unsigned long OS_MailBox_Recv(void) {
	OS_bSignal(&Sema4MailboxEmpty);
	OS_bWait(&Sema4MailboxFull);
	return Mailbox;
}

// ******** OS_Time ************
// reads a timer value 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to max
// The time resolution should be at least 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
unsigned long OS_Time(void){
	return TIMER2_TAV_R;	
}

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units 
// The time resolution should be at least 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
unsigned long OS_TimeDifference(unsigned long stop, unsigned long start){
	return (start-stop);
}

// ******** OS_ClearMsTime ************
// sets the system time to zero from Lab 1)
// Inputs:  none
// Outputs: none
// You are free to change how this works
void OS_ClearMsTime(void){
	Timer = 0;
}

// ******** OS_MsTime ************
// reads the current time in msec (from Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
unsigned long OS_MsTime(void){
	return Timer;
}

//******** OS_Launch *************** 
// start the scheduler, enable interrupts
// Inputs: number of 20ns clock cycles for each time slice
//         you may select the units of this parameter
// Outputs: none (does not return)
// In Lab 2, you can ignore the theTimeSlice field
// In Lab 3, you should implement the user-defined TimeSlice field
void OS_Launch(unsigned long theTimeSlice){
  NVIC_ST_RELOAD_R = theTimeSlice - 1; // reload value
  NVIC_ST_CTRL_R = 0x00000007; // enable, core clock and interrupt arm
  StartOS();                   // start on the first task
}


void SysTick_Handler(void) { long sr;
	#ifdef __Critical_Interval_Measurement__
	sr = StartCritical_check();
	#else
	sr = StartCritical();
	#endif
	
	#ifdef __Profiling__
	timestamp(Profile_SysTick_Starts);
	#endif
	
	NextPt = RunPt->next;
	NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV; // PendSV 	

	#ifdef __Profiling__
	timestamp(Profile_PendSV_Trigger);
	#endif
	
	#ifdef __Profiling__
	timestamp(Profile_SysTick_End);
	#endif
	
	#ifdef __Critical_Interval_Measurement__
  EndCritical_check(sr);
	#else
	EndCritical(sr);
	#endif
	
	
}

static void triggerPendSV (void) {
	#ifdef __Profiling__
	timestamp(Profile_PendSV_Trigger);
	#endif	
	
  NVIC_ST_CURRENT_R = 0;      // any write to current clear systick, next thread gets a full slice
	NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV; // Trigger Pend SV 
}

static void updateNextPt (int startLevel) {
	int i;
	for (i=startLevel; i<NUM_PRIORITY; i++) {
		if (PriPt[i]){
			NextPt = PriPt[i];
			break;
		}
	}
}


//Wakes up sleeping threads and keep a time in units of 1 ms
// Double linked list

void Timer1A_Handler(void){
	TCB *current;
	int minPriority = NUM_PRIORITY;
	
	#ifdef __Profiling__
	timestamp(Profile_Timer1_Starts);
	#endif
	
  TIMER1_ICR_R = TIMER_ICR_TATOCINT;// acknowledge timer2A timeout
  ++Timer;
	
	// Go through the sleeping threads and wake up any neccesary thread
	if(!SleepPt) {
		#ifdef __Profiling__
		timestamp(Profile_Timer1_End);
		#endif
		
		return;
	}
	current = SleepPt;
	do {
		TCB *currentNext = current->next;
		if(!(--(current->sleepCount))) {
			minPriority = (current->priority < minPriority) ? (current->priority) : minPriority;
			if (current->next == current) {
				SleepPt = NULL;
				// Link to priority list
				if(PriPt[current->priority]) {
					DLLINK(current, PriPt[current->priority]);
				} else {
					current->next = current->prev = current;
					PriPt[current->priority] = current;
				}
				break;
			} else {
				DLUNLINK (current);
				SleepPt = current->next;
				// Link to priority list
				if(PriPt[current->priority]) {
					DLLINK(current, PriPt[current->priority]);
				} else {
					current->next = current->prev = current;
					PriPt[current->priority] = current;
				}
			}
		}
		current = currentNext;
	} while (current != SleepPt);
	if (minPriority < RunPt->priority ) {
		updateNextPt(0);
		triggerPendSV();
	}
	
	#ifdef __Profiling__
	timestamp(Profile_Timer1_End);
	#endif
}


// Timer2 does not cause interrupt

//executes a periodic background thread

#ifdef __Jitter_Measurement__
unsigned static long LastTime1;  // time at previous execution
#define IDLE    0
#define STARTED 1
unsigned char MeasureState1 = IDLE;
#endif

void Timer3A_Handler(void){
#ifdef __Jitter_Measurement__
	long jitter;
	int index;
	unsigned long thisTime = OS_Time();
#endif
	
	#ifdef __Profiling__
	timestamp(Profile_Timer3_Starts);
	#endif
	
  TIMER3_ICR_R = TIMER_ICR_TATOCINT;// acknowledge timer3A timeout
	
	(*PeriodicTask1)();                // execute user task

	#ifdef __Jitter_Measurement__
	// Measure and record jitter
	if(MeasureState1 == STARTED) {
		jitter = (long)(OS_TimeDifference(thisTime,LastTime1)/80)-(long)(Period1/80);  // in usec
		MaxJitter1 = (jitter > MaxJitter1)? jitter: MaxJitter1;
		MinJitter1 = (jitter < MinJitter1)? jitter: MinJitter1;
		index = jitter+JITTERSIZE/2;   // us units
		if(index < 0) index = 0;
		if(index>=JitterSize1) index = JITTERSIZE-1;
		JitterHistogram1[index]++; 
		
  } else { // IDLE
		MeasureState1 = STARTED;
	}
	LastTime1 = thisTime;
	#endif
	
	#ifdef __Profiling__
	timestamp(Profile_Timer3_End);
	#endif		
}

#ifdef __Jitter_Measurement__
unsigned static long LastTime2;  // time at previous execution
unsigned char MeasureState2 = IDLE;
#endif

//executes a periodic background thread
void Timer4A_Handler(void){
#ifdef __Jitter_Measurement__
	long jitter;
	int index;
	unsigned long thisTime = OS_Time();
#endif

	#ifdef __Profiling__
	timestamp(Profile_Timer4_Starts);
	#endif
	
  TIMER4_ICR_R = TIMER_ICR_TATOCINT;// acknowledge timer4A timeout
	
	(*PeriodicTask2)();                // execute user task

#ifdef __Jitter_Measurement__
	// Measure and record jitter
	if(MeasureState2 == STARTED) {
		jitter = (long)(OS_TimeDifference(thisTime,LastTime2)/80)-(long)(Period2/80);  // in usec
		MaxJitter2 = (jitter > MaxJitter2)? jitter: MaxJitter2;
		MinJitter2 = (jitter < MinJitter2)? jitter: MinJitter2;
		index = jitter+JITTERSIZE/2;   // us units
		if(index < 0) index = 0;
		if(index>=JitterSize2) index = JITTERSIZE-1;
		JitterHistogram2[index]++; 
  } else { // IDLE
		MeasureState2 = STARTED;
	}
	LastTime2 = thisTime;
#endif
	
	#ifdef __Profiling__
	timestamp(Profile_Timer4_End);
	#endif	
}

#ifdef __Jitter_Measurement__
void Jitter(void) {
	printf("Timer1 jitter = %ld\r\nTimer2 jitter = %ld\r\n", MaxJitter1 - MinJitter1, MaxJitter2 - MinJitter2);
}
#endif

#undef IDLE
#undef STARTED

// SW1 = PF4
// SW2 = PF0

void static DebounceTask1(void){
	OS_Sleep(1); // foreground sleeping, must run within 50ms
	LastPF4 = PF4; // read while it is not bouncing
	GPIO_PORTF_ICR_R = SW1; // clear flag6
	GPIO_PORTF_IM_R |= SW1; // enable interrupt on PD6
	
	OS_Kill();
}

void static DebounceTask2(void){
	OS_Sleep(1); // foreground sleeping, must run within 50ms
	LastPF0 = PF0; // read while it is not bouncing
	GPIO_PORTF_ICR_R = SW2; // clear flag6
	GPIO_PORTF_IM_R |= SW2; // enable interrupt on PD6
	
	OS_Kill();
}

void GPIOPortF_Handler(void){
	unsigned long vector = GPIO_PORTF_MIS_R;
	if(vector & SW1) {		
		if(LastPF4){ // if previous was high, this is falling edge
			(*SW1Task)(); // execute user task
		}
		
		OS_AddThread(&DebounceTask1, 32, 0);
		GPIO_PORTF_IM_R &= ~SW1; // disarm interrupt on PF0
	
	} else { // SW2
		if(LastPF0){ // if previous was low, this is rising edge
			(*SW2Task)(); // execute user task
		}
		
		OS_AddThread(&DebounceTask2, 32, 0);
		GPIO_PORTF_IM_R &= ~SW2; // disarm interrupt on PF0
	}
}

/********************************* Performance Measurement ************************************/
#ifdef __Critical_Interval_Measurement__

unsigned long MaxCriticalInterval;
unsigned long TotalCriticalInterval;
unsigned long StartCriticalTime;

unsigned int CS_Duration;

long StartCritical_check(void) {
	long sr = StartCritical();
	
	StartCriticalTime = OS_Time();
	
	return sr;
}

void EndCritical_check(long sr) {
	unsigned long endCriticalTime = OS_Time();
	long criticalInterval = (long)(OS_TimeDifference(endCriticalTime,StartCriticalTime)/80); // us
	
	TotalCriticalInterval += criticalInterval;
	MaxCriticalInterval = (criticalInterval > MaxCriticalInterval)? criticalInterval : MaxCriticalInterval;
	
	EndCritical(sr);
}

void EnableInterrupts_check(void) {
	EnableInterrupts();
	StartCriticalTime = OS_Time();
}

void DisableInterrupts_check(void) {
	unsigned long endCriticalTime = OS_Time();
	long criticalInterval = (long)(OS_TimeDifference(endCriticalTime,StartCriticalTime)/80); // unit is 1 us
	
	TotalCriticalInterval += criticalInterval;
	MaxCriticalInterval = (criticalInterval > MaxCriticalInterval)? criticalInterval : MaxCriticalInterval;
	
	DisableInterrupts();

}

Sema4Type Sema4CriticalIntervalReady;

static void CriticalIntervalThread(void) {
	OS_Sleep(CS_Duration); // 30 ms
 	OS_Signal(&Sema4CriticalIntervalReady);
	OS_Kill();
	while(1);
}

void OS_CriticalInterval_Start(int duration) {
	CS_Duration = duration;
	StartCriticalTime = MaxCriticalInterval = TotalCriticalInterval = 0;
	
	OS_InitSemaphore(&Sema4CriticalIntervalReady, 0);
	OS_AddThread(CriticalIntervalThread, 32, 0);
	
	OS_Wait(&Sema4CriticalIntervalReady);
	printf("Maximum time in critical secsion = %ld\r\n" \
	       "Percentage of time in critical section = %ld/%d = %ld.%2ld%%\r\n", \
	       MaxCriticalInterval, TotalCriticalInterval, duration*10000,
	       TotalCriticalInterval/(duration*100), TotalCriticalInterval%(duration*100));	
}

#endif

#ifdef __Profiling__

Sema4Type Sema4ProfilingDone;

unsigned char Profiling;
unsigned long ProfileStartTime;

void timestamp(unsigned char value) {
	if(!Profiling) return;
	
	if(ProfilePt >= &Profile[PROFILELENGTH]) {
		Profiling = 0;
		OS_Signal(&Sema4ProfilingDone);

		return;
	}
	
	ProfilePt->value = value;
	ProfilePt->time = ProfileStartTime - OS_Time();
	ProfilePt++;
	

}

void Profile_Dump(void) {
	TimeStamp *pt;
	
	for(pt = Profile; pt < ProfilePt; ++pt) {
		printf("[%07lu] Thread-%d\r\n", pt->time, pt->value);
	}
}

void OS_Profile_Start(void) {
	Profiling = 1;
	ProfilePt = Profile;
	ProfileStartTime = OS_Time();
	
	OS_InitSemaphore(&Sema4ProfilingDone, 0);
	OS_Wait(&Sema4ProfilingDone);

	Profile_Dump();
}




#endif

