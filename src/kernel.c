/******************************************************************************
 * File:        kernel.c
 *
 * Author:      Giancarlo Perez
 *
 * Created:     12/7/24
 *
 * Description: -
 ******************************************************************************/

//=============================================================================
// HARDWARE TARGET
//=============================================================================

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

//=============================================================================
// INCLUDES
//=============================================================================

#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "mm.h"
#include "kernel.h"
#include "uart0.h"

//=============================================================================
// DEFINES AND MACROS
//=============================================================================

#ifndef NULL
 #define NULL 0
#endif

// task
#define INVALID_TASK 0xFF
// tcb
#define NUM_PRIORITIES   16

//=============================================================================
// GLOBALS
//=============================================================================

uint8_t taskCurrent = 0;          // index of last dispatched task
uint8_t taskCount = 0;            // total number of valid tasks
uint8_t firstTask = 1;

// control
bool priorityScheduler = true;    // priority (true) or round-robin (false)
bool priorityInheritance = false; // priority inheritance for mutexes
bool preemption = true;          // preemption (true) or cooperative (false)

typedef struct _tcb {
    uint8_t state;                 // see STATE_ values above
    void* pid;                     // used to uniquely identify thread (add of task fn)
    void* spInit;                  // original top of stack
    void* sp;                      // current stack pointer
    uint8_t priority;              // 0=highest
    uint8_t currentPriority;       // 0=highest (needed for pi)
    uint32_t ticks;                // ticks until sleep complete
    uint64_t srd;                  // MPU subregion disable bits
    uint16_t stackSize;            // Stack size of task
    uint32_t elapsed[2];
    uint32_t runtime;
    char name[16];                 // name of task used in ps command
    uint8_t mutex;                 // index of the mutex in use or blocking the thread
    uint8_t semaphore;             // index of the semaphore that is blocking the thread
} TCB;
TCB tcb[MAX_TASKS];

uint32_t systime = 0; //in ticks (ms)
uint8_t pingpong = 0; //write to A(0) or B(1)
//denominator is every time ping pong is switched (2 seconds)

//=============================================================================
// STATIC FUNCTIONS
//=============================================================================

//automatically pushes value and decrements sp by 4 bytes (32 bits)
static void push_to_stack(uint32_t** sp, uint32_t val) {
    (*sp)--;
    **sp = val;
}

//pops and increments stack by 4 bytes and stores value in ptr if != null
static void pop_from_stack(uint32_t** sp, uint32_t* ptr) {
    if (ptr != NULL) {
        *ptr = **sp;
    }
    (*sp)++;
}

