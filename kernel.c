// Kernel functions
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
#include "tm4c123gh6pm.h"
#include "mm.h"
#include "kernel.h"
#include "uart0.h"

#ifndef NULL
 #define NULL 0
#endif
//-----------------------------------------------------------------------------
// RTOS Defines and Kernel Variables
//-----------------------------------------------------------------------------

// mutex
typedef struct _mutex {
    bool lock;
    uint8_t queueSize;
    uint8_t processQueue[MAX_MUTEX_QUEUE_SIZE];
    uint8_t lockedBy;
} mutex;
mutex mutexes[MAX_MUTEXES];

// semaphore
typedef struct _semaphore {
    uint8_t count;
    uint8_t queueSize;
    uint8_t processQueue[MAX_SEMAPHORE_QUEUE_SIZE];
} semaphore;
semaphore semaphores[MAX_SEMAPHORES];

// task states
#define STATE_INVALID           0 // no task
#define STATE_STOPPED           1 // stopped, all memory freed
#define STATE_READY             2 // has run, can resume at any time
#define STATE_DELAYED           3 // has run, but now awaiting timer
#define STATE_BLOCKED_MUTEX     4 // has run, but now blocked by semaphore
#define STATE_BLOCKED_SEMAPHORE 5 // has run, but now blocked by semaphore

// task
uint8_t taskCurrent = 0;          // index of last dispatched task
uint8_t taskCount = 0;            // total number of valid tasks
uint8_t firstTask = 1;

// control
bool priorityScheduler = false;    // priority (true) or round-robin (false)
bool priorityInheritance = false; // priority inheritance for mutexes
bool preemption = false;          // preemption (true) or cooperative (false)

// tcb
#define NUM_PRIORITIES   16
typedef struct _tcb {
    uint8_t state;                 // see STATE_ values above
    void* pid;                     // used to uniquely identify thread (add of task fn)
    void* spInit;                  // original top of stack
    void* sp;                      // current stack pointer
    uint8_t priority;              // 0=highest
    uint8_t currentPriority;       // 0=highest (needed for pi)
    uint32_t ticks;                // ticks until sleep complete
    uint64_t srd;                  // MPU subregion disable bits
    char name[16];                 // name of task used in ps command
    uint8_t mutex;                 // index of the mutex in use or blocking the thread
    uint8_t semaphore;             // index of the semaphore that is blocking the thread
} TCB;
TCB tcb[MAX_TASKS];

extern void setAsp();
extern void setPsp(void* addr); //set the address of the SP, can only be done in privileged i think
extern uint32_t* getPsp();
extern uint32_t* getMsp();
extern void setCtrl(uint32_t mask);
extern void pushR4_R11();
extern void popR11_R4();

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

/*void pidof(const char name[]) {
    fput1sUart0("%s launched\n", name);
    uint8_t i;
    for (i = 0; i < MAX_TASKS; i++) {
        if (str_equal(tcb[i].name, name)) {
            fput1sUart0("Name: %s", name);
            fput1hUart0("PID: %p", tcb[i].pid);
            return;
        }
    }
}*/

bool initMutex(uint8_t mutex) {
    bool ok = (mutex < MAX_MUTEXES);
    if (ok) {
        mutexes[mutex].lock = false;
        mutexes[mutex].lockedBy = 0;
    }
    return ok;
}

bool initSemaphore(uint8_t semaphore, uint8_t count) {
    bool ok = (semaphore < MAX_SEMAPHORES);
    if (ok) {
        semaphores[semaphore].count = count;
    }
    return ok;
}

// REQUIRED: initialize systick for 1ms system timer
void initRtos(void) {
    uint8_t i;

    //init sysTick (1ms)
    NVIC_ST_RELOAD_R = (40e3)-1; //set timer to 1ms
    NVIC_ST_CTRL_R |= NVIC_ST_CTRL_INTEN | NVIC_ST_CTRL_CLK_SRC | NVIC_ST_CTRL_ENABLE; //set clk src to sysclk, enable systick

    // no tasks running
    taskCount = 0;
    // clear out tcb records
    for (i = 0; i < MAX_TASKS; i++) {
        tcb[i].state = STATE_INVALID;
        tcb[i].pid = 0;
    }
}

// REQUIRED: Implement prioritization to NUM_PRIORITIES
uint8_t rtosScheduler(void) {
    bool ok;
    static uint8_t task = 0xFF;
    ok = false;
    while (!ok)
    {
        task++;
        if (task >= MAX_TASKS)
            task = 0;
        ok = (tcb[task].state == STATE_READY);
    }
    return task;
}

// REQUIRED: modify this function to start the operating system
// by calling scheduler, set srd bits, setting PSP, ASP bit, call fn with fn add in R0
// fn set TMPL bit, and PC <= fn
void startRtos(void) {
    setPsp((uint32_t*)0x20008000);
    setAsp(); //set ASP bit to switch to use psp
    setCtrl(1); //set TMPL; move to unpriv (can no longer pendSV)
    __asm(" SVC #0x00"); //start RTOS
}

