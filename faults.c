#include <faults.h>

extern void setAsp();
extern void setPsp(void* addr); //set the address of the SP, can only be done in privileged i think
extern uint32_t* getPsp();
extern uint32_t* getMsp();
extern void setCtrl(uint32_t mask);

void busFaultIsr(void) {
    int pid = 0;
    char out[OUT_MAX];
    tostring(pid, out, 10);
    fput1sUart0("Bus fault in process %s\n", out);
    while (1);
}

void usageFaultIsr(void) {
    int pid = 0;
    char out[OUT_MAX];
    tostring(pid, out, 10);
    fput1sUart0("Usage fault in process %s\n", out);
    while (1);
}

void hardFaultIsr(void) {
    uint32_t pid = 0;
    uint32_t* psp = getPsp();
    uint32_t* msp = getMsp();
    uint32_t faultStat = NVIC_FAULT_STAT_R;
    fput1dUart0("\nHard fault in process %d\n", pid);
    putsUart0("------------------------------------\n");
    fput1hUart0("PSP:\t0x%p\n", psp);
    fput1hUart0("MSP:\t0x%p\n", msp);
    fput1hUart0("MFAULT:\t0x%p\n", faultStat);
    uint32_t* PC = psp[6];
    fput1hUart0("Offending Instr: 0x%p\n", *PC);
    putsUart0("---------Process Stack Dump---------\n");
    fput1hUart0("xPSR:\t0x%p\n", psp[7]);
    fput1hUart0("PC:\t0x%p\n", PC);
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
    uint32_t pid = 0;
    uint32_t faultStat = NVIC_FAULT_STAT_R;
    uint32_t* psp = getPsp();
    uint32_t* msp = getMsp();
    fput1dUart0("\MPU fault in process %d\n", pid);
    putsUart0("------------------------------------\n");
    fput1hUart0("PSP:\t0x%p\n", psp);
    fput1hUart0("MSP:\t0x%p\n", msp);
    fput1hUart0("MFAULT:\t0x%p\n", faultStat);
    uint32_t* PC = psp[6];
    fput1hUart0("Offending Instr: 0x%p\n", *PC);
    fput1hUart0("Data Address: 0x%p\n", NVIC_MM_ADDR_R);
    putsUart0("---------Process Stack Dump---------\n");
    fput1hUart0("xPSR:\t0x%p\n", psp[7]);
    fput1hUart0("PC:\t0x%p\n", PC);
    fput1hUart0("LR:\t0x%p\n", psp[5]);
    fput1hUart0("R0:\t0x%p\n", psp[0]);
    fput1hUart0("R1:\t0x%p\n", psp[1]);
    fput1hUart0("R2:\t0x%p\n", psp[2]);
    fput1hUart0("R3:\t0x%p\n", psp[3]);
    fput1hUart0("R12:\t0x%p\n", psp[4]);
    putsUart0("------------------------------------\n\n");
    NVIC_SYS_HND_CTRL_R &= ~NVIC_SYS_HND_CTRL_MEMP; //clear fault pending register
    NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV; //trigger pendSV ISR call
    //while (1);
}