//makes the thread appear "as if it has run before"
static void populateInitialStack(uint32_t** sp, _fn fn) {
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

static void dequeue(uint8_t q[], uint8_t qsize, uint8_t idx) {
    uint32_t i;
    for(i = idx; i < qsize - 1; i++) {
        q[i] = q[i+1];
    }
}

static uint32_t taskFromPid(uint32_t pid) {
    uint32_t i;
    for(i = 0; i < MAX_TASKS; i++) {
        if (tcb[i].pid == pid) {
            return i;
        }
    }
    return INVALID_TASK;
}

// REQUIRED: Implement prioritization to NUM_PRIORITIES
static uint8_t rtosScheduler(void) {
    static uint8_t task = 0xFF;
    static uint8_t currIdxPrio[NUM_PRIORITIES] = {0};
    bool ok = false;
    uint8_t i;
    uint8_t highestPrio = 0xFF;
    if (priorityScheduler) {
        for (i = 0; i < MAX_TASKS; i++) {
            if (tcb[i].priority < highestPrio && tcb[i].state == STATE_READY) {
                highestPrio = tcb[i].priority;
            }
        }
    }
    while (!ok) {
        if (priorityScheduler) {
            currIdxPrio[highestPrio]++;
            currIdxPrio[highestPrio] %= taskCount;
            ok = (tcb[currIdxPrio[highestPrio]].state == STATE_READY) && (tcb[currIdxPrio[highestPrio]].priority == highestPrio);
        }
        else {
            task++;
            task %= taskCount;
            ok = (tcb[task].state == STATE_READY);
        }
    }
    return priorityScheduler ? currIdxPrio[highestPrio] : task;
}

//=============================================================================
// PUBLIC FUNCTIONS
//=============================================================================

uint32_t getCurrentTask() {
    return taskCurrent;
}

uint32_t getCurrentPid() {
    return (uint32_t)tcb[taskCurrent].pid;
}

uint32_t getSysTime() {
    return systime;
}

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


void startRtos(void) {
    uint64_t srd = createNoSramAccessMask(); //create temporary SRAM access mask so that startRTOS svCall can push onto stack initially
    addSramAccessWindow(&srd, (uint32_t*)0x20007C00, 0x401);
    applySramAccessMask(srd);
    setPsp((uint32_t*)0x20008000);
    setAsp(); //set ASP bit to switch to use psp
    setCtrl(1); //set TMPL; move to unpriv (can no longer pendSV)
    __asm(" SVC #0x00"); //start RTOS
}


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
            uint8_t* alloc = malloc_(stackBytes); //returns base addr (bottom)
            if (alloc != NULL) {
                if (str_length(name) < 16) {
                    for (i = 0; tcb[i].state != STATE_INVALID; i++); //set i to next available tcb entry
                    str_copy(tcb[i].name, name); //store thread name
                    tcb[i].state = STATE_READY; //set task state to ready
                    tcb[i].pid = fn; //set pid to function addr
                    uint32_t size = (stackBytes + ((stackBytes <= 512 ? 512 : 1024) - ((stackBytes - 1) % (stackBytes <= 512 ? 512 : 1024)) - 1)); //rounds bytes up to nearest multiple of 512 if bytes <= 512, else multiple of 1024
                    uint32_t* sp = (uint32_t*)(alloc + size); //set stack ptr to top of region bc stack decrement //alloc+stackBytes
                    tcb[i].stackSize = size; //stackBytes
                    tcb[i].spInit = sp; //set initial stack pointer to stack base
                    tcb[i].sp = sp; //set stack pointer to stack base (stack pointer decrements on push)
                    populateInitialStack((uint32_t**)&tcb[i].sp, (uint32_t**)fn); //push everything onto the stack to make it appear as if it has ran before
                    tcb[i].priority = priority;
                    tcb[i].elapsed[0] = 0;
                    tcb[i].elapsed[1] = 0;
                    uint64_t taskSrd = createNoSramAccessMask(); //create mask for no sram access
                    addSramAccessWindow(&taskSrd, (uint32_t*)alloc, stackBytes); //modify srd mask to add access to malloc'd region
                    tcb[i].srd = taskSrd; //set task srd to newly created srd
                    tcb[i].semaphore = INVALID_SEMAPHORE;
                    tcb[i].mutex = INVALID_MUTEX;
                    taskCount++; // increment task count
                    taskCurrent++;
                    ok = true;
                }
            }
        }
    }
    return ok;
}

int32_t kill_proc(uint32_t pid) { //svc stuff goes in here
    uint32_t i = taskFromPid(pid);
    uint32_t j, next;
    uint32_t stat = 1;
    if (i != INVALID_TASK) {
        if (tcb[i].state != STATE_STOPPED) {
            //if mutex is locked by the current process
            if (tcb[i].mutex != INVALID_MUTEX) {
                if (mutexes[tcb[i].mutex].lock && mutexes[tcb[i].mutex].lockedBy == i) {
                    mutexes[tcb[i].mutex].lock = 0;
                    if (mutexes[tcb[i].mutex].queueSize > 0) {
                        next = mutexes[tcb[i].mutex].processQueue[0];
                        tcb[next].state = STATE_READY;
                        dequeue(mutexes[tcb[i].mutex].processQueue, mutexes[tcb[i].mutex].queueSize, 0);
                        mutexes[tcb[i].mutex].lock = 1;
                        mutexes[tcb[i].mutex].lockedBy = next;
                        mutexes[tcb[i].mutex].queueSize--;
                    }
                }
                else {
                    for (j = 0; j < mutexes[tcb[i].mutex].queueSize; j++) {
                        if (mutexes[tcb[i].mutex].processQueue[j] == i) {
                            dequeue(mutexes[tcb[i].mutex].processQueue, mutexes[tcb[i].mutex].queueSize, j);
                            mutexes[tcb[i].mutex].queueSize--;
                        }
                    }
                }
            }
            //if not locked by process, check if it's in queue and remove it
            if (tcb[i].semaphore != INVALID_SEMAPHORE) {
                semaphores[tcb[i].semaphore].count++; //increment semaphore
                if (tcb[i].state == STATE_BLOCKED_SEMAPHORE) {
                    //if this task is in the semaphore's queue
                    for (j = 0; j < semaphores[tcb[i].semaphore].queueSize; j++) {
                        if (semaphores[tcb[i].semaphore].processQueue[j] == i) {
                            dequeue(semaphores[tcb[i].semaphore].processQueue, semaphores[tcb[i].semaphore].queueSize, j);
                            semaphores[tcb[i].semaphore].queueSize--;//get rid of it
                            semaphores[tcb[i].semaphore].count--;
                        }
                    }
                    //if another task in queue for semaphore, post it
                    if (semaphores[tcb[i].semaphore].queueSize > 0) {
                        next = semaphores[tcb[i].semaphore].processQueue[0];
                        tcb[next].state = STATE_READY;
                        dequeue(semaphores[tcb[i].semaphore].processQueue, semaphores[tcb[i].semaphore].queueSize, 0);
                        semaphores[tcb[i].semaphore].queueSize--;
                        semaphores[tcb[i].semaphore].count--;
                    }
                }
            }
            //free any allocations that belong to the task
            for (j = 0; j < MAX_ALLOCS; j++) {
                if (allocTable[j].valid && allocTable[j].owner == i) {
                    free_to_heap(allocTable[j].ptr); //how do i check if task freeing owns memory
                }
            }
            tcb[i].state = STATE_STOPPED;
        }
        else {
            stat = -1;
        }
    }
    else {
        stat = -1;
    }
    //set thread to STATE_STOPPED
    //loop through alloc table, if owner is pid free()
    // REQUIRED: remove any pending semaphore waiting, unlock any mutexes
    return stat;
}

