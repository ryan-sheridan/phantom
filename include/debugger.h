#ifndef DEBUGGER_H
#define DEBUGGER_H

#include "bp_wp.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h> // waitpid

extern int attached_pid;

int attach(pid_t pid);
int interrupt(void);
int resume(void);
int detach(void);
int print_registers(void);
int write_registers(const char reg[], uint64_t value);
int set_breakpoint(uint64_t addr);

int print_debug_registers(void);

// vmrw
int read64(uintptr_t addr);
int read32(uintptr_t addr);

int write64(uintptr_t addr, uint64_t bytes);
int write32(uintptr_t addr, uint32_t bytes);

#endif
