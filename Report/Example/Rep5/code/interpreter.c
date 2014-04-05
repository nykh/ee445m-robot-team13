// interpreter.c
// written By Siavash Zangeneh * Nicholas Huang
// Functions that the interpreter uses to parse Input strings and execute the appropriate functions 

#include "io.h"
#include "UART2.h"
#include <string.h>
#include "efile.h"

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

//FileSystem commands
static void parseRemoveCommand(void);
static void parseDirectoryCommand(void);
static void parseFormatCommand(void);
static void parseNewCommand(void);
static void parseEditCommand(void);
static void parseCATCommand(void);

//function protoypes for private functions
static long stringToInteger (char *string);
static void appendtofile (void);

//Commands table, edit both the array and element numbers to update the table
#define NUM_COMMANDS 10
static const commandTable Table[NUM_COMMANDS]= {
	{"lcd", &parseLCDCommand},
	{"help", &helpList},
	
	// File system commands
	{"rm", &parseRemoveCommand},
	{"pwd", &parseDirectoryCommand},
	{"format", &parseFormatCommand},
	{"new", &parseNewCommand},
	{"edit", &parseEditCommand},
	{"cat", &parseCATCommand},
};

void Interpreter(void) {
	char string[50];
	UART_Init();
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

const char *fileerrormsg = "Reading/Writing to disk failed\r";

static void parseRemoveCommand(void){
	char *buffer;

	const char *msg = "Enter the file name to be deleted as the first argument\r";

	OS_bWait(&Sema4FileSystem);
	
	if (buffer = strtok(NULL, " ")) {
		if (eFile_Delete(buffer) == 2) {
			puts("File not found\r");
		} else if (eFile_Delete(buffer) == 1) {
			puts(fileerrormsg);
		}
	} else{
		puts(msg);
		OS_bSignal(&Sema4FileSystem);
		return;
	}
	
	OS_bSignal(&Sema4FileSystem);
}
static void parseDirectoryCommand(void){
	char buffer[100];
	
	OS_bWait(&Sema4FileSystem);
	
	if (eFile_Directory(buffer)){
		puts(fileerrormsg);
		OS_bSignal(&Sema4FileSystem);
		return;
	}
	
	OS_bSignal(&Sema4FileSystem);
	
	puts(buffer);
}

static void parseFormatCommand(void){
	OS_bWait(&Sema4FileSystem);
	
	if (eFile_Format()) {
		puts(fileerrormsg);
	}
	
	OS_bSignal(&Sema4FileSystem);
}

static void parseNewCommand(void){
	char *buffer;

	const char *msg = "Enter the file name to be created as the first argument\r";

	OS_bWait(&Sema4FileSystem);
	
	if (buffer = strtok(NULL, " ")) {
		switch (eFile_Create(buffer) ) {
			case 1:
				puts(fileerrormsg);
			OS_bSignal(&Sema4FileSystem);
			return;
			case 2: 
				puts("The disk is full\r");
			OS_bSignal(&Sema4FileSystem);
			return;
			case 3:
				puts("File already exists\r");
			OS_bSignal(&Sema4FileSystem);
			return;
		}
	} else{
		puts(msg);
		OS_bSignal(&Sema4FileSystem);
		return;
	}
	
	
	appendtofile();
	OS_bSignal(&Sema4FileSystem);
}

static void parseCATCommand(void){
	char *buffer;
	int error;
	
	const char *msg = "Enter the file name to be printed as the first argument\r";
	
	OS_bWait(&Sema4FileSystem);
	
	if (buffer = strtok(NULL, " ")) {
		switch ( eFile_ROpen(buffer) ) {
			case 1:
				puts(fileerrormsg);
				OS_bSignal(&Sema4FileSystem);
			return;
			case 3: 
				puts("File does not exist\r");
				OS_bSignal(&Sema4FileSystem);
			return;
		}
	} else{
		puts(msg);
		OS_bSignal(&Sema4FileSystem);
		return;
	}
	
	do {
		char nextchar;
		error = eFile_ReadNext(&nextchar);
		switch (error) {
			case 0:
				putchar(nextchar);
			break;
			case 1:
				puts(fileerrormsg);
				OS_bSignal(&Sema4FileSystem);
				return;
			case 3:
				printf("\r\n");
			break;
		}
	} while (error == 0);
	
	if (eFile_RClose()){
		puts(fileerrormsg);
	}
	

	OS_bSignal(&Sema4FileSystem);
}

static void parseEditCommand(void){
	char *buffer;
	int error;
	
	const char *msg = "Enter the file name to be edited as the first argument\r";
	
	OS_bWait(&Sema4FileSystem);
	
	if (buffer = strtok(NULL, " ")) {
		error = eFile_WOpen(buffer);
		if (error == 3) {
			puts("File already exists\r");
			
			OS_bSignal(&Sema4FileSystem);
			return;
		} else if (error == 2) {
			puts("The disk is full\r");
			
			OS_bSignal(&Sema4FileSystem);
			return;
		} else if (error == 1) {
			puts(fileerrormsg);
			
			OS_bSignal(&Sema4FileSystem);
			return;
		}
	} else{
		puts(msg);
		
		OS_bSignal(&Sema4FileSystem);
		return;
	}
	
	appendtofile();
	OS_bSignal(&Sema4FileSystem);
}

//Description: command for parsing Profiling requests
// Can invokde OS_Profile_Start and, OS_Profile_Clear, OS_Profile_Dump
//Input: None
//Output: none
static void parseProfilingCommand(void) {	
	puts("Begin Profiling for 100 samples...\r\n");
	OS_Profile_Start();
}

//Description: command for getting charachters from interpreter and writing them to file
//Ends if charachter '^' is typed
//Input: None
//Output: none
static void appendtofile (void) {
	char nextchar;
	puts("Type to append. Enter '^' to finish:\r");
	while ( (nextchar = getchar()) != '^' ) {
		putchar (nextchar);
		switch (eFile_Write(nextchar)){
			case 1:
				puts(fileerrormsg);
			  return;
			case 2: case 3:
				puts("Interpreter failed\r");
				return;
			case 4:
				puts("\r\n\nDisk is full. Closing the file.");
				break;
		}
		if (nextchar == '\r') {
			putchar ('\n');
			switch (eFile_Write('\n')){
				case 1:
					puts(fileerrormsg);
					return;
				case 2: case 3:
					puts("Interpreter failed\r");
					return;
				case 4:
					puts("\r\n\nDisk is full. Closing the file.");
					break;
			}	
		}
	}
	
	printf("\r\n");
	if (eFile_WClose()){
		puts(fileerrormsg);
	}
}
