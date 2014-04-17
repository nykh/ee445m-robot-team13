// interpreter.c
// written By Siavash Zangeneh * Nicholas Huang
// Functions that the interpreter uses to parse Input strings and execute the appropriate functions 

#include "io.h"
#include "UART2.h"
#include <string.h>
#include "ST7735.h"

#include "OS.h"
#include "semaphore.h"

// Commands come from these sources
#include "heap_debug.h"

//ASCII codes for keyboard inputs
#define BACKSPACE               0x08  // back up one character
#define DEL                     0x7F  // back up one character
#define TAB                     0x09  // move cursor right
#define LF                      0x0A  // move cursor all the way left on current line
#define HOME                    0x0A  // move cursor all the way left on current line
#define NEWLINE                 0x0D  // move cursor all the way left on next line


long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value

/*************************** Private Functions ******************************/
static void Interpreter_ParseInput(char *input);
static char *getline(char *str, unsigned short length);

//Structure that is used to create the table that holds all the commands
typedef struct {
	char command[10];
	void(*functionPt) (void);
} commandTable;


// Commands
static void parseLCDCommand (void);
static void helpList (void);

static void parseMotorCommand(void);
/* System tasks */
//static void adjustFilterCommand(void);

/* Debugging functions */
#ifdef __Performance_Measurement__
	#ifdef __Critical_Interval_Measurement__
		static void parseCriticalSectionCommand(void);
	#endif
	#ifdef __Profiling__
		static void parseProfilingCommand(void);
	#endif
#endif

//function protoypes for private functions
static long stringToInteger (char *string);

//Commands table, edit both the array and element numbers to update the table
#define REAL_NUM_COMMANDS 4

#ifdef __Performance_Measurement__
#define NUM_COMMANDS REAL_NUM_COMMANDS+2
#else
#define NUM_COMMANDS REAL_NUM_COMMANDS
#endif

static const commandTable Table[NUM_COMMANDS]= {
	{"lcd", &parseLCDCommand},
	{"help", &helpList},
	{"heap", &Heap_dump},
	
	{"motor", &parseMotorCommand},
	
#ifdef __Performance_Measurement__
	#ifdef __Critical_Interval_Measurement__
	// Critical Section Measurements
	{"cs", &parseCriticalSectionCommand},
	#endif
	#ifdef __Profiling__
	// Profiling measurement
	{"pf", &parseProfilingCommand},
	#endif
#endif
};

//Description: private function that parses the "help" parameters and executes LCD functions
//Input: None
//Output: none
static void helpList () {
	printf( \
	"1- lcd: parameters= device, line, output string\r\n" \
	"2- heap: dump heap\r\n" \
	
	"3- motor: motor setting\r\n" \
	
	"d1- cs: parameters= duration of measurement\r\n" \
	"d2- pf: clear, start profiling and dump result\r\n" \
	);
	//"5- filter: turn FIR filter on/off. \r\n" 
}

void Interpreter(void) {
	char string[50];
	UART_Init();
	OS_InitSemaphore(&Sema4UART, 1);
	
	OS_bWait(&Sema4UART);	
		printf("\r\nWelcome to OS NS ....\r\n");
	OS_bSignal(&Sema4UART);
	for(;;) {		
		OS_bWait(&Sema4UART);
			printf("Enter Command -> ");
			getline(string, 50);
			Interpreter_ParseInput(string);
		OS_bSignal(&Sema4UART);
	}
}

//Public Function
//Description: this function parses the input to the interpreter and calls the appropriate function
//Input: String that holds the complete input line
//Output: None
static void Interpreter_ParseInput(char *input) {
	char* buffer; int i;
	buffer = strtok(input, " ,");									// parsing the first token
	for (i=0;i<NUM_COMMANDS;i++) {								// Iterating through the command table to  
		if ( ! strcmp(buffer, Table[i].command)) {
			Table[i].functionPt();										// Calling the appropriate function in the table
			return;
		}
	}
	printf("command is invalid!\r\n");
	return;
}

