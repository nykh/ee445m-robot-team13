#include "PLL.h"
#include "SysTick.h"
#include "OS.h"
#include "semaphore.h"
#include "can0.h"

#include "ST7735.h"
#include "motor.h"

#include "ir_sensor.h"
#include "ping.h"

#include "interpreter.h"
#include "gpio_debug.h"

#define SAMPLING_RATE 2000
#define TIMESLICE 2*TIME_1MS  // thread switch time in system time units

#define MAIN 1		//1 = receiver, 0 = transmitter

/************************ Debug info ***********************/
unsigned int NumCreated;

/********************** Public functions *******************/

#if MAIN  /******************** Motor Control side *******************/

#define DEBUG_LCD 0
#define DEBUG  0

static long CurrentSpeedR = 0;
static long CurrentSpeedL = 0;
static long RefSpeedR = 0;
static long RefSpeedL = 0;

static long FrontSideError = 0, LastFrontSideError = 0, FrontSideErrorDiff = 0;
static long SideError = 0, LastSideError = 0, SideErrorDiff = 0;
unsigned char SensorF, SensorFPing, SensorR, SensorL, SensorFR, SensorFL;

// Function implementing an incremental controller
//static void IncrementalController( long ref,  long *curr);
typedef enum State_t {GoForward, /* TurnRight, TurnLeft,*/ Stop, SteerRight, SteerLeft, GoBackWard, GoStraight} State;

