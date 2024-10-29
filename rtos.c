// RTOS Framework - Fall 2024
// J Losh

// Student Name: Giancarlo Perez
// TO DO: Add your name(s) on this line.
//        Do not include your ID number(s) in the file.

// Please do not change any function name in this code or the thread priorities

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// 6 Pushbuttons and 5 LEDs, UART
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port
//   Configured to 115,200 baud, 8N1
// Memory Protection Unit (MPU):
//   Region to control access to flash, peripherals, and bitbanded areas
//   4 or more regions to allow SRAM access (RW or none for task)

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include "tm4c123gh6pm.h"
#include "clock.h"
#include "gpio.h"
#include "uart0.h"
#include "wait.h"
#include "mm.h"
#include "kernel.h"
#include "faults.h"
#include "tasks.h"
#include "shell.h"

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    bool ok;

    // Initialize hardware
    initSystemClockTo40Mhz();
    initHw();
    initUart0();
    allowFlashAccess();
    allowPeripheralAccess();
    setupSramAccess();
    initRtos();

    // Setup UART0 baud rate
    setUart0BaudRate(115200, 40e6);

    // Initialize mutexes and semaphores
    initMutex(resource);
    initSemaphore(keyPressed, 1);
    initSemaphore(keyReleased, 0);
    initSemaphore(flashReq, 5);

    // Add required idle process at lowest priority
    ok = createThread(idle, "Idle", 15, 512);                                               //prio is 5 for testing
                                                                                            /*ok = createThread(T1, "T1", 2, 512);
                                                                                            ok = createThread(T2, "T2", 1, 512);
                                                                                            ok = createThread(T3, "T3", 4, 512);
                                                                                            ok = createThread(T4, "T4", 1, 512);*/

    // Add other processes
    //ok &= createThread(lengthyFn, "LengthyFn", 12, 1024);
    //ok &= createThread(flash4Hz, "Flash4Hz", 8, 512);
    //ok &= createThread(oneshot, "OneShot", 4, 1536);
    //ok &= createThread(readKeys, "ReadKeys", 12, 1024);
    //ok &= createThread(debounce, "Debounce", 12, 1024);
    //ok &= createThread(important, "Important", 0, 1024);
    //ok &= createThread(uncooperative, "Uncoop", 12, 1024);
    //ok &= createThread(errant, "Errant", 12, 512);
    //ok &= createThread(shell, "Shell", 12, 4096);

    // TODO: Add code to implement a periodic timer and ISR

    // Start up RTOS
    if (ok)
        startRtos(); // never returns
    else
        while(true);
}
