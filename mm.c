#include <mm.h>
#include <stdint.h>
#include <stdlib.h>
#include "tm4c123gh6pm.h"

uint8_t inUse[5]; //40 bit field, 5 regions, 8 sub regions
alloc_entry allocTable[MAX_ALLOCS];
uint32_t n_allocs = 0;

void* calcSubregionAddr(uint32_t g_sr) {
    void* addr = (g_sr < 24) ? (REGION_0_ADDR + g_sr * 512) : (REGION_3_ADDR + (g_sr - 24) * 1024); //beginning of heap addr
    return addr;
}

uint32_t getSubregionFromAddr(void* addr) {
    uint32_t sr;
    uint32_t diff = (uint32_t)addr - REGION_0_ADDR; //0x20008000 - (uint32_t)addr;
    if (diff < 0x3000) {
        //within the 4k size blocks, each subregion is 512
        sr = diff / 512;
    }
    else {
        //in the 8k size blocks, each region 1024, idx is >= 24
        sr = ((diff - 0x3000) / 1024) + 24;
    }
    return sr;
}

void initEntry(alloc_entry* entry, uint32_t size, void* ptr, uint32_t owner) {
    entry->size = size;
    entry->ptr = ptr;
    entry->owner = owner;
}

void push_entry(alloc_entry entry) {
    alloc_entry* cpy = &allocTable[n_allocs++];
    cpy->owner = entry.owner;
    cpy->ptr = entry.ptr;
    cpy->size = entry.size;
    cpy->valid = 1;
}

void delete_entry(uint32_t idx) {
    if (idx < n_allocs) {
        uint32_t i;
        for (i = idx; i < n_allocs-1; i++) {
            initEntry(&allocTable[i], allocTable[i+1].size, allocTable[i+1].ptr, allocTable[i+1].owner);
            allocTable[i+1].valid = 1;
        }
        initEntry(&allocTable[n_allocs-1], 0, 0, 0);//set the last one to all 0
        allocTable[n_allocs-1].valid = 0;
        n_allocs--;
    }
}

void* malloc_from_heap(uint32_t bytes) {
    uint16_t x = bytes <= 512 ? 512 : 1024;
    uint32_t size = (bytes + (x - ((bytes - 1) % x) - 1)); //rounds bytes up to nearest multiple of 512 if bytes <= 512, else multiple of 1024
    uint32_t r, sr;
    //uint32_t sr_mask;
    uint32_t success = 0;
    //alloc_entry* newEntry = &allocTable[n_allocs];
    alloc_entry newEntry;
    for (r = 0; r < 5; r++) {
        uint32_t contig = 0;
        for (sr = 0; sr < 8; sr++) {
            if (!success) {
                if (~inUse[r] & (1 << sr)) { // if the bit at region R and subregion SR is not set, it is not in use
                    contig++;
                    uint32_t g_sr = r * 8 + sr; //global subregion, from 0 to 39
                    uint32_t sr_size = g_sr < 24 ? 512 : 1024;
                    //if we allocating 512B, look for a 512 subregion
                    //if we are allocating >512B look for a 1024*N subregion
                    //size is the N bytes rounded, what we are looking for
                    if (size > sr_size) {
                        //asking for 1024, found 512 a > b
                        //ignore, malloc won't fit in subregion
                        //asking for 2048, found 1024 a > b && a > 1024
                        if (sr_size == 1024) {
                            //split up into blocks
                            uint32_t n_subregions = size / 1024; //should already be in multiples of 1024
                            //find starting subregion?
                            //subregion minus contig minus 1?
                            if (contig == n_subregions) {
                                uint32_t c;
                                uint32_t sr_start = sr - contig + 1;
                                uint32_t g_sr_start = g_sr - contig + 1;
                                for (c = sr_start; c <= sr; c++) {
                                    inUse[r] |= (1 << c); //set c'th subregion to in use.
                                }
                                void* regionaddr = calcSubregionAddr(g_sr_start); //get address of beginning of contiguous memory.
                                initEntry(&newEntry, size, regionaddr, 0);
                                success = 1;
                            }
                        }
                    }
                    else if (size == sr_size) { //if region directly matches what we are looking for,
                        //asking for 512, found 512 a == b
                        //asking for 1024 found 1024 a == b
                        //claim this subregion
                        inUse[r] |= (1 << sr);
                        void* regionaddr = calcSubregionAddr(g_sr);
                        initEntry(&newEntry, size, regionaddr, 0);
                        success = 1;
                    }
                    else if (size < sr_size) {
                        //asking for 512, found 1024 a < b
                        //ignore, should only go to 512
                        //asking for 1024, found anything higher is impossible
                    }
                }
                else {
                    //if in use, reset the contiguous count
                    contig = 0;
                }
            }
        }
    }
    if (success) {
        push_entry(newEntry);
        //addSramAccessWindow(, newEntry.ptr, newEntry.size);
        //how do we get srd Mask here? maybe read directly from reg but idk if we can
        //CALL OUTSIDE FN
        return newEntry.ptr;
        //n_allocs++:
    }
    else {
        return NULL;
    }
}



