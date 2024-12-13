/******************************************************************************
 * File:        faults.c
 *
 * Author:      Giancarlo Perez
 *
 * Created:     12/13/24
 *
 * Description: -
 ******************************************************************************/

//=============================================================================
// INCLUDES
//=============================================================================

#include "faults.h"
#include "kernel.h"

//=============================================================================
// GLOBALS
//=============================================================================

extern void setAsp();
extern void setPsp(void* addr); //set the address of the SP, can only be done in privileged i think
extern uint32_t* getPsp();
extern uint32_t* getMsp();
extern void setCtrl(uint32_t mask);

//=============================================================================
// PUBLIC FUNCTIONS
//=============================================================================

void busFaultIsr(void) {
    uint32_t pid = getCurrentPid();
    fput1hUart0("\nBus fault in process 0x%p\n", pid);
    while (1);
}

void usageFaultIsr(void) {
    uint32_t pid = getCurrentPid();
    fput1hUart0("\nUsage fault in process 0x%p\n", pid);
    while (1);
}

void hardFaultIsr(void) {
    uint32_t pid = getCurrentPid();
    uint32_t* psp = getPsp();
    uint32_t* msp = getMsp();
    uint32_t faultStat = NVIC_FAULT_STAT_R;
    fput1hUart0("\nHard fault in process 0x%p\n", pid);
    putsUart0("------------------------------------\n");
    fput1hUart0("PSP:\t0x%p\n", (uint32_t)psp);
    fput1hUart0("MSP:\t0x%p\n", (uint32_t)msp);
    fput1hUart0("MFAULT:\t0x%p\n", faultStat);
    uint32_t* PC = (uint32_t*)psp[6];
    fput1hUart0("Offending Instr: 0x%p:", (uint32_t)PC);
    fput1hUart0("  0x%p\n", *PC);
    putsUart0("---------Process Stack Dump---------\n");
    fput1hUart0("xPSR:\t0x%p\n", psp[7]);
    fput1hUart0("PC:\t0x%p\n", (uint32_t)PC);
    fput1hUart0("LR:\t0x%p\n", psp[5]);
    fput1hUart0("R0:\t0x%p\n", psp[0]);
    fput1hUart0("R1:\t0x%p\n", psp[1]);
    fput1hUart0("R2:\t0x%p\n", psp[2]);
    fput1hUart0("R3:\t0x%p\n", psp[3]);
    fput1hUart0("R12:\t0x%p\n", psp[4]);
    putsUart0("------------------------------------\n\n");
    while (1);
}

void mpuFaultIsr(void) {
    uint32_t pid = getCurrentPid();
    uint32_t faultStat = NVIC_FAULT_STAT_R;
    uint32_t* psp = getPsp();
    uint32_t* msp = getMsp();
    fput1hUart0("\nMPU fault in process %p\n", pid);
    putsUart0("------------------------------------\n");
    fput1hUart0("PSP:\t0x%p\n", (uint32_t)psp);
    fput1hUart0("MSP:\t0x%p\n", (uint32_t)msp);
    fput1hUart0("MFAULT:\t0x%p\n", faultStat);
    uint32_t* PC = (uint32_t*)psp[6];
    fput1hUart0("Offending Instr: 0x%p\n", *PC);
    fput1hUart0("Data Address: 0x%p\n", NVIC_MM_ADDR_R);
    putsUart0("---------Process Stack Dump---------\n");
    fput1hUart0("xPSR:\t0x%p\n", psp[7]);
    fput1hUart0("PC:\t0x%p\n", (uint32_t)PC);
    fput1hUart0("LR:\t0x%p\n", psp[5]);
    fput1hUart0("R0:\t0x%p\n", psp[0]);
    fput1hUart0("R1:\t0x%p\n", psp[1]);
    fput1hUart0("R2:\t0x%p\n", psp[2]);
    fput1hUart0("R3:\t0x%p\n", psp[3]);
    fput1hUart0("R12:\t0x%p\n", psp[4]);
    putsUart0("------------------------------------\n\n");
    NVIC_SYS_HND_CTRL_R &= ~NVIC_SYS_HND_CTRL_MEMP; //clear fault pending register
    int32_t stat = kill_proc(pid);
    if (stat != -1) {
        fput1hUart0("Killed process %p\n\n>", pid);
    }
    NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV; //trigger pendSV ISR call
}

