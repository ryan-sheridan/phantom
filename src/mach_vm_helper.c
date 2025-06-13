#include "mach_vm_helper.h"

kern_return_t get_task_port(pid_t pid, task_t *task_out) {
    kern_return_t kr = task_for_pid(mach_task_self(), pid, task_out);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr,
                "task_for_pid(%d) failed: %s (0x%x)\n",
                pid,
                mach_error_string(kr),
                kr);
    } else {
        printf("Got task port 0x%x for PID %d\n", *task_out, pid);
    }
    return kr;
}

static task_t target_task;
static mach_port_t exc_port;

kern_return_t setup_exception_port(pid_t pid) {
    // grab task port for pid
    task_t target_task;
    kern_return_t kr = get_task_port(pid, &target_task);
    if (kr != KERN_SUCCESS) {
        return kr;
    }

    // allocate a recieve right
    kr = mach_port_allocate(mach_task_self(),
                            MACH_PORT_RIGHT_RECEIVE,
                            &exc_port);
    if(kr != KERN_SUCCESS) return kr;

    // insert a send right so we can reply
    kr = mach_port_insert_right(mach_task_self(),
                                exc_port,
                                exc_port,
                                MACH_MSG_TYPE_MAKE_SEND);
    if(kr != KERN_SUCCESS) return kr;

    // register for all exceptions
    exception_mask_t mask = EXC_MASK_BAD_ACCESS
                          | EXC_MASK_BREAKPOINT
                          | EXC_MASK_BAD_INSTRUCTION
                          | EXC_MASK_ARITHMETIC
                          | EXC_MASK_SOFTWARE
                          | EXC_MASK_SYSCALL;
    kr = task_set_exception_ports(target_task,
                                  mask,
                                  exc_port,
                                  EXCEPTION_DEFAULT,
                                  THREAD_STATE_NONE);

    return kr;
}
