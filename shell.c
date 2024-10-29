#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "shell.h"



void rebootDevice() {
    NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
}

void ps() {
    putsUart0("PS called\n");
}

void ipcs() {
    putsUart0("IPCS called\n");
}

void kill(uint32_t pid) {
    char out[OUT_MAX];
    tostring(pid, out, 10);
    fput1sUart0("%s killed\n", out);
}

void pkill(char* name) {
    fput1sUart0("%s killed\n", name);
}

void pi(bool on) {
    fput1sUart0("pi %s\n", on ? "on" : "off");
}

void preempt(bool on) {
    fput1sUart0("preempt %s\n", on ? "on" : "off");
}

void sched(bool prio_on) {
    fput1sUart0("sched %s\n", prio_on ? "prio" : "rr");
}

void pidof(const char name[]) {
    fput1sUart0("%s launched\n", name);
}

void shell() {
    USER_DATA data;
    int n_processes = 3;
    int processes[3] = {
        "Idle",
        "flash4hz",
        "etc"
    };
    while (1) {
        if (kbhitUart0()) {
            getsUart0(&data);
            // Echo back to the user of the TTY interface for testing
            bool valid = false;
            #ifdef DEBUG
            putsUart0(data.buffer);
            putcUart0('\n');
            #endif
            // Parse fields
            parseFields(&data);
            // Echo back the parsed field data (type and fields)
            #ifdef DEBUG
            uint8_t i;
            for (i = 0; i < data.fieldCount; i++) {
                putcUart0(data.fieldType[i]);
                putcUart0('\t');
                putsUart0(&data.buffer[data.fieldPosition[i]]);
                putcUart0('\n');
            }
            #endif


            // Command evaluation
            if (isCommand(&data, "reboot", 0)) { //reboot
                valid = true;
                rebootDevice();
            }
            if (isCommand(&data, "ps", 0)) { //ps
                valid = true;
                ps();
            }
            if (isCommand(&data, "ipcs", 0)) { //ipcs
                valid = true;
                ipcs();
            }
            if (isCommand(&data, "kill", 1)) { //kill pid
                valid = true;
                int pid = getFieldInteger(&data, 1);
                kill(pid);
            }
            if (isCommand(&data, "pkill", 1)) { //pkill proc_name
                valid = true;
                char* str = getFieldString(&data, 1);
                pkill(str);
            }
            if (isCommand(&data, "pi", 1)) { //pi ON|OFF
                valid = true;
                char* stat = getFieldString(&data, 1);
                if (str_equal(stat, "ON")) {
                    pi(true);
                }
                else if (str_equal(stat, "OFF")) {
                    pi(false);
                }
            }
            if (isCommand(&data, "preempt", 1)) { //preempt ON|OFF
                valid = true;
                char* stat = getFieldString(&data, 1);
                if (str_equal(stat, "ON")) {
                    preempt(true);
                }
                else if (str_equal(stat, "OFF")) {
                    preempt(false);
                }
            }
            if (isCommand(&data, "sched", 1)) { //sched PRIO|RR
                valid = true;
                char* stat = getFieldString(&data, 1);
                if (str_equal(stat, "PRIO")) {
                    sched(true);
                }
                else if (str_equal(stat, "RR")) {
                    sched(false);
                }
            }
            if (isCommand(&data, "pidof", 1)) { //pidof proc_name
                valid = true;
                char* name = getFieldString(&data, 1);
                pidof(name);
            }
            if (!valid) {
                //start process of name &data
                int i;
                int found = 0;
                char* name = getFieldString(&data, 0);
                for (i = 0; i < n_processes; i++) {
                    if (str_equal(processes[i], name)) {
                        found = 1;
                        //setPinValue(RED_LED, 1);
                    }
                }
                if (!found) {
                    putsUart0("Invalid command\n");
                }
            }
        }
        else {
            yield();
        }
    }
}
