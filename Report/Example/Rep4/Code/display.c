char DisplayIsOn = ON;
void Display (void) {
	static char LastMode = FREQUENCY;
	while (1) {
		OS_Wait(&Sema4DataAvailable);
		if (DisplayMode == TIME) {		// DisplayMode is in time Domain
			if (LastMode != DisplayMode) {
				ST7735_FillScreen(0); // set screen to black
				ST7735_DrawFastVLine(19,10,100,ST7735_Color565(255,255,41));
				ST7735_DrawFastHLine(19,110,140,ST7735_Color565(255,255,41));
				ST7735_PlotClear(0,4095); 
			}	
			if (DisplayIsOn){
				ST7735_PlotPoint(recording[geti]);
				ST7735_PlotNext();
			}
			geti = (geti + 1) % FFT_LENGTH; 
			LastMode = TIME;
		} else if(DisplayMode == FREQUENCY) {	// DisplayMode is in frequency Domain
			int i;
			/* Display the frequency */
			if (LastMode != FREQUENCY) {
				ST7735_FillScreen(0); // set screen to black
				ST7735_DrawFastVLine(19,10,100,ST7735_Color565(255,255,41));
				ST7735_DrawFastHLine(19,110,140,ST7735_Color565(255,255,41));
				ST7735_PlotClear(0,4095); 
			}
			 ST7735_FillRect(21, 10, 140, 99, ST7735_BLACK);
			if (DisplayIsOn){
				ST7735_PlotReset();
				for (i=0;i<FFT_LENGTH;i++) {
					ST7735_PlotBar(fft_output[i]);
					ST7735_PlotNext();
				}
			}
			LastMode = FREQUENCY;
		}
	}
}