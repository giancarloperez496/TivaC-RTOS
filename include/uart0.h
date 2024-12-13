// UART0 Library
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    -

// Hardware configuration:
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#ifndef UART0_H_
#define UART0_H_

#include <stdint.h>
#include <stdbool.h>
#define MAX_CHARS 80
#define MAX_FIELDS 5
typedef struct _USER_DATA {
    char buffer[MAX_CHARS+1];
    uint8_t fieldCount;
    uint8_t fieldPosition[MAX_FIELDS];
    char fieldType[MAX_FIELDS];
} USER_DATA;
//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initUart0();
void setUart0BaudRate(uint32_t baudRate, uint32_t fcyc);
void putcUart0(char c);
void putsUart0(char* str);
void yield();
void fput1sUart0(const char* format, char* str);
void fput1dUart0(const char* format, uint32_t d);
void fput1hUart0(const char* format, uint32_t h);
void padded_putdUart0(uint32_t d, uint8_t align);
void padded_putsUart0(const char* str, uint8_t align);
void padded_puthUart0(uint32_t h, uint8_t align);
char getcUart0();
void tostring(uint32_t n, char* out, uint32_t b);
uint32_t tonumber(const char* str, uint32_t base);
void getsUart0(USER_DATA* data);
int str_length(const char str[]);
bool str_equal(const char* str1, const char* str2);
void str_copy(char dest[], const char* src);
void parseFields(USER_DATA* data);
char* getFieldString(USER_DATA* data, uint8_t fieldNumber);
int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber);
uint32_t getFieldHexInteger(USER_DATA* data, uint8_t fieldNumber);
bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments);
bool kbhitUart0();

#endif
