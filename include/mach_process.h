#ifndef MACH_PROCESS_H
#define MACH_PROCESS_H

#include <mach/arm/thread_status.h>
#include <mach/exc.h>
#include <mach/exception_types.h>
#include <mach/kern_return.h>
#include <mach/mach.h>
#include <mach/mach_types.h>
#include <mach/message.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

kern_return_t get_task_port(pid_t pid, task_t *task_out);
kern_return_t setup_exception_port(pid_t pid);

kern_return_t mach_resume(void);
kern_return_t mach_suspend(void);
kern_return_t mach_detach(void);
kern_return_t mach_register_print(void);
kern_return_t mach_register_write(const char reg[], uint64_t value);
kern_return_t mach_register_debug_print(void);
kern_return_t mach_set_breakpoint(int index, uint64_t addr);

// vmrw
kern_return_t mach_read64(uintptr_t addr, uint64_t *out);
kern_return_t mach_write64(uint64_t addr, uint64_t val);
kern_return_t mach_read32(uint64_t addr, uint32_t *out);
kern_return_t mach_write32(uint64_t addr, uint32_t val);

#endif
