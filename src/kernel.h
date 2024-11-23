// Kernel functions
// J Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef KERNEL_H_
#define KERNEL_H_

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>

//-----------------------------------------------------------------------------
// RTOS Defines and Kernel Variables
//-----------------------------------------------------------------------------

// function pointer
typedef void (*_fn)();

// mutex
#define MAX_MUTEXES 1
#define MAX_MUTEX_QUEUE_SIZE 2
#define resource 0

// semaphore
#define MAX_SEMAPHORES 3
#define MAX_SEMAPHORE_QUEUE_SIZE 2
#define keyPressed 0
#define keyReleased 1
#define flashReq 2

// tasks
#define MAX_TASKS 12
#define PS_REFRESH_TIME 1

// task states
#define STATE_INVALID           0 // no task
#define STATE_STOPPED           1 // stopped, all memory freed
#define STATE_READY             2 // has run, can resume at any time
#define STATE_DELAYED           3 // has run, but now awaiting timer
#define STATE_BLOCKED_MUTEX     4 // has run, but now blocked by semaphore
#define STATE_BLOCKED_SEMAPHORE 5 // has run, but now blocked by semaphore

//sv calls
#define SVC_START           0x00
#define SVC_YIELD           0x01
#define SVC_SLEEP           0x02
#define SVC_LOCK            0x03
#define SVC_UNLOCK          0x04
#define SVC_WAIT            0x05
#define SVC_POST            0x06
#define SVC_PRIO            0x07
#define SVC_PI              0x08
#define SVC_PREEMPTION      0x09
#define SVC_PS              0x0A
#define SVC_IPCS            0x0B
#define SVC_PIDOF           0x0C
#define SVC_MEMINFO         0x0D
#define SVC_STOPTHREAD      0x0E
#define SVC_MALLOC          0x0F
#define SVC_RESTARTTHREAD   0x10
#define SVC_SETPRIORITY     0x11
#define SVC_KILL            0x12

#define SVC_REBOOT          0xFF

// mutex
#define INVALID_MUTEX 0xFF
typedef struct _mutex {
    bool lock;
    uint8_t queueSize;
    uint8_t processQueue[MAX_MUTEX_QUEUE_SIZE];
    uint8_t lockedBy;
} mutex;
mutex mutexes[MAX_MUTEXES];

// semaphore
#define INVALID_SEMAPHORE 0xFF
typedef struct _semaphore {
    uint8_t count;
    uint8_t queueSize;
    uint8_t processQueue[MAX_SEMAPHORE_QUEUE_SIZE];
} semaphore;
semaphore semaphores[MAX_SEMAPHORES];


//ps SVC will write to this struct then return it back to caller
typedef struct _psInfo {
    void* pid; //pid
    char name[16]; //needs to be strcpied
    //uint16_t cpuTime; //format 70.44% -> 7044
    uint32_t prio;
    uint32_t runtime; //ticks
    uint8_t state; //if blocked mutex -> mutex, else if blocked semaphore -> semaphore
    uint8_t mutex_or_sem;
} psInfo;

typedef struct _ipcsInfo {
    mutex mutexes[MAX_MUTEXES];
    semaphore semaphores[MAX_SEMAPHORES];
    char nameArr[MAX_TASKS][16];
} ipcsInfo;

typedef struct _memInfo {//maybe add owner index later
    char ownerName[16];
    uint8_t valid;
    void* baseAdd;
    uint32_t size;
    uint32_t usage;
} memInfo;

//-----------------------------------------------------------------------------
// External Functions
//-----------------------------------------------------------------------------

extern void setAsp();
extern void setPsp(void* addr); //set the address of the SP, can only be done in privileged i think
extern uint32_t* getPsp();
extern uint32_t* getMsp();
extern void setCtrl(uint32_t mask);
extern void pushR4_R11();
extern void popR11_R4();
extern uint32_t getR0();

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

bool initMutex(uint8_t mutex);
bool initSemaphore(uint8_t semaphore, uint8_t count);
void initRtos(void);
void startRtos(void);
bool createThread(_fn fn, const char name[], uint8_t priority, uint32_t stackBytes);
uint32_t kill_proc(uint32_t pid);
uint32_t restartThread(_fn fn);
uint32_t stopThread(_fn fn);

void setThreadPriority(_fn fn, uint8_t priority);
uint32_t getCurrentTask();
uint32_t getCurrentPid();
uint32_t getSysTime();

void yield(void);
void sleep(uint32_t tick);
void lock(int8_t mutex);
void unlock(int8_t mutex);
void wait(int8_t semaphore);
void post(int8_t semaphore);

void sysTickIsr(void);
void pendsvIsr(void);
void svCallIsr(void);

#endif