//automatically pushes value and decrements sp by 4 bytes (32 bits)
void push_to_stack(uint32_t** sp, uint32_t val) {
    (*sp)--;
    **sp = val;
}

//pops and increments stack by 4 bytes and stores value in ptr if != null
void pop_from_stack(uint32_t** sp, uint32_t* ptr) {
    if (ptr != NULL) {
        *ptr = **sp;
    }
    (*sp)++;
}

//makes the thread appear "as if it has run before"
void populateInitialStack(uint32_t** sp, _fn fn) {
    push_to_stack(sp, 1 << 24); //xPSR - THUMB bit (24) has to be correct or will fault (functions defined in thumb), 4 flags are arbitrary
    push_to_stack(sp, (uint32_t)fn); //PC - fn ptr
    push_to_stack(sp, 0xFFFFFFFD); //initial LR - bogus
    push_to_stack(sp, 12); //R12 - bogus
    push_to_stack(sp, 3); //R3 - bogus
    push_to_stack(sp, 2); //R2 - bogus
    push_to_stack(sp, 1); //R1 - bogus
    push_to_stack(sp, 0); //R0 - bogus
    push_to_stack(sp, 0xFFFFFFFD); //push LR (R14)
    push_to_stack(sp, 4); //push R4
    push_to_stack(sp, 5); //push R5
    push_to_stack(sp, 6); //push R6
    push_to_stack(sp, 7); //push R7
    push_to_stack(sp, 8); //push R8
    push_to_stack(sp, 9); //push R9
    push_to_stack(sp, 10); //push R10
    push_to_stack(sp, 11); //push R11
}

// REQUIRED:
// add task if room in task list
// store the thread name
// allocate stack space and store top of stack in sp and spInit
// set the srd bits based on the memory allocation
// initialize the created stack to make it appear the thread has run before
bool createThread(_fn fn, const char name[], uint8_t priority, uint32_t stackBytes) {
    bool ok = false;
    uint8_t i = 0;
    bool found = false;
    if (taskCount < MAX_TASKS) {
        // make sure fn not already in list (prevent reentrancy)
        while (!found && (i < MAX_TASKS)) {
            found = (tcb[i++].pid == fn);
        }
        if (!found) {
            // find first available tcb record
            i = 0;
            uint8_t* alloc = malloc_from_heap(stackBytes);
            if (alloc != NULL) {
                if (str_length(name) < 16) {
                    for (i = 0; tcb[i].state != STATE_INVALID; i++); //set i to next available tcb entry
                    str_copy(tcb[i].name, name); //store thread name
                    tcb[i].state = STATE_READY; //set task state to ready
                    tcb[i].pid = fn; //set pid to function addr
                    uint32_t* sp = (uint32_t*)(alloc + stackBytes);
                    tcb[i].spInit = sp; //set initial stack pointer to stack base
                    tcb[i].sp = sp; //set stack pointer to stack base (stack pointer decrements on push)
                    populateInitialStack(&tcb[i].sp, (uint32_t**)fn); //push everything onto the stack to make it appear as if it has ran before
                    tcb[i].priority = priority;
                    uint64_t taskSrd = createNoSramAccessMask(); //create mask for no sram access
                    addSramAccessWindow(&taskSrd, (uint32_t*)alloc, stackBytes); //modify srd mask to add access to malloc'd region
                    tcb[i].srd = taskSrd; //set task srd to newly created srd
                    // increment task count
                    taskCount++;
                    ok = true;
                }
            }
        }
    }
    return ok;
}

// REQUIRED: modify this function to restart a thread
void restartThread(_fn fn) {

}

// REQUIRED: modify this function to stop a thread
// REQUIRED: remove any pending semaphore waiting, unlock any mutexes
void stopThread(_fn fn) {

}

// REQUIRED: modify this function to set a thread priority
void setThreadPriority(_fn fn, uint8_t priority) {

}

// REQUIRED: modify this function to yield execution back to scheduler using pendsv
void yield(void) {
    __asm(" SVC #0x01"); // call pendSV
}

// REQUIRED: modify this function to support 1ms system timer
// execution yielded back to scheduler until time elapses using pendsv
void sleep(uint32_t tick) {
    __asm(" SVC #0x02");
}

// REQUIRED: modify this function to lock a mutex using pendsv
void lock(int8_t mutex) { //svc 1
    __asm(" SVC #0x03");
}

// REQUIRED: modify this function to unlock a mutex using pendsv
void unlock(int8_t mutex) { //svc 2
    __asm(" SVC #0x04");
}

// REQUIRED: modify this function to wait a semaphore using pendsv
void wait(int8_t semaphore) { //svc 3
    __asm(" SVC #0x05");
}

// REQUIRED: modify this function to signal a semaphore is available using pendsv
void post(int8_t semaphore) { //svc 4
    __asm(" SVC #0x06");
}

