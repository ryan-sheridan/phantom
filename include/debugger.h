#ifndef DEBUGGER_H
#define DEBUGGER_H

#include <stdio.h>
#include <sys/ptrace.h> // ptrace
#include <sys/wait.h>   // waitpid
#include <signal.h>
#include <string.h>

int attach(pid_t pid);

#endif
