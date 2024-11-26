#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "shell.h"
#include "kernel.h"
#include "mm.h"

void ps() {
    //putsUart0("PS called\n");
    psInfo taskInfo[MAX_TASKS] = {0};
    __asm(" SVC #0x0A");
    /*
     * flash4Hz - 0.00%
     * lengthyfn - one of the biggest
     * shell - calling ps twice will be significantly higher than first
     *
     *
     */
    putsUart0("|-i-|--PID--|---Name---|--CPU Time%--|--Prio--|------State------|--Semaphore/Mutex--|\n");
    uint8_t i;
    uint32_t percent;
    for (i = 0; i < MAX_TASKS; i++) {
        if (taskInfo[i].state != STATE_INVALID) {
            percent = ((10000*taskInfo[i].runtime)/(PS_REFRESH_TIME*1000));
            putsUart0("|");
            padded_putdUart0(i, 4);
            padded_puthUart0((uint32_t)(taskInfo[i].pid), 8);
            padded_putsUart0(taskInfo[i].name, 11);
            padded_putdUart0(percent/100, percent/100 < 10 ? 1 : 2);
            putsUart0(".");
            fput1dUart0("%d", (percent/10)%10);
            padded_putsUart0("%", percent/100 < 10 ? 12 : 11);
            padded_putdUart0(taskInfo[i].prio, 9);
            switch (taskInfo[i].state) {
            case STATE_STOPPED:
                padded_putsUart0("STOPPED", 18);
                padded_putsUart0(" ", 18);
                break;
            case STATE_READY:
                padded_putsUart0("READY", 18);
                padded_putsUart0(" ", 18);
                break;
            case STATE_DELAYED:
                padded_putsUart0("DELAYED", 18);
                padded_putsUart0(" ", 18);
                break;
            case STATE_BLOCKED_MUTEX:
                padded_putsUart0("BLOCKED_MUTEX", 18);
                padded_putdUart0(taskInfo[i].mutex_or_sem, 18);
                break;
            case STATE_BLOCKED_SEMAPHORE:
                padded_putsUart0("BLOCKED_SEMAPHORE", 18);
                padded_putdUart0(taskInfo[i].mutex_or_sem, 18);
                break;
            }
            putsUart0("|\n");
        }
    }
    putsUart0("|-----------------------------------------------------------------------------------|\n\n");
}

void ipcs() {
    ipcsInfo info[1] = {0};
    __asm(" SVC #0x0B");
    uint32_t i, j;
    putsUart0("|---Mutex---|--Locked--|-LockedBy-|-Queue Size-|-----Queue-----|\n");
    for (i = 0; i < MAX_MUTEXES; i++) {
        putsUart0("|");
        padded_putdUart0(i, 12);
        if (info->mutexes[i].lock) {
            padded_putsUart0("yes", 11);
            padded_putsUart0(info->nameArr[info->mutexes[i].lockedBy], 11);
            padded_putdUart0(info->mutexes[i].queueSize, 13);
            if (info->mutexes[i].queueSize > 0) {
                for (j = 0; j < info->mutexes[i].queueSize; j++) {
                    //fput1dUart0("\t%d - ", j);
                    fput1sUart0("%s", info->nameArr[info->mutexes[i].processQueue[j]]); //print names
                    if (j != info->mutexes[i].queueSize-1) {
                        putsUart0("->");
                    }
                }
            }
        }
        else {
            padded_putsUart0(" no", 13);
            padded_putsUart0(" -", 16);
        }
        putsUart0("\n");
    }
    putsUart0("|--------------------------------------------------------------|\n\n");

    putsUart0("|---Semaphore---|--Count--|----Queue Size----|------Queue------|\n");
    for (i = 0; i < MAX_SEMAPHORES; i++) {
        putsUart0("|");
        padded_putdUart0(i, 16);
        padded_putdUart0(info->semaphores[i].count, 11);
        padded_putdUart0(info->semaphores[i].queueSize, 18);
        if (info->semaphores[i].queueSize > 0) {
            for (j = 0; j < info->semaphores[i].queueSize; j++) {
                //fput1dUart0("\t%d - ", j);
                fput1sUart0("%s", info->nameArr[info->semaphores[i].processQueue[j]]); //print names
                if (j != info->semaphores[i].queueSize-1) {
                    putsUart0("->");
                }
            }
        }
        putsUart0("\n");
    }
    putsUart0("|--------------------------------------------------------------|\n\n");
}

void kill(uint32_t pid) {
    uint32_t stat = stopThread((_fn)pid);
    if (stat) {
        fput1hUart0("Killed task at 0x%p\n", pid);
    }
    else {
        fput1hUart0("Could not kill task at 0x%p\n", pid);
    }
}

void pkill(const char name[]) {
    uint32_t pid = pidof(name);
    if (pid > 0) {
        uint32_t stat = stopThread((_fn)pid);
        if (stat) {
            fput1sUart0("Killed task %s\n", name);
        }
        else {
            fput1sUart0("Could not kill task %s\n", name);
        }
    }
    else {
        putsUart0("Invalid process\n");
    }
}