void yield(void) {
    __asm(" SVC #0x01"); // SVC_YIELD
}

void sleep(uint32_t tick) {
    __asm(" SVC #0x02");
}

void lock(int8_t mutex) {
    __asm(" SVC #0x03");
}

void unlock(int8_t mutex) {
    __asm(" SVC #0x04");
}

void wait(int8_t semaphore) {
    __asm(" SVC #0x05");
}

void post(int8_t semaphore) {
    __asm(" SVC #0x06");
}

uint32_t stopThread(_fn fn) {
    __asm(" SVC #0x0E");
    //return R0
}

uint32_t restartThread(_fn fn) {
    __asm(" SVC #0x10");
    //return R0
}

void setThreadPriority(_fn fn, uint8_t priority) {
    __asm(" SVC #0x11");
}

void* malloc_from_heap(uint32_t size) {
    __asm(" SVC #0x0F");
    void* addr = (void*)getR0();
    return addr;
}

// REQUIRED: modify this function to add support for the system timer
// REQUIRED: in preemptive code, add code to request task switch
void sysTickIsr(void) {
    //called every 1ms
    //decrements task ticks and changes state from blocked or ready
    systime++;
    uint8_t is1sec = (systime % (PS_REFRESH_TIME*1000) == 0); //calculate mod once
    uint32_t i;
    for(i = 0; i < taskCount; i++) {
        if (tcb[i].state == STATE_DELAYED) {
            if (tcb[i].ticks == 0) {
                tcb[i].state = STATE_READY;
            }
            else {
                tcb[i].ticks--;
            }
        }
        if (tcb[i].state != STATE_INVALID) {
            if (is1sec) {
                tcb[i].elapsed[!pingpong] = 0; //if pingpong, write A, else write B
            }
        }
    }
    if (is1sec) { //if 1000ms reached
        pingpong ^= 1; //flip ping
    }
    if (preemption) { //if preemption is enabled, context switch to next task
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
    }
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
    tcb[taskCurrent].elapsed[pingpong] += (systime - tcb[taskCurrent].runtime);
    taskCurrent = rtosScheduler(); //call scheduler
    tcb[taskCurrent].runtime = systime; //records the starting time of the task
    applySramAccessMask(tcb[taskCurrent].srd); //restore SRD
    setPsp(tcb[taskCurrent].sp); //restore PSP
    popR11_R4(); //restore all regs (R11-R4)
    __asm(" MRS R0, PSP");
    __asm(" LDR R14, [R0]"); //load LR from stack into R14
    __asm(" ADD R0, R0, #4");
    __asm(" MSR PSP, R0");
    __asm(" BX LR"); //may not be necessary
}