void Controller(void) {
	static int Time = 0, i = 0;
	static State currentState = GoForward;
	static int counter = 0;
	static long error;
	
	if (Time == 18000) {
		Motor_Stop();
		return;
	} else {
		Time++;
	}
	
	#if   DEBUG_LCD
	#if !DEBUG
	i = (i+1)&0xF;
	switch(i) {
		case 0: ST7735_Message(1,0, "Front IR: ", SensorF); break;
		case 2: ST7735_Message(1,1, "Front Pg: ", SensorFPing); break;
		case 4:	ST7735_Message(1,2, "FroR: ", SensorFR);
		case 6:	ST7735_Message(1,3, "FroL: ", SensorFL);	
		
		case 8:	ST7735_Message(0,0, "Right: ", SensorR);
		case 10:	ST7735_Message(0,1, "Left : ", SensorL);
		
		case 14: ST7735_Message(0,6, "Speed0: ", (unsigned long) RefSpeedR); break;
		case 1: ST7735_Message(0,7, "Speed1: ", (unsigned long) RefSpeedL); break;
		case 12: ST7735_Message(0,2, "Error: ", (long) error); break;
	}
	#endif
	#endif
	
	
	//7C checkout, P = 80, I = 40, D = 0
	#define Fast_Speed    24000
	#define Slow_Speed    12000
	#define Steer_Diff    2000
	#define Speed_lowbound 5000
	#define Steering_Forward_P   200
	#define Steering_Forward_I   10 // Smaller = I term greater
	#define Steering_Forward_D   0
	#define Sterring_Integral_Capacity 40000
	
	#define F_Go2Stop_THRS   30
	#define F_Turn2Go_THRS   F_Go2Stop_THRS+5
	#define FS_Go2Stop_THRS  8
	#define FS_Steer2Go_THRS FS_Go2Stop_THRS+5
	
	#define FRONT            SensorF
	
	//currentState=TurnLeft;
	//currentState = GoForward;
	switch(currentState) {
		static long error_i = 0;
		static long diff_error = 0;
		case GoForward:
			/****************************************/ Debug_LED(RED);
		
			RefSpeedL = RefSpeedR = Fast_Speed;
		
		// + > biasing to right
		// - < biasing to left
		  error = (SideError + FrontSideError)/2;
		  diff_error = (SideErrorDiff + FrontSideErrorDiff)/2;
		// By practical observation: the value of error is in the range [-255, 255]

			error_i += error;
			if (error_i > Sterring_Integral_Capacity) error_i = Sterring_Integral_Capacity;
			if (error_i < -Sterring_Integral_Capacity) error_i = -Sterring_Integral_Capacity;
			
			if (error > 0 ) {
				RefSpeedL -= error * Steering_Forward_P; // + error_i / Steering_Forward_I - diff_error * Steering_Forward_D;
			} else {
				RefSpeedR += error * Steering_Forward_P; // + error_i / Steering_Forward_I - diff_error * Steering_Forward_D;
			}
			
			if (RefSpeedR < Fast_Speed - Speed_lowbound) RefSpeedR = Fast_Speed - Speed_lowbound;
			if (RefSpeedL < Fast_Speed - Speed_lowbound) RefSpeedL = Fast_Speed - Speed_lowbound;
			
			CurrentSpeedR=RefSpeedR;
			CurrentSpeedL=RefSpeedL;
			
			// State change
			
			// Emergency
			/*if (FRONT < F_Go2Stop_THRS || SensorFR < FS_Go2Stop_THRS || SensorFL < FS_Go2Stop_THRS) {
				error_i = 0;
				currentState = Stop; break;
			}*/
			
			
			//Sterring
			if (FRONT < F_Go2Stop_THRS || SensorFR < FS_Go2Stop_THRS || SensorFL < FS_Go2Stop_THRS) {
				currentState = Stop;
				/*
				if (SideError < -5) {
					currentState = SteerRight;
				} else if (SideError > 5) {
					currentState = SteerLeft;
				} else if (FrontSideError > 0) {
					currentState = SteerLeft;
				} else {
					currentState = SteerRight;					
				}*/
				counter = 0;
				error_i = 0; break;
			}
			// Normal : equal
			/*if(FRONT < F_Go2Steer_THRS) {
				if(SensorR > SensorL) {
					if(SensorR >= S_Go2Steer_THRS) {
						error_i = 0;
						currentState = SteerRight;
					}
				} else {
					if (SensorL >= S_Go2Steer_THRS) {
						error_i = 0;
						currentState = SteerLeft;
					}
				}
			}*/
			break;
			
//		case TurnRight:
//			if (counter++ == 100) {
//				counter = 0;
//				currentState = GoBackWard;
//			}
//			/****************************************/ Debug_LED(YELLOW);
//		
//			//RefSpeedL = Slow_Speed;
//			//RefSpeedR = Slow_Speed - (FRONT_THRESH-SensorF)*Steering_Turn;
//			//RefSpeedR = Slow_Speed - 6000;
//		
//			RefSpeedR = -Turn_Speed + 2000;
//			RefSpeedL = Turn_Speed + 2000;
//			IncrementalController(RefSpeedR, &CurrentSpeedR);
//			IncrementalController(RefSpeedL, &CurrentSpeedL);

//			// State change
//			if (FRONT > F_Turn2Go_THRS && SensorFR > FS_Go2Stop_THRS+5 && SensorFL > FS_Go2Stop_THRS+5 ){  
//				currentState = GoForward;
//				counter = 0;
//			}

//			break;
//			
//		case TurnLeft:
//			/****************************************/ Debug_LED(BLUE);
//			if (counter++ == 100) {
//				counter = 0;
//				currentState = GoBackWard;
//			}
//		
////			RefSpeedL = Slow_Speed;
////			RefSpeedR = Slow_Speed + (FRONT_THRESH-SensorF)*Steering_Turn;
////			RefSpeedR = Slow_Speed + 6000;		
////			if (RefSpeedR > 25000) RefSpeedR = 25000;
////			if (RefSpeedR < 0) {
////				RefSpeedR = 0;
////			}
//			RefSpeedR = +Turn_Speed;
//			RefSpeedL = -Turn_Speed - 1500;
//			IncrementalController(RefSpeedR, &CurrentSpeedR);
//			IncrementalController(RefSpeedL, &CurrentSpeedL);

//			// State change
//			if (FRONT > F_Turn2Go_THRS && SensorFR > FS_Go2Stop_THRS+5 && SensorFL > FS_Go2Stop_THRS+5 ) {
//				counter = 0;
//				currentState = GoForward;
//			}
//			break;
//			
		case Stop:
			/****************************************/ Debug_LED(BLUE);
		
		  // Stoping the wheels
			RefSpeedR = RefSpeedL = 0;
			CurrentSpeedL = CurrentSpeedR = 0;
		
		  if (counter  == 50) {
				if (FRONT < F_Turn2Go_THRS || SensorFR < FS_Go2Stop_THRS || SensorFL < FS_Go2Stop_THRS ) {
					if (SensorR > SensorL + 5) {
						currentState=SteerRight;
					} else if (SensorL > SensorR + 5) {
						currentState=SteerLeft;
					} else if (SensorFL < SensorFR) {
						currentState=SteerRight;
					} else {
						currentState=SteerLeft;						
					}
				} else {
					currentState = GoForward;
				}
				counter = 0;
			} else {
				counter ++;
			}
			break;
			
		case SteerRight:
			/****************************************/ Debug_LED(GREEN);
			if (counter++ == 100) {
				counter = 0;
				currentState = GoBackWard;
			}
			RefSpeedR = Fast_Speed/2;
			RefSpeedL = Fast_Speed;
		
			CurrentSpeedR = RefSpeedR;
			CurrentSpeedL = RefSpeedL;
			
			// State change
			//if (FRONT >= F_Steer2Go_THRS) currentState = GoForward;
			if (FRONT > F_Turn2Go_THRS && SensorFR > FS_Steer2Go_THRS && SensorFL > FS_Steer2Go_THRS ){
				currentState = GoStraight; counter = 0;
			}
			break;
			
		case SteerLeft:
			/****************************************/ Debug_LED(PURPLE);
			if (counter++ == 100) {
				counter = 0;
				currentState = GoBackWard;
			}
			RefSpeedR = Fast_Speed;
			RefSpeedL = Fast_Speed/2;
		
			CurrentSpeedR = RefSpeedR;
			CurrentSpeedL = RefSpeedL;
			
			// State change
			//if (FRONT >= F_Steer2Go_THRS) currentState = GoForward;
			if (FRONT > F_Turn2Go_THRS && SensorFR > FS_Steer2Go_THRS && SensorFL > FS_Steer2Go_THRS ) {
				currentState = GoStraight; counter = 0;
			}
			break;
		
		case GoBackWard:
			/****************************************/ Debug_LED(VIOLET);
			if (counter++ == 50) {
				counter = 0;
				currentState = GoForward;
			}
			RefSpeedR = -Fast_Speed/2;
			RefSpeedL = -Fast_Speed/2;
		
			CurrentSpeedR = RefSpeedR;
			CurrentSpeedL = RefSpeedL;
			break;
			
			case GoStraight:
			/****************************************/ Debug_LED(WHITE);
			if (counter++ == 60) {
				counter = 0;
				currentState = GoForward;
			}
			RefSpeedR = Fast_Speed;
			RefSpeedL = Fast_Speed;
		
			CurrentSpeedR = RefSpeedR;
			CurrentSpeedL = RefSpeedL;
			
			//Sterring
			if (FRONT < F_Go2Stop_THRS || SensorFR < FS_Go2Stop_THRS || SensorFL < FS_Go2Stop_THRS) {
				if (SideError < -5) {
					currentState = SteerRight;
				} else if (SideError > 5) {
					currentState = SteerLeft;
				} else if (FrontSideError > 0) {
					currentState = SteerLeft;
				} else {
					currentState = SteerRight;					
				}
				counter = 0;
				error_i = 0; break;
			}
			break;
			
	}
	
	//Motor_MotionUpdate(Fast_Speed, Fast_Speed);
	Motor_MotionUpdate(CurrentSpeedR, CurrentSpeedL);
}