void sched(uint8_t prio_on) {
    __asm(" SVC #0x07");
    fput1sUart0("sched %s\n", prio_on ? "prio" : "rr");
}

void pi(uint8_t on) {
    __asm(" SVC #0x08");
    fput1sUart0("pi %s\n", on ? "on" : "off");
}

void preempt(uint8_t on) {
    __asm(" SVC #0x09");
    fput1sUart0("preempt %s\n", on ? "on" : "off");
}

uint32_t pidof(const char name[]) {
    __asm(" SVC #0x0C");
    uint32_t pid = getR0();
    //fput1hUart0("%p\n", pid);
    return pid;
}

void meminfo() {
    memInfo info[MAX_ALLOCS] = {};
    __asm(" SVC #0x0D");
    uint32_t i;
    putsUart0("|---Alloc---|---Thread---|---Address---|---Size---|---Usage---|\n");
    for (i = 0; i < MAX_ALLOCS; i++) { //i dont have access to n_allocs idiot
        if (info[i].valid) {
            putsUart0("|");
            padded_putdUart0(i, 12);
            padded_putsUart0(info[i].ownerName, 13);
            putsUart0("0x");
            padded_puthUart0((uint32_t)info[i].baseAdd, 12);
            padded_putdUart0(info[i].size, 11);
            fput1dUart0("%d.", info[i].usage / 10);
            fput1dUart0("%d%", info[i].usage % 10);
            padded_putsUart0(" ", info[i].usage/10 < 10 ? 7 : 6);
            putsUart0("|\n");
        }
        //whole = value / 10
        //decimal = value % 10
    }
    putsUart0("|-------------------------------------------------------------|\n\n");
}

void reboot() {
    __asm(" SVC #0xFF");
}

void shell() {
    USER_DATA data;
    uint8_t pre = 1;
    uint8_t prio = 1;
    putsUart0(">");
    while (1) {
        if (kbhitUart0()) {
            getsUart0(&data);
            bool valid = false;
            parseFields(&data);
            // Command evaluation
            if (isCommand(&data, "reboot", 0)) { //reboot
                valid = true;
                reboot();
            }
            if (isCommand(&data, "ps", 0)) { //ps
                valid = true;
                ps();
            }
            if (isCommand(&data, "ipcs", 0)) { //ipcs
                valid = true;
                ipcs();
            }
            if (isCommand(&data, "meminfo", 0)) { //meminfo
                valid = true;
                meminfo();
            }
            if (isCommand(&data, "kill", 1)) { //kill pid
                valid = true;
                uint32_t pid = getFieldHexInteger(&data, 1);
                kill(pid);
            }
            if (isCommand(&data, "pkill", 1)) { //pkill proc_name
                valid = true;
                char* str = getFieldString(&data, 1);
                pkill(str);
            }
            /*if (isCommand(&data, "pi", 1)) { //pi ON|OFF
                valid = true;
                char* stat = getFieldString(&data, 1);
                if (str_equal(stat, "ON")) {
                    pi(true);
                }
                else if (str_equal(stat, "OFF")) {
                    pi(false);
                }
            }*/
            if (isCommand(&data, "preempt", 0)) {
                    valid = true;
                    fput1sUart0("Preemption: %s\n", pre ? "On" : "Off");
                }
            if (isCommand(&data, "preempt", 1)) { //preempt ON|OFF
                valid = true;
                char* stat = getFieldString(&data, 1);
                if (str_equal(stat, "ON")) {
                    preempt(true);
                    pre = 1;
                }
                else if (str_equal(stat, "OFF")) {
                    preempt(false);
                    pre = 0;
                }
            }
            if (isCommand(&data, "sched", 0)) {
                valid = true;
                fput1sUart0("Scheduler Mode: %s\n", prio ? "priority" : "round-robin");
            }
            if (isCommand(&data, "sched", 1)) { //sched PRIO|RR
                valid = true;
                char* stat = getFieldString(&data, 1);
                if (str_equal(stat, "PRIO")) {
                    sched(1);
                    prio = 1;
                }
                else if (str_equal(stat, "RR")) {
                    sched(0);
                    prio = 0;
                }
            }
            if (isCommand(&data, "pidof", 1)) { //pidof proc_name
                valid = true;
                char* name = getFieldString(&data, 1);
                uint32_t pid = pidof(name);
                if (pid) {
                    fput1hUart0("0x%p\n", pid);
                }
                else {
                    putsUart0("Invalid thread\n");
                }
            }
            if (!valid) {
                //start process of name &data
                //uint32_t i;
                //uint8_t found = 0;
                char* name = getFieldString(&data, 0);
                uint32_t pid = pidof(name);
                if (pid) {
                    uint32_t stat = restartThread((_fn)pid);
                    if (stat) {
                        fput1sUart0("%s launched\n", name);
                    }
                    else {
                        fput1sUart0("Error launching %s\n", name);
                    }
                }
                else {
                    putsUart0("Invalid command\n");
                }
            }
            putsUart0(">");
        }
        else {
            yield();
        }
    }
}
