#include "io.h"
#include "UART2.h"

int fputc(int ch, FILE *f){
	UART_OutChar(ch);
	return (1);
}

int fgetc (FILE *f){
	char c = UART_InChar();
	if (c == 0x0D) {
		c = '\n';
		UART_OutChar('\r');
	}
	UART_OutChar(c);
	return (c);
}

int ferror(FILE *f){
	/* Your implementation of ferror */
	return EOF;
}
