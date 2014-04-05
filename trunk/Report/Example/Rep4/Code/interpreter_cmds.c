/************************* DSP commands **********************/
//Descriptiont: command for turning the filter on or off
// Expects parameter "on" or "off" 
#define NOW_USING   1
#define STOP_USING  0
#define NOT_USING  -1
extern char Filter_Use;

static void adjustFilterCommand(void) {
  char *buffer;
  
  const char *msg = "Error: Enter 'on' or 'off'\r";
  
  if (buffer = strtok(NULL, " ")) {
    if(strcmp(buffer, "on") == 0) {
			Filter_Use =  NOW_USING; 
		} else if(strcmp(buffer, "off") == 0) {
			Filter_Use = STOP_USING;
		} else {
			puts(msg);
		}
  } else {
    puts(msg);
  }
}


// Description: command for changing the fft sampling mode
// Available modes are Once, Button-trigger, and Continuous-trigger
extern char TriggerMode;
extern short Capture;
static void fftCommand(void) {
	char *buffer;
  
  const char *msg = "Error: Enter 'c' (continuous), 'b' (button), or 'o' (once)\r";
  
  if (buffer = strtok(NULL, " ")) {
		if(strcmp(buffer, "c") == 0) {
			TriggerMode = CONTINUOUS;
		} else if(strcmp(buffer, "b") == 0) {
			TriggerMode = BUTTON;
		} else if(strcmp(buffer, "o") == 0) {
			TriggerMode = NOW;
			Capture = 200;
		} else {
			puts(msg);
		}
  } else {
    puts(msg);
  }
}

// Command for performing a FFT once
extern long recording[];
extern long fft_output[];
static void captureCommand(void){
	char *buffer;
  
  const char *msg = "Error: Enter 'fft' or 'voltage' \r";
	
  
  if (buffer = strtok(NULL, " ")) {
    if(strcmp(buffer, "fft") == 0) {
			int i;
			for (i=0;i<64;i++){
				printf("%ld\r\n", fft_output[i]);
			}
		} else if(strcmp(buffer, "voltage") == 0) {
			char i, start;
			i = start = puti;
			do {
				printf("%ld\r\n", recording[i]);
				i = (i+1)%64;
			} while (i != start);
		} else {
			puts(msg);
		}
  } else {
    puts(msg);
  }
}

//Description: command for changing display mode between Time mode and Frequency mode
//Will change the global variable displayMode
extern unsigned char puti;
extern unsigned char geti;

#define TIME      1
#define FREQUENCY 2
extern char DisplayMode;
#define ON 1
#define OFF 0
extern char DisplayIsOn;
static void parseDisplayCommand(void) {
  char *buffer;
  
  const char *msg = "Error: Enter 'time' or 'freq' or 'run' or 'stop'\r";
  
  if (buffer = strtok(NULL, " ")) {
    if(strcmp(buffer, "time") == 0) {
			if (DisplayMode == FREQUENCY){
				long sr;
				sr = StartCritical();
				puti = geti = 0;
				Sema4DataAvailable.Value = 0;
				DisplayMode =  TIME; 
				EndCritical(sr);
			}
		} else if(strcmp(buffer, "freq") == 0) {
			DisplayMode = FREQUENCY;
		} else if(strcmp(buffer, "run") == 0) {
			DisplayIsOn = ON;
		} else if(strcmp(buffer, "stop") == 0) {
			DisplayIsOn = OFF;
		} else {
			puts(msg);
		}
  } else {
    puts(msg);
  }
}

