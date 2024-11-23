#ifndef SHELL_H_
#define SHELL_H_

#include "gpio.h"
#include "tm4c123gh6pm.h"
//#define RED_LED PORTF,1 // PF1
#define OUT_MAX 50 //50 chars max

void ps();
void ipcs();
void kill(uint32_t pid);
void pkill(const char name[]);
void sched(uint8_t prio_on);
void pi(uint8_t on);
void preempt(uint8_t on);
uint32_t pidof(const char name[]);
void meminfo();
void reboot();
void shell();

#endif
