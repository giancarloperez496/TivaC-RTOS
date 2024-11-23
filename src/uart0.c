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

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "tm4c123gh6pm.h"
#include "uart0.h"

// PortA masks
#define UART_TX_MASK 2
#define UART_RX_MASK 1

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize UART0
void initUart0()
{
    // Enable clocks
    SYSCTL_RCGCUART_R |= SYSCTL_RCGCUART_R0;
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R0;
    _delay_cycles(3);

    // Configure UART0 pins
    GPIO_PORTA_DR2R_R |= UART_TX_MASK;                  // set drive strength to 2mA (not needed since default configuration -- for clarity)
    GPIO_PORTA_DEN_R |= UART_TX_MASK | UART_RX_MASK;    // enable digital on UART0 pins
    GPIO_PORTA_AFSEL_R |= UART_TX_MASK | UART_RX_MASK;  // use peripheral to drive PA0, PA1
    GPIO_PORTA_PCTL_R &= ~(GPIO_PCTL_PA1_M | GPIO_PCTL_PA0_M); // clear bits 0-7
    GPIO_PORTA_PCTL_R |= GPIO_PCTL_PA1_U0TX | GPIO_PCTL_PA0_U0RX;
                                                        // select UART0 to drive pins PA0 and PA1: default, added for clarity

    // Configure UART0 to 115200 baud (assuming fcyc = 40 MHz), 8N1 format
    UART0_CTL_R = 0;                                    // turn-off UART0 to allow safe programming
    UART0_CC_R = UART_CC_CS_SYSCLK;                     // use system clock (40 MHz)
    UART0_IBRD_R = 21;                                  // r = 40 MHz / (Nx115.2kHz), set floor(r)=21, where N=16
    UART0_FBRD_R = 45;                                  // round(fract(r)*64)=45
    UART0_LCRH_R = UART_LCRH_WLEN_8 | UART_LCRH_FEN;    // configure for 8N1 w/ 16-level FIFO
    UART0_CTL_R = UART_CTL_TXE | UART_CTL_RXE | UART_CTL_UARTEN;
                                                        // enable TX, RX, and module
}

// Set baud rate as function of instruction cycle frequency
void setUart0BaudRate(uint32_t baudRate, uint32_t fcyc)
{
    uint32_t divisorTimes128 = (fcyc * 8) / baudRate;   // calculate divisor (r) in units of 1/128,
                                                        // where r = fcyc / 16 * baudRate
    divisorTimes128 += 1;                               // add 1/128 to allow rounding
    UART0_CTL_R = 0;                                    // turn-off UART0 to allow safe programming
    UART0_IBRD_R = divisorTimes128 >> 7;                // set integer value to floor(r)
    UART0_FBRD_R = ((divisorTimes128) >> 1) & 63;       // set fractional value to round(fract(r)*64)
    UART0_LCRH_R = UART_LCRH_WLEN_8 | UART_LCRH_FEN;    // configure for 8N1 w/ 16-level FIFO
    UART0_CTL_R = UART_CTL_TXE | UART_CTL_RXE | UART_CTL_UARTEN;
                                                        // turn-on UART0
}

// Blocking function that writes a serial character when the UART buffer is not full
void putcUart0(char c)
{
    while (UART0_FR_R & UART_FR_TXFF);               // wait if uart0 tx fifo full
    UART0_DR_R = c;                                  // write character to fifo
}

// Blocking function that writes a string when the UART buffer is not full
void putsUart0(char* str)
{
    uint8_t i = 0;
    while (str[i] != '\0')
        putcUart0(str[i++]);
}

void tostring(uint32_t n, char* out, uint32_t b) {
    char num[10];
    uint32_t i = 0;
    uint64_t j = 1;
    uint32_t c;
    if (n == 0) {
        out[0] = '0';
        out[1] = 0;
        return;
    }
    for (c = n; c > 0; c /= b) {
        uint64_t r = ((n % (j*b))/j);
        char digit = (r < 10) ? (48 + r) : (65 + r - 10);
        num[i] = digit;
        j *= b;
        i++;
    }
    for (c = 0; c < i; c++) {
        out[c] = num[i-c-1];
    }
    out[c] = 0;
}

void fput1sUart0(const char* format, char* str) {
    while (*format != '\0') {
        if (*format == '%' && *(format + 1) == 's') {
            format += 2;
            putsUart0(str);
        } else {
            char buf[2] = {*format, '\0'};
            putsUart0(buf);
            format++;
        }
    }
}

void padded_putsUart0(const char* str, uint8_t align) {
    uint32_t str_len = str_length(str); // Get the length of the string
    putsUart0(str);
    if (align > str_len) {
        uint32_t i;
        for (i = 0; i < align - str_len; i++) {
            putsUart0(" "); // Print spaces to pad to the desired alignment
        }
    }
}

