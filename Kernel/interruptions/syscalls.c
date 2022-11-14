// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "syscalls.h"
#include "keyboard.h"
#include "snapshot.h"
#include "console.h"
#include "time.h"
#include "interrupts.h"
#include "memoryManager.h"
#include "processManager.h"
#include "sem.h"
#include "pipes.h"
#include "string.h"
#include "stdio.h"

uint8_t getRTC(uint64_t rtcID)
{
    uint8_t x = _getRTCInfo(rtcID);
    uint8_t result = ((x / 16) * 10) + (x & 0xf);
    return result;
}

void sleep(uint64_t seconds)
{
    int startingSeconds = seconds_elapsed();
    int currentSeconds = startingSeconds;
    while ((currentSeconds - startingSeconds) <= seconds)
    {
        yield();
        _hlt();
        currentSeconds = seconds_elapsed();
    }
}

int inforeg(uint64_t *buffer)
{
    loadRegisters(buffer);
    return 0;
}

void getMem(uint64_t direc, uint8_t *buffer, uint64_t bytes)
{
    for (uint8_t i = 0; i < bytes; i++)
    {
        buffer[i] = (uint8_t)_getMem(direc + i); // llamo 'bytes'(32) veces a _getMem con 32 posiciones de memoria distintos
    }
}

uint64_t syscallHandler(syscallId rax, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
    void *mem;
    switch (rax)
    {
    case SYS_READ:
        return sysRead((char *)arg0);
    case SYS_WRITE:
        return sysWrite((char *)arg0, (uint64_t)arg1, (color_t *)arg2);
    case SYS_CLEAN_SCREEN:
        clearScreen();
        return 0;
    case SYS_INFOREG:
        return inforeg((uint64_t *)arg1);
    case SYS_DATENTIME:
        return getRTC((uint64_t)arg0);
    case SYS_PRINTMEM:
        getMem((uint64_t)arg0, (uint8_t *)arg1, (uint64_t)arg2);
        return 0;
    case SYS_SET_CURSOR:
        setCursor((int)arg0);
        return 0;
    case SYS_WAIT:
        sleep((uint64_t)arg0);
        return 0;
    case SYS_MALLOC:
        mem = malloc(arg0);
        addMalloc(mem);
        return (uint64_t) mem;
    case SYS_FREE:
        freeMalloc((void *)arg0);
        free((void *)arg0);
        return 0;
    case SYS_GETMEMINFO:
        return (uint64_t)getMemoryInfo();
    case SYS_CREATE_PROCESS:
        return startTask((void (*)(int argc, char **argv))arg0, (int)arg1, (char **)arg2, (int)arg3, (int *)arg4);
    case SYS_GETPID:
        return getpid();
    case SYS_GETPROCESSESINFO:
        return (uint64_t)getSchedulerInfo();
    case SYS_KILL:
        return killTask((int)arg0);
    case SYS_EXIT:
        return killCurrent();
    case SYS_NICE:
        nice((int)arg0, (int)arg1);
        return 0;
    case SYS_BLOCK:
        blockTask((int)arg0);
        return 0;
    case SYS_RESUME:
        resumeTask((int)arg0);
        return 0;
    case SYS_YIELD:
        yield();
        return 0;
    case SYS_WAITPID:
        return waitpid((int)arg0);
    case SYS_SEMOPEN:
        return semOpen((char *)arg0, (int)arg1);
    case SYS_SEMINIT:
        return semInit((int)arg0);
    case SYS_SEMWAIT:
        return semWait((int)arg0);
    case SYS_SEMPOST:
        return semPost((int)arg0);
    case SYS_SEMCLOSE:
        return semClose((int)arg0);
    case SYS_SEMPRINT:
        return (uint64_t)getSemInfo();
    case SYS_PIPEOPEN:
        return pipeOpen((int)arg0);
    case SYS_PIPECLOSE:
        return pipeClose((int)arg0);
    case SYS_PIPEREAD:
        return pipeRead((int)arg0);
    case SYS_PIPEWRITE:
        return pipeWrite((int)arg0, (char *)arg1);
    case SYS_PIPEPRINT:
        return (uint64_t)getPipeInfo();
    }
    return -1;
}
