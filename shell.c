#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "uart0.h"
#include "shell.h"


void rebootDevice() {
    __asm(" SVC #0xFF");
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

}

void shell() {
    USER_DATA data;
    while (1) {
        if (kbhitUart0()) {
            getsUart0(&data);
            bool valid = false;
            parseFields(&data);
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
                //int i;
                int found = 0;
                char* name = getFieldString(&data, 0);
                /*for (i = 0; i < n_processes; i++) {
                    if (str_equal(processes[i], name)) {
                        found = 1;
                        //setPinValue(RED_LED, 1);
                    }
                }*/
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