//Description: private function that converts a string to long integer
//Input: String pointer, string should represent an unsigned long
//Output: returns the long integer. returns -1 in case of error
static long stringToInteger (char *string){
	int result=0;
	while(*string != '\0'){
		if (*string < '0' || *string > '9') {
			return -1;
		}
		result = result*10 + (int) ((*string)-'0') ;
		string ++;
	}
	return result;
}

//Description: Public function to get a single line from Putty. It will output every input character back to the screen
//Input 1: pointer to the array that holds the inputs
//Input 2: maximum length of the charachter array
//Output: returns the input pointer
#define GETC()   getc(stdin)
#define PUTC(c)  fputc(c, stdin)
static char *getline(char *str, unsigned short length) {
	int c;
	char *str2 = str;
	
	// continue parsing new characters until reaching newline or maximum length
	while((c = GETC()) != EOF && c != NEWLINE && length > 0) {  
	  if(c == DEL) {  			// if backspace is pressed
			if(str2 > str) {		// only delete a character if the cursor has moved from the first position 
				--str2;
				PUTC(c);
				++length;
			}
		} else {							// puts a new character into buffer
			PUTC(c);
			*str2++ = c;
			--length;
		}
	}
	
	PUTC('\r'), PUTC('\n');
	*str2 = '\0';
	
	return str;
}

#undef GETC
#undef PUTC


//Description: private function that parses the "lcd" parameters and executes LCD functions
//Input: None, though strtok must have been called to get the first token
//Output: none
static void parseLCDCommand () {
	char* buffer; 
  short device, line;
	
	const char *msg = "Error: Enter a valid line number as the %s parameter!\r\n";
	
	buffer = strtok(NULL, " ");						// get the first token
	if (buffer != NULL) {
		device = stringToInteger (buffer);	
		if (device <0 || device > 1) {			// check if device number is valid
			printf(msg, "first");
			return;
		}
	} else{
		printf(msg, "first");
		return;
	}
	buffer = strtok(NULL, " ");						// get the second token
	if ( buffer != NULL) {
		line = stringToInteger (buffer);
		if (line < 0 || line > 7){					// check if line number is valid
		  printf(msg, "second");
			return;
		}
	} else{
		printf(msg, "second");
		return;
	}
	buffer = strtok(NULL, " ") ;					// get the third token
	if ( buffer != NULL) {
		ST7735_Message (device, line, buffer, 0);		// This function is executed if all inputs are correct
		printf("Printed the string on LCD\r\n");	
	} else{
		printf(msg, "third");
		return;
	}
}

//Description: private function that parses the "motor" parameters
//Input: None, though strtok must have been called to get the first token
//Output: none
static void parseMotorCommand(void) {
	char* buffer; 
	
	puts("Not implemented.\r");
}

/************************* DSP commands **********************/
//Descriptiont: command for turning the filter on or off
// Expects parameter "on" or "off" 
/*#define NOW_USING   1
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
#undef NOW_USING
#undef STOP_USING
#undef NOT_USING
*/

#ifdef __Critical_Interval_Measurement__

//Description: command for parsing Critical Section measurement requests
// Can invokde OS_Critical_Dump, OS_Critical_Start
//Input: None
//Output: none
static void parseCriticalSectionCommand(void) {
	char *buffer;
	int n;
	
	const char *msg = "Error: Enter a positive number n, program will measure for n*10ms.\r";
	
	if (buffer = strtok(NULL, " ")) {
		if((n = (int)stringToInteger(buffer)) > 0) {
			OS_CriticalInterval_Start(n);
		} else {
			puts(msg);
		}
	} else{
		puts(msg);
		return;
	}
}

#endif

#ifdef __Profiling__

//Description: command for parsing Profiling requests
// Can invokde OS_Profile_Start and, OS_Profile_Clear, OS_Profile_Dump
//Input: None
//Output: none
static void parseProfilingCommand(void) {	
	puts("Begin Profiling for 100 samples...\r\n");
	OS_Profile_Start();
}

#endif
