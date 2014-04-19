#include "io.h"
#include "UART2.h"

// functions for interfacing the standard library
int fputc(int ch, FILE *f){
	UART_OutChar(ch);
	return (1);
}

int fgetc (FILE *f){
	char c = UART_InChar();
	return (c);
}

int ferror(FILE *f){
	/* Your implementation of ferror */
	return EOF;
}