void free_to_heap(void* ptr) {
    uint32_t i;
    for (i = 0; i < MAX_ALLOCS; i++) {
        alloc_entry entry = allocTable[i];
        if (entry.valid) {
            //check ownership?
            if (entry.ptr == ptr) {
                uint32_t sr = getSubregionFromAddr(ptr);
                uint32_t r = sr / 8;
                inUse[r] &= ~(1 << (sr % 8)); //clear subregion bit
                delete_entry(i); //delete entry from alloc table,
            }
        }
    }
    //check if owned memory before freeing
    //how?

}

uint32_t log2(uint32_t v) {
    uint32_t log = 0;
    while (v >>= 1) {
        log++;
    }
    return log;
}


void initMpu() {
    NVIC_MPU_CTRL_R &= ~NVIC_MPU_CTRL_ENABLE;
    NVIC_MPU_CTRL_R |= NVIC_MPU_CTRL_PRIVDEFEN; //| NVIC_MPU_CTRL_HFNMIENA; //enable bg region (privileged mode only)
    /*
     * Region -1 - Background:  0x00000000 - 0xFFFFFFFF
     * Region 0 - R0:           0x20001000 - 0x20001FFF
     * Region 1 - R1:           0x20002000 - 0x20002FFF
     * Region 2 - R2:           0x20003000 - 0x20003FFF
     * Region 3 - R3:           0x20004000 - 0x20005FFF
     * Region 4 - R4:           0x20006000 - 0x20007FFF
     * Region 5 - Flash:        0x00000000 - 0x0003FFFF
     * Region 6 - Peripheral:   0x40000000 - 0xDFFFFFFF
     */
    /*initMpuRegion(0, REGION_0_ADDR, 4*_kB, 0b011);
    initMpuRegion(1, REGION_1_ADDR, 4*_kB, 0b011);
    initMpuRegion(2, REGION_2_ADDR, 4*_kB, 0b011);
    initMpuRegion(3, REGION_3_ADDR, 8*_kB, 0b011);
    initMpuRegion(4, REGION_4_ADDR, 8*_kB, 0b011);
    initMpuRegion(5, REGION_FLASH_ADDR, 256*_kB, 0b011);
    initMpuRegion(6, REGION_PERIPH_ADDR, 3*_GB, 0b011);*/
    NVIC_MPU_CTRL_R |= NVIC_MPU_CTRL_ENABLE;
}

/*
 * Region size (B) = 2^(SIZE + 1)
 * SIZE = log2(r_size) - 1c
 *
 * 256kB flash = log(256*1024) - 1
 * SIZE = 0x11
 *
 *       | TEX | S | C | B |
 * Flash | 000   0   1   0 |
 * iSRAM | 000   1   1   0 |
 * Perip | 000   1   0   1 |
 */
//NVIC_MPU_ATTR_XN | //Instruction fetches are disabled

/*
 * Region -1 - Background:  0x00000000 - 0xFFFFFFFF
 * Region 0 - R0:           0x20001000 - 0x20001FFF
 * Region 1 - R1:           0x20002000 - 0x20002FFF
 * Region 2 - R2:           0x20003000 - 0x20003FFF
 * Region 3 - R3:           0x20004000 - 0x20005FFF
 * Region 4 - R4:           0x20006000 - 0x20007FFF
 * Region 5 - Flash:        0x00000000 - 0x0003FFFF
 * Region 6 - Peripheral:   0x40000000 - 0xDFFFFFFF
 */

//001 - | P: RW | UP: -- |
//011 - | P: RW | UP: RW |

void allowFlashAccess() {
    NVIC_MPU_NUMBER_R = 5; //flash is region 5 in MPU
    uint32_t N = log2(256*_kB); //N = log2(region size) (256kB) cannot use this line as it requires SRAM
    NVIC_MPU_BASE_R |= (REGION_FLASH_ADDR & NVIC_MPU_BASE_ADDR_M);
    NVIC_MPU_ATTR_R = (NVIC_MPU_ATTR_AP_M & 0b011 << 24) | NVIC_MPU_ATTR_CACHEABLE | ((N-1) << 1) | NVIC_MPU_ATTR_ENABLE; // log2(256k) = 18,
}

void allowPeripheralAccess(void) {
    NVIC_MPU_NUMBER_R = 6; //peripheral is region 6 in MPU
    uint32_t N = log2(64*_MB); //N = log2(region size) (64kB)
    NVIC_MPU_BASE_R |= (REGION_PERIPH_ADDR & NVIC_MPU_BASE_ADDR_M);
    NVIC_MPU_ATTR_R = NVIC_MPU_ATTR_XN | (NVIC_MPU_ATTR_AP_M & 0b011 << 24) |  NVIC_MPU_ATTR_SHAREABLE | NVIC_MPU_ATTR_BUFFRABLE | ((N-1) << 1) | NVIC_MPU_ATTR_ENABLE; // log2(64M) = 18,
}

