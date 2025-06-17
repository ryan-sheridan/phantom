#ifndef MACH_PROCESS_H
#define MACH_PROCESS_H

#include <stdint.h>
#include <sys/types.h>
#include <mach/mach.h>
#include <mach/exc.h>
#include <pthread.h>
#include <stdio.h>

kern_return_t get_task_port(pid_t pid, task_t *task_out);
kern_return_t setup_exception_port(pid_t pid);

kern_return_t mach_resume(void);
kern_return_t mach_suspend(void);
kern_return_t mach_detach(void);
kern_return_t mach_register_print(void);
kern_return_t mach_register_write(const char reg[], uint64_t value);
kern_return_t mach_register_debug_print(void);
kern_return_t mach_set_breakpoint(int index, uint64_t addr);

#endif
