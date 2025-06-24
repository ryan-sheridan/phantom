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
#include <stdbool.h>

// very important
kern_return_t get_task_port(pid_t pid, task_t *task_out);
// important for catching exceptions on the target_task
kern_return_t setup_exception_port(pid_t pid);

// general purpose functionality on mach task
kern_return_t mach_resume(void);
kern_return_t mach_suspend(void);
kern_return_t mach_detach(void);
kern_return_t mach_register_print(void);
kern_return_t mach_register_write(const char reg[], uint64_t value);
kern_return_t mach_register_debug_print(void);
kern_return_t mach_set_breakpoint(int index, uint64_t addr);
kern_return_t mach_remove_breakpoint(int idx);
kern_return_t mach_step(void);
kern_return_t mach_register_exception_print(void);

// vmrw - reads or writes 64 or 32 bytes to whatever address we want
// TODO: test r/w on __TEXT segment, iirc this errors out, but can be fixed with permissions?
kern_return_t mach_read64(uintptr_t addr, uint64_t *out);
kern_return_t mach_read32(uintptr_t addr, uint32_t *out);

kern_return_t mach_write64(uintptr_t addr, uint64_t bytes);
kern_return_t mach_write32(uintptr_t addr, uint32_t bytes);

// aslr
extern mach_vm_address_t slide;
extern bool slide_enabled;

// this should auto slide, every read and write should be to addr + slide
kern_return_t mach_set_auto_slide_enabled(bool enabled);
// we can also set the slide to whatever value we want
kern_return_t mach_set_slide_value(mach_vm_address_t slide);
// returns the aslr slide into &out_slide
kern_return_t mach_get_aslr_slide(mach_vm_address_t *out_slide);

#endif