// REQUIRED: modify this function to add support for the service call
// REQUIRED: in preemptive code, add code to handle synchronization primitives
void svCallIsr(void) {
    /*
     * xPSR PSP + 7
     * PC   PSP + 6
     * LR   PSP + 5
     * R12  PSP + 4
     * R3   PSP + 3
     * R2   PSP + 2
     * R1   PSP + 1
     * R0   PSP + 0
     */
    uint32_t* psp = getPsp();
    uint32_t* pc = (uint32_t*)(psp[6]);
    uint8_t svcNum = ((uint8_t*)pc)[-2]; //sv call is 2 bytes behind pc

    uint8_t R0_8b = psp[0]; //first parameter of function calling svCall - uint8
    uint32_t R0_32b = psp[0]; //first parameter of function calling svCall - uint32
    uint8_t R1_8b = psp[1];

    psInfo* psinfo = (psInfo*)psp[0]; //used in ps
    const char* str = (const char*)psp[0]; //used in pidof
    ipcsInfo* ipcsinfo = (ipcsInfo*)psp[0]; //used in ipcs
    memInfo* minfo = (memInfo*)psp[0];

    uint8_t next, q, prio;
    uint32_t i, j, tick, pid, size;
    void* mallocAddr;
    switch (svcNum) {
    case SVC_START: //start OS
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        break;
    case SVC_YIELD: //pendSV
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        break;
    case SVC_SLEEP: //sleep
        tick = R0_32b;
        tcb[taskCurrent].ticks = tick;
        tcb[taskCurrent].state = STATE_DELAYED;
        NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        break;
    case SVC_LOCK: //lock mutex i
        i = R0_8b;
        tcb[taskCurrent].mutex = i;
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
    case SVC_UNLOCK: //unlock mutex i
        i = R0_8b;
        if (mutexes[i].lockedBy == taskCurrent) {
            mutexes[i].lock = 0;
            tcb[taskCurrent].mutex = INVALID_MUTEX;
            if (mutexes[i].queueSize > 0) {
                next = mutexes[i].processQueue[0]; //get next task in mutex queue

                dequeue(mutexes[i].processQueue, mutexes[i].queueSize, 0); //remove next task from queue
                mutexes[i].queueSize--;

                //next task locks mutex
                mutexes[i].lock = 1;
                mutexes[i].lockedBy = next;
                tcb[next].mutex = i;
                tcb[next].state = STATE_READY;
            }
        }
        break;
    case SVC_WAIT:
        i = R0_8b;
        tcb[taskCurrent].semaphore = i;
        if (semaphores[i].count > 0) {
            semaphores[i].count--;
        }
        else {
            semaphores[i].processQueue[semaphores[i].queueSize++] = taskCurrent;
            tcb[taskCurrent].state = STATE_BLOCKED_SEMAPHORE;
            NVIC_INT_CTRL_R |= NVIC_INT_CTRL_PEND_SV;
        }
        break;
    case SVC_POST:
        i = R0_8b;
        semaphores[i].count++;
        tcb[taskCurrent].semaphore = INVALID_SEMAPHORE;
        if (semaphores[i].queueSize > 0) {
            next = semaphores[i].processQueue[0];

            dequeue(semaphores[i].processQueue, semaphores[i].queueSize, 0);
            semaphores[i].queueSize--;

            semaphores[i].count--;
            tcb[next].state = STATE_READY;
        }
        break;
    case SVC_PRIO:
        i = R0_8b;
        priorityScheduler = i;
        break;
    /*case SVC_PI:
        i = R0_8b;
        priorityInheritance = i;
        break;*/
    case SVC_PREEMPTION:
        i = R0_8b;
        preemption = i;
        break;
    case SVC_PS:
        for (i = 0; i < taskCount; i++) {
            psinfo[i].pid = tcb[i].pid;
            str_copy(psinfo[i].name, tcb[i].name);
            psinfo[i].runtime = tcb[i].elapsed[!pingpong];
            psinfo[i].state = tcb[i].state;
            psinfo[i].prio = tcb[i].priority;
            if (tcb[i].state == STATE_BLOCKED_MUTEX) {
                psinfo[i].mutex_or_sem = tcb[i].mutex;
            }
            else if (tcb[i].state == STATE_BLOCKED_SEMAPHORE) {
                psinfo[i].mutex_or_sem = tcb[i].semaphore;
            }
            else {
                psinfo[i].mutex_or_sem = 0xFF;
            }
        }
        break;
    case SVC_IPCS:
        for (i = 0; i < MAX_TASKS; i++) {
            if (tcb[i].state != STATE_INVALID) {
                str_copy(ipcsinfo->nameArr[i], tcb[i].name);
            }
            else {
                str_copy(ipcsinfo->nameArr[i], "INVALID_TASK");
            }
        }
        for (i = 0; i < MAX_MUTEXES; i++) {
            ipcsinfo->mutexes[i].lock = mutexes[i].lock;
            ipcsinfo->mutexes[i].queueSize = mutexes[i].queueSize;
            for (j = 0; j < MAX_MUTEX_QUEUE_SIZE; j++) {
                ipcsinfo->mutexes[i].processQueue[j] = mutexes[i].processQueue[j];
            }
            ipcsinfo->mutexes[i].lockedBy = mutexes[i].lockedBy;
        }
        for (i = 0; i < MAX_SEMAPHORES; i++) {
            ipcsinfo->semaphores[i].count = semaphores[i].count;
            ipcsinfo->semaphores[i].queueSize = semaphores[i].queueSize;
            for (j = 0; j < MAX_MUTEX_QUEUE_SIZE; j++) {
                ipcsinfo->semaphores[i].processQueue[j] = semaphores[i].processQueue[j];
            }
        }
    case SVC_PIDOF:
        psp[0] = 0;
        for (i = 0; i < MAX_TASKS; i++) {
            if (str_equal(tcb[i].name, str)) {
                psp[0] = (uint32_t)(tcb[i].pid);
            }
        }
        break;
    case SVC_MEMINFO:
        //print stuff in Alloctable
        for (i = 0; i < MAX_ALLOCS; i++) {
            uint32_t o = allocTable[i].owner; //owner does not get set when creating thread, fix this
            if (allocTable[i].valid) {
                minfo[i].valid = true;
                minfo[i].baseAdd = allocTable[i].ptr;
                str_copy(minfo[i].ownerName, tcb[o].name);// = allocTable[i].owner;
                minfo[i].size = allocTable[i].size;
                minfo[i].usage = (1000*((uint32_t)tcb[o].spInit - (uint32_t)tcb[o].sp))/(tcb[o].stackSize);
            }
        }
        break;
    case SVC_STOPTHREAD:
        pid = R0_32b;
        psp[0] = kill_proc(pid);
        break;
    case SVC_MALLOC:
        size = R0_32b;
        mallocAddr = malloc_(size);
        addSramAccessWindow(&tcb[taskCurrent].srd, (uint32_t*)mallocAddr, size); //add the new malloc into the task's srd
        applySramAccessMask(tcb[taskCurrent].srd); //apply the srd to the CPU until next context switch
        psp[0] = (uint32_t*)mallocAddr;
        break;
    case SVC_RESTARTTHREAD:
        pid = R0_32b;
        i = taskFromPid(pid);
        psp[0] = 1; //return status code
        if (i != INVALID_TASK) {
            if (tcb[i].state == STATE_STOPPED) {
                uint8_t* alloc = malloc_(tcb[i].stackSize); //returns base addr (bottom)
                if (alloc != NULL) {
                    uint32_t k;
                    for (k = 0; k < MAX_ALLOCS; k++) {
                        if (allocTable[k].ptr == alloc) {
                            allocTable[k].owner = i;
                        }
                    }
                    uint32_t* sp = (uint32_t*)(alloc + tcb[i].stackSize);//tcb[i].spInit; //set stack ptr to top of region bc stack decrement
                    tcb[i].spInit = sp; //set initial stack pointer to stack base
                    tcb[i].sp = sp; //set stack pointer to stack base (stack pointer decrements on push)
                    populateInitialStack((uint32_t**)&tcb[i].sp, (uint32_t**)pid); //push everything onto the stack to make it appear as if it has ran before
                    uint64_t taskSrd = createNoSramAccessMask(); //create mask for no sram access
                    addSramAccessWindow(&taskSrd, (uint32_t*)(tcb[i].spInit-tcb[i].stackSize), tcb[i].stackSize); //modify srd mask to add access to malloc'd region
                    tcb[i].srd = taskSrd; //set task srd to newly created srd
                    tcb[i].state = STATE_READY; //set task state to ready
                    tcb[i].semaphore = INVALID_SEMAPHORE;
                    tcb[i].mutex = INVALID_MUTEX;
                }
                else {
                    //malloc could not find space
                    psp[0] = 0; //RETURN ERROR_INSUFFICIENT_MEMORY
                }
            }
            else {
                //cannot restart thread, either running or invalid
                psp[0] = 0; //RETURN ERROR_INVALID_THREAD
            }
            /*
             * restartThread will never actually restart the thread, it will only
             * reconfigure it as creatThread and set the state as ready
             * does not pendsv
             */
        }
        break;
    case SVC_SETPRIORITY:
        pid = R0_32b;
        prio = R1_8b;
        i = taskFromPid(pid);
        if (i != INVALID_TASK) {
            tcb[i].priority = prio;
        }
        break;
    case SVC_REBOOT:
        NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
        break;
    }
    /*
     * R0
     * R1
     * R2
     * R3
     * R12
     * LR
     * PC
     * xPSR
     */
}

