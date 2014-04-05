extern void Jitter(void);   // prints jitter information (write this)
void Thread7(void){
  OS_bWait(&Sema4UART);
		UART_OutString("\n\rEE345M/EE380L, Lab 3 Preparation 2\n\r");
  OS_bSignal(&Sema4UART);
	
	OS_Sleep(1000);   // 10 seconds        
	
	OS_bWait(&Sema4UART);
		Jitter();    // print jitter information
		UART_OutString("\n\r\n\r");
	OS_bSignal(&Sema4UART);
  
	OS_Kill();
}

#define workA 500       // {5,50,500 us} work in Task A
#define counts1us 80    // number of OS_Time counts per 1us
void TaskA(void){
  PE1 = 0x02;
  CountA++;
  PseudoWork(workA*counts1us); //  do work (100ns time resolution)
  PE1 = 0x00;
}
#define workB 250       // 250 us work in Task B
void TaskB(void){
  PE2 = 0x04;
  CountB++;
  PseudoWork(workB*counts1us); //  do work (100ns time resolution)
  PE2 = 0x00;
}

int main(void) {
  PLL_Init();
  OS_InitSemaphore(&Sema4UART, 1);
  
  Debug_PortE_Init();
  OS_Init();           
  NumCreated = 0 ;
  NumCreated += OS_AddThread(&Interpreter,128,1);  
	NumCreated += OS_AddThread(&Thread6,128,2); 
  NumCreated += OS_AddThread(&Thread7,128,1); 
	
  OS_AddPeriodicThread(&TaskA,TIME_1MS,0);           
  OS_AddPeriodicThread(&TaskB,2*TIME_1MS,1);         
 
  OS_Launch(TIME_2MS); 
  return 0;             
}