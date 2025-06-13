#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <stdio.h>
#include <sys/ptrace.h> // ptrace
#include <sys/wait.h>   // waitpid
#include <signal.h>
#include <string.h>

static pid_t attached_pid = 0;

int attach(pid_t pid);
int stop(void);
int resume(void);
int detach(void);

#endif
