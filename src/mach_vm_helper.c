#include "mach_vm_helper.h"
#include <mach/kern_return.h>
#include <pthread.h>

extern void *exception_listener(void *arg);

// global exception port we will allocate
static mach_port_t exc_port = MACH_PORT_NULL;
static mach_msg_type_number_t saved_exc_count = 0;

// arrays to store previous port settings
mach_port_t old_ports[EXC_TYPES_COUNT];
exception_mask_t old_masks[EXC_TYPES_COUNT];
exception_behavior_t old_behaviors[EXC_TYPES_COUNT];
thread_state_flavor_t old_flavors[EXC_TYPES_COUNT];
mach_msg_type_number_t exc_count;

task_t target_task;

kern_return_t get_task_port(pid_t pid, task_t *task_out) {
    kern_return_t kr = task_for_pid(mach_task_self(), pid, task_out);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr,
                "[-] task_for_pid(%d) failed: %s (0x%x)\n",
                pid,
                mach_error_string(kr),
                kr);
    } else {
        printf("[+] Got task port 0x%x for PID %d\n", *task_out, pid);
    }
    return kr;
}

kern_return_t setup_exception_port(pid_t pid) {
    // grab task port for pid
    kern_return_t kr = get_task_port(pid, &target_task);
    if (kr != KERN_SUCCESS) {
        return kr;
    }

    exception_mask_t mask = EXC_MASK_BAD_ACCESS
                          | EXC_MASK_BREAKPOINT
                          | EXC_MASK_BAD_INSTRUCTION
                          | EXC_MASK_ARITHMETIC
                          | EXC_MASK_SOFTWARE
                          | EXC_MASK_SYSCALL;

    // capture old exception ports
    exc_count = EXC_TYPES_COUNT;
    kr = task_get_exception_ports(
        target_task,
        mask,
        &old_masks[0],
        old_ports,
        &exc_count,
        &old_behaviors[0],
        &old_flavors[0]
    );
    if(kr != KERN_SUCCESS) {
        saved_exc_count = exc_count;
        fprintf(stderr, "[-] task_get_exception_ports failed: %s (0x%x)\n",
                mach_error_string(kr), kr);
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
    kr = task_set_exception_ports(target_task,
                                  mask,
                                  exc_port,
                                  EXCEPTION_DEFAULT,
                                  THREAD_STATE_NONE);

    if(kr != KERN_SUCCESS) return kr;

    pthread_t thr;
    int err = pthread_create(&thr, NULL, exception_listener, &exc_port);

    if (err) {
      fprintf(stderr, "[-] Failed to create exception_listener: %s\n", strerror(err));
      return err;
    }

    pthread_detach(thr);
    return kr;
}

kern_return_t mach_resume() {
  return task_resume(target_task);
}

kern_return_t mach_detach() {
  kern_return_t kr;
  for(mach_msg_type_number_t i = 0; i < saved_exc_count; i++) {
    mach_port_t port = old_ports[i];
    if(port == MACH_PORT_NULL) continue;

    kr = task_set_exception_ports(
        target_task,
        old_masks[i],
        port,
        old_behaviors[i],
        old_flavors[i]
    );

    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "[-] Error restoring exception port [%u]: %s\n",
                i, mach_error_string(kr));
    }
  }

  saved_exc_count = 0;

  // deallocate receive right for the allocated debugger port
  if(exc_port != MACH_PORT_NULL) {
    kr = mach_port_destroy(mach_task_self(), exc_port);
    if (kr != KERN_SUCCESS) {
          fprintf(stderr, "[-] Error destroying debugger port: %s\n",
                  mach_error_string(kr));
      }
      exc_port = MACH_PORT_NULL;
  }
  return KERN_SUCCESS;
}
