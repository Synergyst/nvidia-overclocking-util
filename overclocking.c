/*
            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
                    Version 2, December 2004
 Copyright (C) 2004 Sam Hocevar <sam@hocevar.net>
 Everyone is permitted to copy and distribute verbatim or modified
 copies of this license document, and changing it is allowed as long
 as the name is changed.
            DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE
   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION
  0. You just DO WHAT THE FUCK YOU WANT TO.
*/

/*
 * Some changes (or lack thereof) made to this file include:
 * 
 * 
 * Preserved original author's license
 * 
 * Added general comments for future readers (TODO: add even more comments)
 * 
 * Fixed some minor issues I've noticed
 * 
 * Added getopt.h library to ease implementing as a standalone console app (I have no clue how to reverse-engineer, otherwise this utility would've been a simple DLL)
 * 
 * Allowed overclocking either memory AND/OR core, instead of just core OR core AND memory. :)
 * 
 * Added GDDR6X support to memory type string list
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "nvapi.h"
// This include is really only useful for easing implementation for using this overclocking utility as a standalone console application rather than a DLL/library.
#include "getopt.h"

//#define EXPORT __declspec(dllexport)

char* get_memtype_string(int memtype) {
    switch (memtype) {
        case 0:
            return "\0";
            break;
        case 1:
            return "SDR";
            break;
        case 2:
            return "DDR";
            break;
        case 3:
        case 9:
            return "DDR2";
            break;
        case 4:
            return "GDDR2";
            break;
        case 5:
            return "GDDR3";
            break;
        case 6:
            return "GDDR4";
            break;
        case 7:
            return "DDR3";
            break;
        case 8:
            return"GDDR5";
            break;
        case 10:
            return "GDDR5X";
            break;
        case 11:
        case 13:
        default:
            printf("([debug] memtype code: %d)\t", memtype);
            return "Unknown memory type";
            break;
        case 12:
            return "HBM2";
            break;
        case 14:
            return "GDDR6";
            break;
        case 15:
            return "GDDR6X";
            break;
    }
}

int get_memfreq_multiplier(int memtype) {
    switch (memtype) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 9:
        case 12:
        default:
            return 1;
        case 8:
            return 2;
        case 14:
            return 4;
    }
}

int main(int argc, char** argv) {
    /*
     * nGPU = number of GPU's
     * systype = system type
     * memsize = capacity
     * memtype = NVAPI memory type as int
     * hdlGPU = NVAPI GPU handle array
     * sysname = Full name of GPU provided by NVAPI
     * biosname = VBIOS revision provided by NVAPI for given GPU
     */
    int nGPU = 0;
    int systype;
    int memsize[64], memtype[64];
    int* hdlGPU[64], * buf = 0;
    char sysname[64];
    char* biosname[64];
    NV_GPU_PERF_PSTATES20_INFO_V1 pstates_info[64], pstate_overclock;
    NV_GPU_CLOCK_FREQUENCIES_V2 clock_freq[64];
    memset(clock_freq, 0, sizeof(NV_GPU_CLOCK_FREQUENCIES_V2) * 64);

    NvInit();
    NvEnumGPUs(hdlGPU, &nGPU);
    NvGetName(hdlGPU[0], sysname);
    NvGetSysType(hdlGPU[0], &systype);

    for (int i = 0; i < nGPU; i++) {
        pstates_info[i].version = 0x11c94;
        biosname[i] = malloc(64);
        NvGetMemSize(hdlGPU[i], &memsize[i]);
        NvGetMemType(hdlGPU[i], &memtype[i]);
        NvGetBiosName(hdlGPU[i], biosname[i]);
        clock_freq[i].version = 0x20108; // set struct version
        clock_freq[i].ClockType = 1; // request base clock
        NvGetFreq(hdlGPU[i], &clock_freq[i]);
        NvGetPstates(hdlGPU[i], &pstates_info[i]);
    }

    /*
     * NOTE: This commented block below could be useful for ridding NVML errors in some cases
     * 
     * That or perhaps detecting if the user is using a laptop, then producing a debug/warn message
     * informing them that some laptops may have locked-out controls.
     * 
     */
    /*switch (systype) {
        case 1:     printf("\nType: Laptop\n"); break;
        case 2:     printf("\nType: Desktop\n"); break;
        default:    printf("\nType: Unknown\n"); break;
    }*/

    /*
     * NOTE: Uncomment this block below to print debug information without setting a new overclock
     */
    printf("Name: %s\n", sysname);
    printf("Detected %d GPU(s)\n", nGPU);
    for (int i = 0; i < nGPU; i++) {
        printf("Settings for GPU %d\n", i);
        printf("VRAM: %dMB %s\n", memsize[i] / 1024, get_memtype_string(memtype[i]));
        printf("BIOS: %s\n", biosname[i]);
        free(biosname[i]);
        printf("\nGPU: %dMHz\n", (int)((clock_freq[i].domain[0].frequency) + ((pstates_info[i].pstates[0].clocks[0]).freqDelta_kHz.value)) / 1000);
        printf("VRAM: %dMHz\n", (int)((pstates_info[i].pstates[0].clocks[1]).data.single.freq_kHz) / get_memfreq_multiplier(memtype[i]) / 1000);
        printf("\nCurrent GPU OC: %dMHz\n", (int)((pstates_info[i].pstates[0].clocks[0]).freqDelta_kHz.value) / 1000);
        printf("Current VRAM OC: %dMHz\n\n", (int)((pstates_info[i].pstates[0].clocks[1]).freqDelta_kHz.value) / get_memfreq_multiplier(memtype[i]) / 1000);
    }
    Sleep(100);
    exit(0);

    int cardId = 0, cur_gpu = 0;
    int gpufreq_mhz = 0, vramfreq_mhz = 0;
    bool doOverclockCore = false, doOverclockMemory = false;
    int opt;
    // TODO: this getopt library I'm using works.. though I have it implemented here horrifically. Please ignore/fix/remove/replace with something better. :)
    while ((opt = getopt(argc, argv, ":i:z:d:c:m:")) != -1) {
        switch (opt) {
            case 'i':
                cur_gpu = atoi(optarg);
                if (cur_gpu >= nGPU) {
                    printf("[GPU #%d] not found!\nPlease specify a GPU within the range 0 to %d", cur_gpu, nGPU - 1);
                    return 0;
                }
                cardId = optarg;
                //printf("Using [GPU #%d]\n", cur_gpu);
                break;
            case 'z':
                cardId = atoi(optarg);
                printf("%d", (int)((pstates_info[cardId].pstates[0].clocks[1]).freqDelta_kHz.value) / get_memfreq_multiplier(memtype[cardId]) / 1000);
                return 0;
                break;
            case 'd':
                printf("Name: %s\n", sysname);
                printf("Detected %d GPU(s)\n", nGPU);
                for (int i = 0; i < nGPU; i++) {
                    printf("Settings for GPU %d\n", i);
                    printf("VRAM: %dMB %s\n", memsize[i] / 1024, get_memtype_string(memtype[i]));
                    printf("BIOS: %s\n", biosname[i]);
                    free(biosname[i]);
                    printf("GPU: %dMHz\n", (int)((clock_freq[i].domain[0].frequency) + ((pstates_info[i].pstates[0].clocks[0]).freqDelta_kHz.value)) / 1000);
                    printf("VRAM: %dMHz\n", (int)((pstates_info[i].pstates[0].clocks[1]).data.single.freq_kHz) / get_memfreq_multiplier(memtype[i]) / 1000);
                    printf("Current GPU OC: %dMHz\n", (int)((pstates_info[i].pstates[0].clocks[0]).freqDelta_kHz.value) / 1000);
                    printf("Current VRAM OC: %dMHz\n\n", (int)((pstates_info[i].pstates[0].clocks[1]).freqDelta_kHz.value) / get_memfreq_multiplier(memtype[i]) / 1000);
                }
                return 0;
                break;
            case 'c':
                gpufreq_mhz = atoi(optarg);
                //printf("Attempting to set [GPU #%d] core offset: %d Mhz\n", cur_gpu, gpufreq_mhz);
                doOverclockCore = true;
                break;
            case 'm':
                vramfreq_mhz = atoi(optarg);
                //printf("Attempting to set [GPU #%d] memory offset: %d Mhz\n", cur_gpu, vramfreq_mhz);
                doOverclockMemory = true;
                break;
            case ':':
                printf("option needs a value\n");
                break;
            case '?':
                printf("unknown option : %c\n", optopt);
                break;
        }
    }
    if (!(-1000 <= gpufreq_mhz && gpufreq_mhz <= 1000)) {
        printf("[core] %d Mhz is out of the safe range!\n", gpufreq_mhz);
        return 0;
    } else {
        if (doOverclockCore) {
            memset(&pstate_overclock, 0, sizeof(NV_GPU_PERF_PSTATES20_INFO_V1));
            pstate_overclock.version = 0x11c94;
            pstate_overclock.numPstates = 1;
            pstate_overclock.numClocks = 1;
            pstate_overclock.pstates[0].clocks[0].domainId = 0; // GPU core clock domain
            pstate_overclock.pstates[0].clocks[0].freqDelta_kHz.value = gpufreq_mhz * 1000;
            if (NvSetPstates(hdlGPU[cur_gpu], &pstate_overclock)) {
                printf("Core OC failed!\n");
            } else {
                printf("%d", gpufreq_mhz);
            }
        }
    }
    if (!(-500 <= vramfreq_mhz && vramfreq_mhz <= 1500)) {
        printf("[memory] %d Mhz is out of the safe range!\n", vramfreq_mhz);
        return 0;
    } else {
        if (doOverclockMemory) {
            memset(&pstate_overclock, 0, sizeof(NV_GPU_PERF_PSTATES20_INFO_V1));
            pstate_overclock.version = 0x11c94;
            pstate_overclock.numPstates = 1;
            pstate_overclock.numClocks = 2;
            pstate_overclock.pstates[0].clocks[1].domainId = 4; // VRAM clock domain
            pstate_overclock.pstates[0].clocks[1].freqDelta_kHz.value = vramfreq_mhz * 1000 * get_memfreq_multiplier(memtype[cur_gpu]);
            if (NvSetPstates(hdlGPU[cur_gpu], &pstate_overclock)) {
                printf("Memory OC failed!\n");
            } else {
                printf("%d", vramfreq_mhz);
            }
        }
    }
    NvUnload();
    return 0;
}

/* 

*****
TODO: Add call to nvidia-smi.exe to lock core clocks.
Currently this utility will only set offset-based memory and core over/underclocks.
Power limits can already be set via the public NVAPI (non-NDA version).. so that will not be included in this file.
*****

EXPORT int SetCoreClockOffset() {
    HANDLE thread = CreateThread(NULL, 0, ThreadFunc, NULL, 0, NULL);
    if (thread) {
        // Optionally do stuff, such as wait on the thread.
    }
    printf("\nTesting from DLL\n\n");
    return 0;
}
EXPORT void GetCoreClockOffset() {
    //
}
EXPORT void SetMemoryClockOffset() {
    //
}
EXPORT void GetMemoryClockOffset() {
    //
}
EXPORT void SetPowerLimit() {
    //
}
EXPORT void GetPowerLimit() {
    //
}*/