static void NetworkReceive(void) {
	PackageID receiveID;
	unsigned char canData[4];
	Debug_LED_Init();
	CAN0_Open();
	Motor_Init(2);
	RefSpeedR = RefSpeedL = 3000;
	while (1) {
		CAN0_GetMail(&receiveID, canData);	
		
		if (receiveID == IRSensor) {			
				SensorF = canData[3];
				SensorL = canData[2];
				SensorR = canData[0];
				LastSideError = SideError;
				SideError = (long) SensorL - (long) SensorR;
				//SideError = 30 - (long) SensorR;
				SideErrorDiff = SideError - LastSideError;
			
			#if   DEBUG_LCD
			#if DEBUG
				ST7735_Message(0,0,"IR0: ", canData[0]);
				ST7735_Message(0,1,"IR1: ", canData[1]);
				ST7735_Message(0,2,"IR2: ", canData[2]);
				ST7735_Message(0,3,"IR3: ", canData[3]);
			#endif
			#endif
			
		} else if(receiveID == PingSensor) {
				SensorFL = canData[0];
				SensorFR = canData[1];
				SensorFPing = canData[2];
			
				LastFrontSideError = FrontSideError;
			  FrontSideError = (long) SensorFL - (long) SensorFR;
				//FrontSideError = 45 - (long) SensorFR;
				FrontSideErrorDiff = FrontSideError - LastFrontSideError;
				
			#if   DEBUG_LCD
			#if DEBUG
				ST7735_Message(1,0, "Ping0: ", canData[0]);
				ST7735_Message(1,1, "Ping1: ", canData[1]);
				ST7735_Message(1,2, "Ping2: ", canData[2]);
				ST7735_Message(1,3, "Ping3: ", canData[3]);
			#endif
			#endif
				
		}
		
		
	}
}

