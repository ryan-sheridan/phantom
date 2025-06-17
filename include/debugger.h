#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <stdio.h>
#include <sys/wait.h>   // waitpid
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include "bp_wp.h"

extern int attached_pid;

int attach(pid_t pid);
int interrupt(void);
int resume(void);
int detach(void);
int print_registers(void);
int write_registers(const char reg[], uint64_t value);
int set_breakpoint(uint64_t addr);

int print_debug_registers(void);

#endif
