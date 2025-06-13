#ifndef MACH_VM_HELPER_H
#define MACH_VM_HELPER_H

#include <sys/types.h>
#include <mach/mach.h>
#include <pthread.h>
#include <stdio.h>

kern_return_t get_task_port(pid_t pid, task_t *task_out);
kern_return_t setup_exception_port(pid_t pid);

#endif
