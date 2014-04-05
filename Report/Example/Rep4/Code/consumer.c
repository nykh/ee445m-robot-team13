// Consumer
// Foreground thread that takes in data from FIFO, apply filter, and record data
// If trigger Capture is set, it will perform a 64-point FFT on the recorded data
// and store result in fft_output[]
// Block when the FIFO is empty
unsigned long fft_output[FFT_LENGTH];

long recording[2*FFT_LENGTH];
unsigned char puti = 0;
unsigned char geti = 0;

#define SAMPLING_RATE 2000

void Consumer(void) {
  ADC_Collect(0, SAMPLING_RATE, dummy, 64); // Timer triggered ADC sampling
  while (1) {
		// Get data, will block if FIFO is empty
		long data = OS_Fifo_Get();
			
		// Choosing whether to apply the filter
		if(Filter_Use == NOW_USING) data = Filter(data);
		else if(Filter_Use == STOP_USING) {
			Filter_Use = NOT_USING;
			Filter_ClearMemo();
		}
		
		// record data
		recording[puti] = recording[puti+FFT_LENGTH] = data;
		puti = (puti + 1) % FFT_LENGTH;
		
		if(puti == geti) { // a data point is just put in, so in this case it is overrun
		  DataLost += 1;
		  geti = (geti + 1) % FFT_LENGTH; // discard the oldest value
		} else {
			if (DisplayMode == TIME ) {			
				OS_Signal(&Sema4DataAvailable);
			}
		}
		
		if(Capture == DO_CAPTURE) {
			short real; long imag;
			int k;
			Capture = NO_CAPTURE; // Acknowledge
			cr4_fft_64_stm32(fft_output, &recording[puti-FFT_LENGTH+1], FFT_LENGTH);
			for (k=0;k<FFT_LENGTH;k++) {
				real = (short) (fft_output[k] & 0x0000FFFF);
				imag= fft_output[k] >> 16;
				fft_output[k] = sqrt((long)(real)*(long)(real)+imag*imag);
			}
			if (DisplayMode == FREQUENCY)
				OS_Signal(&Sema4DataAvailable);
		}
		
		if(TriggerMode == CONTINUOUS) ++Capture;
	}
}


/****** Background Button Task ***/
// In Button Triggering mode, it trigger the Consumer to perform one fft
void ButtonTask(void) {
	if(TriggerMode == BUTTON) Capture = DO_CAPTURE;
}
