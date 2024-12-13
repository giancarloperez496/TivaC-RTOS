#ifndef FAULTS_H_
#define FAULTS_H_

#include "uart0.h"
#include "tm4c123gh6pm.h"

#define OUT_MAX 50 //50 chars max


void busFaultIsr(void);
void usageFaultIsr(void);
void hardFaultIsr(void);
void mpuFaultIsr(void);

#endif
