// Tasks
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "gpio.h"
#include "wait.h"
#include "kernel.h"
#include "uart0.h"
#include "clock.h"
#include "mm.h"
#include "tasks.h"
#include "nvic.h"


//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
// REQUIRED: Add initialization for blue, orange, red, green, and yellow LEDs
//           Add initialization for 6 pushbuttons
void initHw(void)
{
    // Setup LEDs and pushbuttons
    initSystemClockTo40Mhz();
    initUart0();
    setUart0BaudRate(115200, 40e6);
    enablePort(PORTA);
    enablePort(PORTC);
    enablePort(PORTD);
    enablePort(PORTE);
    enablePort(PORTF);
    SYSCTL_RCGCWTIMER_R |= SYSCTL_RCGCWTIMER_R0;
    selectPinPushPullOutput(BLUE_LED);
    selectPinPushPullOutput(GREEN_LED);
    selectPinPushPullOutput(YELLOW_LED);
    selectPinPushPullOutput(ORANGE_LED);
    selectPinPushPullOutput(RED_LED);
    selectPinPushPullOutput(WHITE_LED);

    setPinValue(BLUE_LED, 0);
    setPinValue(GREEN_LED, 0);
    setPinValue(YELLOW_LED, 0);
    setPinValue(ORANGE_LED, 0);
    setPinValue(RED_LED, 0);
    setPinValue(WHITE_LED, 0);

    setPinCommitControl(PB5);
    enablePinPulldown(PB0);
    enablePinPulldown(PB1);
    enablePinPulldown(PB2);
    enablePinPulldown(PB3);
    enablePinPulldown(PB4);
    enablePinPulldown(PB5);
    /*enablePinPullup(PB0);
    enablePinPullup(PB1);
    enablePinPullup(PB2);
    enablePinPullup(PB3);
    enablePinPullup(PB4);
    enablePinPullup(PB5);*/
    selectPinDigitalInput(PB0);
    selectPinDigitalInput(PB1);
    selectPinDigitalInput(PB2);
    selectPinDigitalInput(PB3);
    selectPinDigitalInput(PB4);
    selectPinDigitalInput(PB5);

    NVIC_SYS_HND_CTRL_R |= NVIC_SYS_HND_CTRL_USAGE | NVIC_SYS_HND_CTRL_BUS | NVIC_SYS_HND_CTRL_MEM; //enable usage fault, bus fault, memory management (MPU) fault
    NVIC_CFG_CTRL_R |= NVIC_CFG_CTRL_DIV0;

    // Power-up flash
    //setPinValue(GREEN_LED, 1);
    //waitMicrosecond(250000);
    //setPinValue(GREEN_LED, 0);
    //waitMicrosecond(250000);

    // Configure Timer 0 for 1 sec tick
    WTIMER0_CTL_R &= ~TIMER_CTL_TAEN;                 // turn-off timer before reconfiguring
    WTIMER0_CFG_R = TIMER_CFG_32_BIT_TIMER;           // configure as 32-bit timer (A+B)
    WTIMER0_TAMR_R = TIMER_TAMR_TACDIR | TIMER_TAMR_TAMR_PERIOD;          // configure for one shot (count up)
    WTIMER0_TAILR_R = 40e6*(1);
    WTIMER0_IMR_R |= TIMER_IMR_TATOIM;                // turn-on interrupt
    WTIMER0_TAV_R = 0;
    WTIMER0_CTL_R |= TIMER_CTL_TAEN;
    enableNvicInterrupt(INT_WTIMER0A);

    putsUart0("Starting...\n");
}

void wtimer0Isr() {
    setPinValue(WHITE_LED, !getPinValue(WHITE_LED));
    WTIMER0_ICR_R |= TIMER_ICR_TATOCINT;
}

// REQUIRED: add code to return a value from 0-63 indicating which of 6 PBs are pressed
uint8_t readPbs(void){
    uint8_t a = getPinValue(PB5) << 5 | getPinValue(PB4) << 4 | getPinValue(PB3) << 3 | getPinValue(PB2) << 2 | getPinValue(PB1) << 1 | getPinValue(PB0);
    return a;
}

// one task must be ready at all times or the scheduler will fail
// the idle task is implemented for this purpose
void idle(void)
{
    while(true)
    {
        setPinValue(ORANGE_LED, 1);
        waitMicrosecond(1000);
        setPinValue(ORANGE_LED, 0);
        yield();
    }
}

/*void idle2(void)
{
    while(true)
    {
        setPinValue(ORANGE_LED, 1);
        waitMicrosecond(1000);
        setPinValue(ORANGE_LED, 0);
        yield();
    }
}*/

void flash4Hz(void)
{
    while(true)
    {
        setPinValue(GREEN_LED, !getPinValue(GREEN_LED));
        sleep(125);
    }
}

void oneshot(void)
{
    while(true)
    {
        wait(flashReq);
        setPinValue(YELLOW_LED, 1);
        sleep(1000);
        setPinValue(YELLOW_LED, 0);
    }
}

void partOfLengthyFn(void)
{
    // represent some lengthy operation
    waitMicrosecond(990);
    // give another process a chance to run
    yield();
}

void lengthyFn(void)
{
    uint16_t i;
    uint8_t* mem;
    mem = malloc_from_heap(5000 * sizeof(uint8_t));
    while(true)
    {
        lock(resource);
        for (i = 0; i < 5000; i++)
        {
            partOfLengthyFn();
            mem[i] = i % 256;
        }
        setPinValue(RED_LED, !getPinValue(RED_LED));
        unlock(resource);
    }
}

void readKeys(void)
{
    uint8_t buttons;
    while(true)
    {
        wait(keyReleased);
        buttons = 0;
        while (buttons == 0)
        {
            buttons = readPbs();
            yield();
        }
        post(keyPressed);
        if ((buttons & 1) != 0)
        {
            setPinValue(YELLOW_LED, !getPinValue(YELLOW_LED));
            setPinValue(RED_LED, 1);
        }
        if ((buttons & 2) != 0)
        {
            post(flashReq);
            setPinValue(RED_LED, 0);
        }
        if ((buttons & 4) != 0)
        {
            restartThread(flash4Hz);
        }
        if ((buttons & 8) != 0)
        {
            stopThread(flash4Hz);
        }
        if ((buttons & 16) != 0)
        {
            setThreadPriority(lengthyFn, 4);
        }
        yield();
    }
}

void debounce(void)
{
    uint8_t count;
    while(true)
    {
        wait(keyPressed);
        count = 10;
        while (count != 0)
        {
            sleep(10);
            if (readPbs() == 0)
                count--; //if button is not pressed
            else
                count = 10; //if button is pressed
        }
        post(keyReleased);
    }
}

void uncooperative(void)
{
    while(true)
    {
        while (readPbs() == 8)
        {
        }
        yield();
    }
}

void errant(void)
{
    uint32_t* p = (uint32_t*)0x20000000;
    while(true)
    {
        while (readPbs() == 32)
        {
            *p = 0;
        }
        yield();
    }
}

void important(void)
{
    while(true)
    {
        lock(resource);
        setPinValue(BLUE_LED, 1);
        sleep(1000);
        setPinValue(BLUE_LED, 0);
        unlock(resource);
    }
}
