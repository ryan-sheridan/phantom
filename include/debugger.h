#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <stdio.h>
#include <sys/ptrace.h> // ptrace
#include <sys/wait.h>   // waitpid
#include <signal.h>
#include <string.h>

extern pid_t attached_pid;

int attach(pid_t pid);
int interrupt(void);
int resume(void);
int detach(void);

#endif