// REQUIRED: modify this function to add support for the system timer
// REQUIRED: in preemptive code, add code to request task switch
void sysTickIsr(void) {
    //called every 1ms
    //decrements task ticks and changes state from blocked or ready
    uint32_t i;
    for(i = 0; i < taskCount; i++) {
        if (tcb[i].state == STATE_DELAYED) {
            if (tcb[i].ticks == 0) {
                tcb[i].state = STATE_READY;
                //yield();
            }
            else {
                tcb[i].ticks--;
            }
        }
    }
    //if (preemption) { //if preemption is enabled, context switch to next task
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
    //}
}

// REQUIRED: in coop and preemptive, modify this function to add support for task switching
// REQUIRED: process UNRUN and READY tasks differently
__attribute__((naked)) void pendsvIsr(void) {
    if (NVIC_FAULT_STAT_R & NVIC_FAULT_STAT_DERR) { //if data error
        NVIC_FAULT_STAT_R |= NVIC_FAULT_STAT_DERR; //clear error bit
    }
    if (NVIC_FAULT_STAT_R & NVIC_FAULT_STAT_IERR) { //if instruction error
        NVIC_FAULT_STAT_R |= NVIC_FAULT_STAT_IERR; //clear error bit
    }
    if (!firstTask) {
        __asm(" MRS R0, PSP");
        __asm(" SUB R0, R0, #4");
        __asm(" STR LR, [R0]"); //store LR to stack
        __asm(" MSR PSP, R0");
        pushR4_R11(); //push regs
        tcb[taskCurrent].sp = getPsp(); //save psp
    }
    firstTask = 0;
    taskCurrent = rtosScheduler(); //call scheduler
    applySramAccessMask(tcb[taskCurrent].srd); //restore SRD
    setPsp(tcb[taskCurrent].sp); //restore PSP
    popR11_R4(); //restore all regs (R11-R4)
    __asm(" MRS R0, PSP");
    __asm(" LDR R14, [R0]"); //load LR from stack into R14
    __asm(" ADD R0, R0, #4");
    __asm(" MSR PSP, R0");
    __asm(" BX LR"); //may not be necessary
}

void dequeue(uint8_t q[], uint8_t qsize) {//, uint8_t idx) {
    uint32_t i;
    for(i = 0; i < qsize - 1; i++) {
        q[i] = q[i+1];
    }
}

// REQUIRED: modify this function to add support for the service call
// REQUIRED: in preemptive code, add code to handle synchronization primitives
void svCallIsr(void) {
    uint32_t* psp = getPsp();
    uint32_t* pc = (uint32_t*)(psp[6]);
    uint8_t svcNum = ((uint8_t*)pc)[-2]; //sv call is 2 bytes behind pc
    uint8_t i = psp[0];
    uint32_t tick = psp[0];
    uint8_t next, q;
    switch (svcNum) {
    case START: //start OS
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        break;
    case YIELD: //pendSV
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        break;
    case SLEEP: //sleep
        tcb[taskCurrent].ticks = tick; //i is tick
        tcb[taskCurrent].state = STATE_DELAYED;
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        break;
    case LOCK: //lock mutex
        if (!mutexes[i].lock) {
            mutexes[i].lock = 1;
            mutexes[i].lockedBy = taskCurrent;
        }
        else {
            q = mutexes[i].queueSize;
            mutexes[i].processQueue[q] = taskCurrent;
            mutexes[i].queueSize++;
            tcb[taskCurrent].state = STATE_BLOCKED_MUTEX;
            NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        }
        break;
    case UNLOCK: //unlock mutex
        if (mutexes[i].lockedBy == taskCurrent) {
            mutexes[i].lock = 0;
            if (mutexes[i].queueSize > 0) {
                next = mutexes[i].processQueue[0];
                tcb[next].state = STATE_READY;
                dequeue(mutexes[i].processQueue, mutexes[i].queueSize);
                mutexes[i].lock = 1;
                mutexes[i].lockedBy = next;
                mutexes[i].queueSize--;
            }
        }
        break;
    case WAIT: //wait for semaphore
        if (semaphores[i].count > 0) {
            semaphores[i].count--;
        }
        else {
            q = semaphores[i].queueSize;
            semaphores[i].processQueue[q] = taskCurrent;
            semaphores[i].queueSize++;
            tcb[taskCurrent].state = STATE_BLOCKED_SEMAPHORE;
            tcb[taskCurrent].semaphore = i;
            NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        }
        break;
    case POST: //post to semaphore
        semaphores[i].count++;
        if (semaphores[i].queueSize > 0) {
            next = semaphores[i].processQueue[0];
            tcb[next].state = STATE_READY;
            dequeue(semaphores[i].processQueue, semaphores[i].queueSize);//, 0); //remove 0th element
            semaphores[i].count--;
        }
        break;
    case 0xFF:
        NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
        break;
    }
}