void fput1dUart0(const char* format, uint32_t d) {
    char str[12];
    tostring(d, str, 10);
    while (*format != '\0') {
        if (*format == '%' && *(format + 1) == 'd') {
            format += 2;
            putsUart0(str);
        } else {
            char buf[2] = {*format, '\0'};
            putsUart0(buf);
            format++;
        }
    }
}

void padded_putdUart0(uint32_t d, uint8_t align) {
    char str[12];
    tostring(d, str, 10);
    uint32_t str_len = str_length(str);
    putsUart0(str);
    if (align > str_len) {
        uint32_t i;
        for (i = 0; i < align - str_len; i++) {
            putsUart0(" ");
        }
    }
}

void fput1hUart0(const char* format, uint32_t h) {
    char str[10] = {0};
    tostring(h, str, 16);
    while (*format != '\0') {
        if (*format == '%' && *(format + 1) == 'p') {
            format += 2;
            putsUart0(str);
        } else {

            char buf[2] = {*format, '\0'};
            putsUart0(buf);
            format++;
        }
    }
}

void padded_puthUart0(uint32_t h, uint8_t align) {
    char str[12];
    tostring(h, str, 16);
    uint32_t str_len = str_length(str);
    putsUart0(str);
    if (align > str_len) {
        uint32_t i;
        for (i = 0; i < align - str_len; i++) {
            putsUart0(" ");
        }
    }
}

// Blocking function that returns with serial data once the buffer is not empty
char getcUart0()
{
    while (UART0_FR_R & UART_FR_RXFE); // wait if uart0 rx fifo empty
    return UART0_DR_R & 0xFF;                        // get character from fifo
}

void getsUart0(USER_DATA* data) {
    int count = 0;
    while (true) {
        if (kbhitUart0()) {
            char c = getcUart0();
            if ((c == 8 || c == 127) & (count > 0)) {
                count--;
            }
            else if (c == 13) {
                data->buffer[count] = '\0';
                return;
            }
            else if (c >= 32) {
                data->buffer[count] = c;
                count++;
            }
            if (count == MAX_CHARS) {
                data->buffer[count] = '\0';
                return;
            }
        }
        else {
            yield();
        }
    }
}

int str_length(const char str[]) {
    int c;
    for (c = 0; str[c] != 0; c++);
    return c;
}

bool str_equal(const char* str1, const char* str2) {
    int i;
    if (str_length(str1) != str_length(str2)) {
        return false;
    }
    for (i = 0; str1[i] != 0; i++) {
        if (str1[i] != str2[i]) {
            return false;
        }
    }
    return true;
}

//assumes dest has enough allocated memory to store src
void str_copy(char dest[], const char* src) {
    uint32_t i;
    for (i = 0; src[i] != 0; i++) {
        dest[i] = src[i];
    }
    dest[i+1] = 0;
}

void parseFields(USER_DATA* data) {
    int hasCounted = 0;
    int i;
    data->fieldCount = 0;
    for (i = 0; data->buffer[i] != 0; i++) {
        char c = data->buffer[i];
        if ((c >= 48 && c <= 57) || c == 95) {
            //is number OR '_'
            if (!hasCounted) {
                hasCounted = 1;
                data->fieldType[data->fieldCount] = 'n';
                data->fieldPosition[data->fieldCount] = i;
                data->fieldCount++;
            }
        }
        else if ((c >= 65 && c <= 90) || (c >= 97 && c <= 122)) {
            //is alphabet
            if (!hasCounted) {
                hasCounted = 1;
                data->fieldType[data->fieldCount] = 'a';
                data->fieldPosition[data->fieldCount] = i;
                data->fieldCount++;
            }
        }
        else {
            // is delimiter
            hasCounted = 0;
            data->buffer[i] = 0;
        }
        if (data->fieldCount == MAX_FIELDS) {
            return;
        }
    }
    return;
}

char* getFieldString(USER_DATA* data, uint8_t fieldNumber) {
    if (fieldNumber < data->fieldCount) {
        return &data->buffer[data->fieldPosition[fieldNumber]];
    }
    else {
        return NULL;
    }
}

int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNumber) {
    if (fieldNumber < MAX_FIELDS) {
        if (data->fieldType[fieldNumber] == 'n') {
            return atoi((const char*)&(data->buffer[data->fieldPosition[fieldNumber]]));
        }
    }
    else {
        return NULL;
    }
    return NULL;
}

bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments) {
    int i;
    int arg1_len = 0;;
    for (; data->buffer[arg1_len] != ' ' && data->buffer[arg1_len] != 0; arg1_len++);
    if (str_length(strCommand) != arg1_len) {
        return false;
    }
    for (i = 0; data->buffer[i] != 0 && strCommand[i] != 0; i++) {
        if (data->buffer[i] != strCommand[i]) {
            return false;
        }
    }
    if (data->fieldCount-1 == minArguments) {
        return true;
    }
    return false;
}

// Returns the status of the receive buffer
bool kbhitUart0()
{
    return !(UART0_FR_R & UART_FR_RXFE);
}