//#define INC_STEP  700
//static void IncrementalController( long ref,  long *curr) {
//	*curr = ref;
//	return;
//	if (*curr > ref + INC_STEP ) {
//		  *curr -= INC_STEP;
//	} else if (*curr < ref - INC_STEP) {
//			*curr += INC_STEP;
//	} else *curr = ref;
//}

int main(void) {
  PLL_Init();
  
	#if   DEBUG_LCD
  ST7735_InitR(INITR_REDTAB);
	ST7735_SetRotation(1);
  ST7735_FillScreen(0); // set screen to black
	#endif

	/*********************************************/Debug_LED_Init();

  OS_Init();
  
  NumCreated = 0;
  //NumCreated += OS_AddThread(&Interpreter,128,1); 
  NumCreated += OS_AddThread(&NetworkReceive,128,1); 
  NumCreated += OS_AddPeriodicThread(&Controller, 10*TIME_1MS, 3); 
  
  OS_Launch(TIMESLICE);
}



#else  /******************* Receiver = DAS side *******************/

unsigned char IRvalues[4];
unsigned char pingValue[4];


void NetworkSend(void) {	
	unsigned char CanData[4];
	
	// may Block
	IR_getValues(IRvalues);
	CanData[0] = IRvalues[0];
	CanData[1] = IRvalues[1];
	CanData[2] = IRvalues[2];
	CanData[3] = IRvalues[3];
	CAN0_SendData(IRSensor, CanData);
	
	CanData[0] = pingValue[0];
	CanData[1] = pingValue[1];
	CanData[2] = pingValue[2];
	CanData[3] = pingValue[3];
	CAN0_SendData(PingSensor, CanData);
	// perform distance conversion
}

unsigned long sensor_fail[2] = {0,};

void PingSensorSend(void) {
//	unsigned char CanData[4];
	
		Ping_Init();
	
	while(1) {
		// may block
		if(PingValue(&pingValue[0], 0)) sensor_fail[0]+=1;
		if(PingValue(&pingValue[1], 1)) sensor_fail[1]+=1;
//		if(PingValue(&pingValue[2], 2)) sensor_fail[2]+=1;
//		if(PingValue(&pingValue[3], 3)) sensor_fail[3]+=1;
	}
}


int main(void) {
  PLL_Init();
  
  OS_Init();
	
	CAN0_Open();
	OS_InitSemaphore(&Sema4CAN, 1);
	
	Debug_LED_Init();
	
  NumCreated = 0;
  //NumCreated += OS_AddThread(&Interpreter,128,1);
	IR_Init(); 
	NumCreated += OS_AddPeriodicThread(&NetworkSend, 10*TIME_1MS, 3); 
	NumCreated += OS_AddThread(&PingSensorSend, 128, 3);
	
//	Ping_Init();
  OS_Launch(TIMESLICE);
}

#endif

	/*if (SensorF >= 100) {
		RefSpeedR = RefSpeedL = Fast_Speed;
	} else if (SensorF >= 50) {
		RefSpeedR = RefSpeedL = Slow_Speed;
	} else {
		if (SensorR >= 50) {    //Front is block, right is free
					RefSpeedR = 12000 - (50-SensorF)*Steering;
					if (RefSpeedR & 0x80000000) {
						RefSpeedR = 0;
					}
					RefSpeedL = 12000;
		} else {
			RefSpeedL = 12000;
			RefSpeedR = 12000 + RightAngle*50;
			if (RefSpeedR > 25000) RefSpeedR = 25000;
		}
	}*/
