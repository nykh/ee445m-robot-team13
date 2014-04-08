void NetworkReceive(void) {
	PackageID receiveID;
	unsigned char canData[4];
	
	// Initialize network
	CAN0_Open();
	
	while (1) {
		CAN0_GetMail(&receiveID, canData);
		switch(receiveID) {
			case IRSensor0:
				ST7735_Message(0,0,"IR0: ", ((unsigned short *)canData)[0]);
				dataReceived++;
			break;
			case UltraSonic:
				ST7735_Message(0,1,"ULS0: ", ((unsigned long *)canData)[0]);
			break;
			default:
			break;
		}
	}
}

int main(void) {
  PLL_Init();
  OS_Init(); 
  
  // Initialize Display
  ST7735_InitR(INITR_REDTAB);
  ST7735_SetRotation(1);
  ST7735_FillScreen(0);
  
  NumCreated += OS_AddThread(&NetworkReceive,128,1); 
  
  OS_Launch(TIMESLICE);
}