void setupSramAccess(void) {
    uint32_t N = log2(4*_kB); //N = log2(region size) (256kB)
    NVIC_MPU_NUMBER_R = 0;
    NVIC_MPU_BASE_R |= (REGION_0_ADDR & NVIC_MPU_BASE_ADDR_M);
    NVIC_MPU_ATTR_R =
            NVIC_MPU_ATTR_XN |
            (0b011 << 24) |
            NVIC_MPU_ATTR_SHAREABLE |
            NVIC_MPU_ATTR_BUFFRABLE |
            NVIC_MPU_ATTR_SRD_M |
            (N-1 << 1) |
            NVIC_MPU_ATTR_ENABLE;

    NVIC_MPU_NUMBER_R = 1;
    NVIC_MPU_BASE_R |= (REGION_1_ADDR & NVIC_MPU_BASE_ADDR_M);
    NVIC_MPU_ATTR_R =
            NVIC_MPU_ATTR_XN |
            (0b011 << 24) |
            NVIC_MPU_ATTR_SHAREABLE |
            NVIC_MPU_ATTR_BUFFRABLE |
            NVIC_MPU_ATTR_SRD_M |
            (N-1 << 1) |
            NVIC_MPU_ATTR_ENABLE;

    NVIC_MPU_NUMBER_R = 2;
    NVIC_MPU_BASE_R |= (REGION_2_ADDR & NVIC_MPU_BASE_ADDR_M);
    NVIC_MPU_ATTR_R =
            NVIC_MPU_ATTR_XN |
            (0b011 << 24) |
            NVIC_MPU_ATTR_SHAREABLE |
            NVIC_MPU_ATTR_BUFFRABLE |
            NVIC_MPU_ATTR_SRD_M |
            (N-1 << 1) |
            NVIC_MPU_ATTR_ENABLE;

    N = log2(8*_kB);
    NVIC_MPU_NUMBER_R = 3;
    NVIC_MPU_BASE_R |= (REGION_3_ADDR & NVIC_MPU_BASE_ADDR_M);
    NVIC_MPU_ATTR_R =
            NVIC_MPU_ATTR_XN |
            (0b011 << 24) |
            NVIC_MPU_ATTR_SHAREABLE |
            NVIC_MPU_ATTR_BUFFRABLE |
            NVIC_MPU_ATTR_SRD_M |
            (N-1 << 1) |
            NVIC_MPU_ATTR_ENABLE;

    NVIC_MPU_NUMBER_R = 4;
    NVIC_MPU_BASE_R |= (REGION_4_ADDR & NVIC_MPU_BASE_ADDR_M);
    NVIC_MPU_ATTR_R =
            NVIC_MPU_ATTR_XN |
            (0b011 << 24) |
            NVIC_MPU_ATTR_SHAREABLE |
            NVIC_MPU_ATTR_BUFFRABLE |
            NVIC_MPU_ATTR_SRD_M |
            (N-1 << 1) |
            NVIC_MPU_ATTR_ENABLE;
}


uint64_t createNoSramAccessMask(void) {
    uint64_t srdBitMask = 0x000000FFFFFFFFFF; //bits 39:0 are set since there are 40 subregions to set SRD
    return srdBitMask;
}

void applySramAccessMask(uint64_t srdBitMask) {
    /*
     * 7        6        5        4        3        2        1        0
     *                            87654321 87654321 87654321 87654321 87654321
     *                            88888888 88888888 44444444 44444444 44444444
     * 00000000 00000000 00000000 11111111 11111111 11111111 11111111 11111111
     * 0   0    0   0    0   0    F   F    F   F    F   F    F   F    F   F
     */
    int i;
    for (i = 0; i < 5; i++) { //5 is number of sram regions
        uint8_t region_mask = (srdBitMask >> (8*i)) & 0xFF; //get the 8 bits at Nth region of mask
        NVIC_MPU_NUMBER_R = i; //set MPU to region i
        NVIC_MPU_ATTR_R &= ~NVIC_MPU_ATTR_SRD_M; //clear bits to allow writing.
        NVIC_MPU_ATTR_R |= region_mask << 8; //set SRD field (15:8) to the new region mask
    }
}

void addSramAccessWindow(uint64_t* srdBitMask, uint32_t* baseAdd, uint32_t size_in_bytes) {
    //cannot use 0x20000000 because that region doesnt exist, might need to change?
    //but the SramAccessMask does not account for the OS space, 0x20000000-1000 sooo
    //uint32_t N = (0x20008000 - baseAdd > 4000) ? 9 : 10;
    //uint32_t* addr = baseAdd & ~((1 << N) - 1)
    uint32_t baseregion = getSubregionFromAddr(baseAdd);
    //uint64_t mask = createNoSramAccessMask();
    uint32_t i = 0;
    uint32_t N;
    uint32_t sr = baseregion;
    do {
        N = (sr < 24) ? 512 : 1024;
        *srdBitMask &= ~(1ULL << sr);
        i += N;
        sr++;
    } while (i < size_in_bytes);
    applySramAccessMask(*srdBitMask);
}

