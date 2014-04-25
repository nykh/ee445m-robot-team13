#include "PLL.h"
#include "SysTick.h"
#include "OS.h"
#include "semaphore.h"
#include "can0.h"

#include "ST7735.h"
#include "motor.h"
#include "atan.h"

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

#define DEBUG_LCD 1
#define DEBUG  0

static long CurrentSpeedR = 0;
static long CurrentSpeedL = 0;
static long RefSpeedR = 0;
static long RefSpeedL = 0;

unsigned char SensorF, SensorR, SensorL, SensorFR, SensorFL, RightAngle;

// Function implementing an incremental controller
static void IncrementalController( long ref,  long *curr);
typedef enum State_t {GoForward, TurnRight, TurnLeft, Stop, SteerRight, SteerLeft} State;

void Controller(void) {
	static State currentState = GoForward;
	
	#if   DEBUG_LCD
	#if !DEBUG
	static int i = 0;
	i = (i+1)&0x7;
	switch(i) {
		case 0: ST7735_Message(1,0, "Front: ", SensorF); break;
		case 1: ST7735_Message(1,2, "Right: ", SensorR); break;
		case 2: ST7735_Message(1,1, "Left: ", SensorL); break;
		case 3:	ST7735_Message(1,3, "FroRight: ", SensorFR);
		
		case 4: ST7735_Message(0,0, "Speed0: ", (unsigned long) RefSpeedR); break;
		case 5: ST7735_Message(0,1, "Speed1: ", (unsigned long) RefSpeedL); break;
	}
	#endif
	#endif
	
	#define Fast_Speed  18000
	#define Slow_Speed  14000
	#define Steering_Turn    150
	#define Steer_Diff       3000
	#define Steering_Forward_P   30
	#define Steering_Forward_I   10
	#define Steering_Forward_I_CAP  1000
	#define BASE_TURN			8000
	
	#define F_Go2Stop_THRS   35
	#define F_Turn2Go_THRS   F_Go2Stop_THRS+5
	#define F_Go2Steer_THRS  60
	#define F_Steer2Go_THRS  F_Go2Stop_THRS+20
	#define SIDE_THRS        30
	#define S_Go2Steer_THRS  50
	
	//currentState=TurnLeft;
	switch(currentState) {
		static long integrated_error = 0;
		long error;
		
		case GoForward:
			/****************************************/ Debug_LED(RED);
		
			RefSpeedL = Fast_Speed;
		
//			if (SensorL > SIDE_THRESH+10) {
//				RefSpeedR = Fast_Speed + ( 30 - (long) SensorR) * Steering_Forward_P \
//															 + ( 45 - (long) SensorFR) * Steering_Forward_P;
//			} else if (SensorR > SIDE_THRESH+10) {
//				RefSpeedR = Fast_Speed + ( (long) SensorL - 30) * Steering_Forward_P	\
//															 + ( (long) SensorL - 45) * Steering_Forward_P;
//			} else {
//				RefSpeedR = Fast_Speed + ((long) SensorL - (long) SensorR) * Steering_Forward_P \
//															 + ((long) SensorFL - (long) SensorFR) * Steering_Forward_P;
//			}
		
		// + > biasing to right
		// - < biasing to left
		  error = ((long) SensorL - (long) SensorR) + ((long) SensorFL - (long) SensorFR);
		// By practical observation: the value of error is in the range [-255, 255]
		// therefore the next capping statements are not necessary.
		
		// Not necessary
//			if (error > Steering_Forward_I_CAP) error = Steering_Forward_I_CAP;
//			if (error < -Steering_Forward_I_CAP) error = -Steering_Forward_I_CAP;
			integrated_error += error;
			
			RefSpeedR = Fast_Speed + error * Steering_Forward_P + integrated_error / Steering_Forward_I;
			
			if (RefSpeedR > Fast_Speed + 6000) RefSpeedR = Fast_Speed + 6000;
			if (RefSpeedR <Fast_Speed - 6000) RefSpeedR = Fast_Speed - 6000;
			
			if (CurrentSpeedL == RefSpeedL) {
				IncrementalController(RefSpeedR, &CurrentSpeedR);
			} else {
				IncrementalController(RefSpeedL, &CurrentSpeedL);
				CurrentSpeedR = CurrentSpeedL;
			}
			
			// the next two lines are meaningless
//			if (RefSpeedR > 25000) RefSpeedR = 25000;
//			if (RefSpeedR < 0) RefSpeedR = 0;
			
			
			// State change
			
			// Emergency
			if (SensorF < F_Go2Stop_THRS) {
				currentState = Stop; break;
			}
			
			// Normal : equal
			if(SensorF < F_Go2Steer_THRS) {
				if(SensorR > SensorL) {
					if(SensorR >= S_Go2Steer_THRS) {
						currentState = SteerRight;
					}
				} else {
					if (SensorL >= S_Go2Steer_THRS) {
						currentState = SteerLeft;
					}
				}
			}
			break;
			
		case TurnRight:
			/****************************************/ Debug_LED(YELLOW);
		
			//RefSpeedL = Slow_Speed;
			//RefSpeedR = Slow_Speed - (FRONT_THRESH-SensorF)*Steering_Turn;
			//RefSpeedR = Slow_Speed - 6000;
//			i
		
			RefSpeedR = -BASE_TURN + 2000;
			RefSpeedL = BASE_TURN + 2000;
			IncrementalController(RefSpeedR, &CurrentSpeedR);
			IncrementalController(RefSpeedL, &CurrentSpeedL);

			// State change
			if (SensorF > F_Turn2Go_THRS) currentState = GoForward;

			break;
			
		case TurnLeft:
			/****************************************/ Debug_LED(BLUE);
		
//			RefSpeedL = Slow_Speed;
//			RefSpeedR = Slow_Speed + (FRONT_THRESH-SensorF)*Steering_Turn;
//			RefSpeedR = Slow_Speed + 6000;		
//			if (RefSpeedR > 25000) RefSpeedR = 25000;
//			if (RefSpeedR < 0) {
//				RefSpeedR = 0;
//			}
			RefSpeedR = +BASE_TURN;
			RefSpeedL = -BASE_TURN - 1500;
			IncrementalController(RefSpeedR, &CurrentSpeedR);
			IncrementalController(RefSpeedL, &CurrentSpeedL);

			// State change
			if (SensorF > F_Turn2Go_THRS) currentState = GoForward;
		
			break;
			
		case Stop:
			/****************************************/ Debug_LED(WHITE);
		
			RefSpeedR = RefSpeedL = 0;
			IncrementalController(RefSpeedR, &CurrentSpeedR);
			CurrentSpeedL = CurrentSpeedR;
		  if (CurrentSpeedR == 0) {
				if (SensorF < F_Turn2Go_THRS) {
					if (SensorFR< SensorFL - 5) {
						currentState=TurnLeft; break; 
					}
					if (SensorFL < SensorFR - 5) {
						currentState=TurnRight; break; 
					}
					if (SensorR>SensorL) {
						currentState=TurnRight; break; 
					} else {
						currentState=TurnLeft; break; 
					} 
				} else {
				currentState = GoForward;
				}
			} 
			break;
			
		case SteerRight:
			/****************************************/ Debug_LED(GREEN);
		
			RefSpeedR = Slow_Speed - Steer_Diff;
			RefSpeedL = Slow_Speed;
		
			if (RefSpeedL == Slow_Speed) {
				IncrementalController(RefSpeedR, &CurrentSpeedR);
			} else {
				IncrementalController(RefSpeedL, &CurrentSpeedL);
				CurrentSpeedR = CurrentSpeedL;
			}
			
			// State change
			if (SensorF >= F_Steer2Go_THRS) currentState = GoForward;
			if (SensorF <  F_Go2Stop_THRS) currentState = Stop;
			break;
			
		case SteerLeft:
			/****************************************/ Debug_LED(PURPLE);
		
			RefSpeedR = Slow_Speed;
			RefSpeedL = Slow_Speed- Steer_Diff;
			if (RefSpeedR == Slow_Speed) {
				IncrementalController(RefSpeedL, &CurrentSpeedL);
			} else {
				IncrementalController(RefSpeedR, &CurrentSpeedR);
				CurrentSpeedL = CurrentSpeedR;
			}
			
			// State change
			if (SensorF >= F_Steer2Go_THRS) currentState = GoForward;
			if (SensorF < F_Go2Stop_THRS) currentState = Stop;
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
	Motor_Init(5000);
	Motor_0_MotionUpdate(0, 1);
	Motor_1_MotionUpdate(0, 1);
	RefSpeedR = RefSpeedL = 3000;
	while (1) {
		CAN0_GetMail(&receiveID, canData);	
		
		if (receiveID == IRSensor) {			
				SensorF = canData[3];
				SensorL = canData[2];
				SensorR = canData[0];
			
			#if   DEBUG_LCD
			#if DEBUG
				ST7735_Message(0,0,"IR0: ", canData[0]);
				ST7735_Message(0,1,"IR1: ", canData[1]);
				ST7735_Message(0,2,"IR2: ", canData[2]);
				ST7735_Message(0,3,"IR3: ", canData[3]);
			#endif
			#endif
			
				RightAngle = (unsigned char) (myatan((double)SensorR / (double)SensorF)*180/3.14 + 0.5);

		} else if(receiveID == PingSensor) {
				SensorFL = canData[0];
				SensorFR = canData[1];
				
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

#define INC_STEP  3000
static void IncrementalController( long ref,  long *curr) {
	if (*curr > ref + INC_STEP ) {
		  *curr -= INC_STEP;
	} else if (*curr < ref - INC_STEP) {
			*curr += INC_STEP;
	} else *curr = ref;
}

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
  NumCreated += OS_AddPeriodicThread(&Controller, 50*TIME_1MS, 3); 
  
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
	
//		OS_bWait(&Sema4CAN);
//		((unsigned short*)CanData)[0] = IRvalues[0];
//		CAN0_SendData(IRSensor0, CanData);
//	  OS_bSignal(&Sema4CAN);
}

unsigned long sensor_fail[4] = {0,};

void PingSensorSend(void) {
//	unsigned char CanData[4];
	
		Ping_Init();
	
	while(1) {
		// may block
		if(PingValue(&pingValue[0], 0)) sensor_fail[0]+=1;
		if(PingValue(&pingValue[1], 1)) sensor_fail[1]+=1;
		if(PingValue(&pingValue[2], 2)) sensor_fail[2]+=1;
		if(PingValue(&pingValue[3], 3)) sensor_fail[3]+=1;
	}
	
//		OS_bWait(&Sema4CAN);
//		((unsigned long*)CanData)[0] = pingValue[pingNum];	
//		CAN0_SendData(PingSensor | pingNum, CanData);
//	  OS_bSignal(&Sema4CAN);
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
	NumCreated += OS_AddPeriodicThread(&NetworkSend, 50*TIME_1MS, 3); 
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
