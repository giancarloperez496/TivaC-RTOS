/******************************************************************************
 * File:        rtos.c
 *
 * Author:      Giancarlo Perez
 *
 * Created:     12/7/24
 *
 * Description: -
 ******************************************************************************/

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

//=============================================================================
// INCLUDES
//=============================================================================

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

//=============================================================================
// MAIN FUNCTION
//=============================================================================

int main(void) {
    bool ok;
    // Initialize hardware
    initSystemClockTo40Mhz();
    initHw();
    initMpu();
    initUart0();
    initRtos();

    // Setup UART0 baud rate
    setUart0BaudRate(115200, 40e6);

    // Initialize mutexes and semaphores
    initMutex(resource);
    initSemaphore(keyPressed, 1);
    initSemaphore(keyReleased, 0);
    initSemaphore(flashReq, 5);

    ok = createThread(idle, "Idle", 15, 512);

    // Add other processes
    ok &= createThread(lengthyFn, "LengthyFn", 12, 1024);
    ok &= createThread(flash4Hz, "Flash4Hz", 8, 512);
    ok &= createThread(oneshot, "OneShot", 4, 1536);
    ok &= createThread(readKeys, "ReadKeys", 12, 1024);
    ok &= createThread(debounce, "Debounce", 12, 1024);
    ok &= createThread(important, "Important", 0, 1024);
    ok &= createThread(uncooperative, "Uncoop", 12, 1024);
    ok &= createThread(errant, "Errant", 12, 512);
    ok &= createThread(shell, "Shell", 12, 4096);

    // Start up RTOS
    if (ok)
        startRtos(); // never returns
    else
        while(true);
}
