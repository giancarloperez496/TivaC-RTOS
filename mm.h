#ifndef MM_H_
#define MM_H_

#include <stdint.h>

#define REGION_FLASH_ADDR 0x00000000
#define REGION_OS_ADDR 0x20000000
#define REGION_0_ADDR 0x20001000
#define REGION_1_ADDR 0x20002000
#define REGION_2_ADDR 0x20003000
#define REGION_3_ADDR 0x20004000
#define REGION_4_ADDR 0x20006000
#define REGION_0_ADDR_TOP 0x20008000
#define REGION_1_ADDR_TOP 0x20006000
#define REGION_2_ADDR_TOP 0x20004000
#define REGION_3_ADDR_TOP 0x20003000
#define REGION_4_ADDR_TOP 0x20008000
#define REGION_PERIPH_ADDR 0x40000000
#define _kB 1024
#define _MB 1024*1024
#define _GB 1024*1024*1024
#define MAX_ALLOCS 12

typedef struct {
    void* ptr;
    uint32_t size;
    uint32_t owner;
    uint8_t valid;
} alloc_entry;

void setRegionAddr(uint32_t region, void* addr);
void initMpu();
void* calcSubregionAddr(uint32_t g_sr);
uint32_t getSubregionFromAddr(void* addr);
void initEntry(alloc_entry* entry, uint32_t size, void* ptr, uint32_t owner);
void push_entry(alloc_entry entry);
void delete_entry(uint32_t idx);
void* malloc_from_heap(uint32_t bytes);
void free_to_heap(void* ptr);
void allowFlashAccess(void);
void allowPeripheralAccess(void);
void setupSramAccess(void);
uint64_t createNoSramAccessMask(void);
void applySramAccessMask();
void addSramAccessWindow(uint64_t* srdBitMask, uint32_t* baseAdd, uint32_t size_in_bytes);

#endif
