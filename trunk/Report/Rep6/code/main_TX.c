void NetworkSend(void) {	
	unsigned short IRvalues[4];
	unsigned long sonarValues[4];
	unsigned char CanData[4];
	
	IR_getValues(IRvalues);
	((unsigned short*)CanData)[0] = IRvalues[0];
	CAN0_SendData(IRSensor0, CanData);
	
	Ping_getData (sonarValues);
	((unsigned long*)CanData)[0] = sonarValues[0];
	CAN0_SendData(UltraSonic, CanData);
}

int main(void) {
  PLL_Init();
  OS_Init();
  
  // Initialize sensors
  IR_Init();
  Ping_Init();
  
  // Initialize network
  CAN0_Open();
  
  NumCreated += OS_AddPeriodicThread(&NetworkSend, 100*TIME_1MS, 3); 
  
  OS_Launch(TIMESLICE);
}