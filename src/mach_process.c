#include "mach_process.h"
#include <inttypes.h>
#include <mach/arm/thread_status.h>
#include <mach/exception_types.h>
#include <mach/kern_return.h>
#include <mach/mach_types.h>
#include <mach/message.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include "exception_listener.h"

#ifndef ARM_DEBUG_REG_MAX
#define ARM_DEBUG_REG_MAX 16
#endif

// global exception port and saved state
static mach_port_t exc_port = MACH_PORT_NULL;
static mach_msg_type_number_t saved_exc_count = 0;
static mach_port_t old_ports[EXC_TYPES_COUNT];
static exception_mask_t old_masks[EXC_TYPES_COUNT];
static exception_behavior_t old_behaviors[EXC_TYPES_COUNT];
static thread_state_flavor_t old_flavors[EXC_TYPES_COUNT];

// target task handle
// - the task port for the attached task
static task_t target_task;

// struct for thread arr
typedef struct {
    thread_act_port_array_t threads;
    mach_msg_type_number_t   count;
} ThreadList;

// helper: fetch all threads in the task
// used for a few funcs
static ThreadList _get_thread_list(mach_port_t task) {
    ThreadList tl = { .threads = NULL, .count = 0 };
    kern_return_t kr = task_threads(task, &tl.threads, &tl.count);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "task_threads failed: %s\n", mach_error_string(kr));
    }
    return tl;
}

// helper: get ARM_THREAD_STATE64
// print general registers
static kern_return_t _get_thread_state64(thread_act_t thread,
                                         arm_thread_state64_t *out) {
    mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
    *out = (arm_thread_state64_t){0};
    kern_return_t kr = thread_get_state(thread,
                                        ARM_THREAD_STATE64,
                                        (thread_state_t)out,
                                        &count);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr,
                "thread_get_state(thread 0x%x) failed: %s\n",
                thread,
                mach_error_string(kr));
    }
    return kr;
}

// helper: set ARM_THREAD_STATE64
// write to general registers
static kern_return_t _set_thread_state64(thread_act_t thread,
                                         const arm_thread_state64_t *in) {
    mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
    kern_return_t kr = thread_set_state(thread,
                                        ARM_THREAD_STATE64,
                                        (thread_state_t)in,
                                        count);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr,
                "thread_set_state(thread 0x%x) failed: %s\n",
                thread,
                mach_error_string(kr));
    }
    return kr;
}

// helper: get ARM_DEBUG_STATE64
// debug reg print
static kern_return_t _get_thread_debug_state64(thread_act_t thread,
                                                arm_debug_state64_t *out) {
    mach_msg_type_number_t count = ARM_DEBUG_STATE64_COUNT;
    *out = (arm_debug_state64_t){0};
    kern_return_t kr = thread_get_state(thread,
                                        ARM_DEBUG_STATE64,
                                        (thread_state_t)out,
                                        &count);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr,
                "thread_get_debug_state(thread 0x%x) failed: %s\n",
                thread,
                mach_error_string(kr));
    }
    return kr;
}

// helper: set ARM_DEBUG_STATE64
// set hw breakpoint
static kern_return_t _set_thread_debug_state64(thread_act_t thread,
                                                const arm_debug_state64_t *in) {
    mach_msg_type_number_t count = ARM_DEBUG_STATE64_COUNT;
    kern_return_t kr = thread_set_state(thread,
                                        ARM_DEBUG_STATE64,
                                        (thread_state_t)in,
                                        count);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr,
                "thread_set_debug_state(thread 0x%x) failed: %s\n",
                thread,
                mach_error_string(kr));
    }
    return kr;
}

// grab a task port for a pid
kern_return_t get_task_port(pid_t pid, task_t *task_out) {
    kern_return_t kr = task_for_pid(mach_task_self(), pid, task_out);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "[-] task_for_pid(%d) failed: %s (0x%x)\n",
                pid, mach_error_string(kr), kr);
    } else {
        printf("[+] Got task port 0x%x for PID %d\n", *task_out, pid);
    }
    return kr;
}

// Suspend and resume
kern_return_t mach_suspend(void) {
    kern_return_t kr = task_suspend(target_task);
    if (kr != KERN_SUCCESS) {
        fprintf(stderr, "[-] task_suspend failed: %s (0x%x)\n",
                mach_error_string(kr), kr);
        return kr;
    }
    printf("[+] Task [%d] suspended\n", target_task);
    return KERN_SUCCESS;
}

kern_return_t mach_resume(void) {
    return task_resume(target_task);
}

// setup exception port
kern_return_t setup_exception_port(pid_t pid) {
    kern_return_t kr = get_task_port(pid, &target_task);
    if (kr != KERN_SUCCESS) return kr;

    mach_suspend();

    exception_mask_t mask = EXC_MASK_BAD_ACCESS | EXC_MASK_BREAKPOINT |
                            EXC_MASK_BAD_INSTRUCTION | EXC_MASK_ARITHMETIC |
                            EXC_MASK_SOFTWARE | EXC_MASK_SYSCALL;

    mach_msg_type_number_t count = EXC_TYPES_COUNT;
    kr = task_get_exception_ports(target_task, mask,
                                  &old_masks[0], old_ports,
                                  &count, old_behaviors,
                                  old_flavors);
    if (kr != KERN_SUCCESS) {
        saved_exc_count = count;
        fprintf(stderr, "[-] task_get_exception_ports failed: %s (0x%x)\n",
                mach_error_string(kr), kr);
        return kr;
    }

    exc_port = MACH_PORT_NULL;
    kr = mach_port_allocate(mach_task_self(),
        MACH_PORT_RIGHT_RECEIVE,
                            &exc_port);
    if (kr != KERN_SUCCESS) return kr;

    kr = mach_port_insert_right(mach_task_self(), exc_port,
                                exc_port, MACH_MSG_TYPE_MAKE_SEND);
    if (kr != KERN_SUCCESS) return kr;

    kr = task_set_exception_ports(target_task, mask,
                                  exc_port,
                                  EXCEPTION_DEFAULT |
                                  MACH_EXCEPTION_CODES,
                                  THREAD_STATE_NONE);
    if (kr != KERN_SUCCESS) return kr;

    pthread_t thr;
    int err = pthread_create(&thr, NULL,
                              exception_listener,
                              &exc_port);
    if (err) {
        fprintf(stderr,
                "[-] Failed to create exception_listener: %s\n",
                strerror(err));
        return err;
    }
    printf("[+] exception listener thread started\n");
    pthread_detach(thr);
    return KERN_SUCCESS;
}

// detach: restore exception ports and cleanup
kern_return_t mach_detach(void) {
    for (mach_msg_type_number_t i = 0; i < saved_exc_count; ++i) {
        if (old_ports[i] != MACH_PORT_NULL) {
            task_set_exception_ports(target_task,
                                     old_masks[i],
                                     old_ports[i],
                                     old_behaviors[i],
                                     old_flavors[i]);
        }
    }
    saved_exc_count = 0;
    if (exc_port != MACH_PORT_NULL) {
        mach_port_destroy(mach_task_self(), exc_port);
        exc_port = MACH_PORT_NULL;
    }
    return KERN_SUCCESS;
}

// print the debug registers for the first thread
// CHORE: decide if we need this
kern_return_t mach_register_debug_print(void) {
    ThreadList tl = _get_thread_list(target_task);
    for (mach_msg_type_number_t i = 0; i < tl.count; ++i) {
        arm_debug_state64_t dbg;
        if (_get_thread_debug_state64(tl.threads[i], &dbg) != KERN_SUCCESS)
            continue;

        printf("=== THREAD %u DEBUG REGISTERS ===\n", i);
        for (unsigned j = 0; j < ARM_DEBUG_REG_MAX; ++j) {
            printf("BVR[%u]=0x%016llx BCR[%u]=0x%016llx\n",
                   j, dbg.__bvr[j], j, dbg.__bcr[j]);
        }
        for (unsigned j = 0; j < ARM_DEBUG_REG_MAX; ++j) {
            printf("WVR[%u]=0x%016llx WCR[%u]=0x%016llx\n",
                   j, dbg.__wvr[j], j, dbg.__wcr[j]);
        }
        printf("\n");
    }
    vm_deallocate(mach_task_self(), (vm_address_t)tl.threads,
                  tl.count * sizeof(thread_t));
    return KERN_SUCCESS;
}

// print general purpose registers for the main thread
// thanks lyla.c -- by billy ellis
kern_return_t mach_register_print(void) {
    ThreadList tl = _get_thread_list(target_task);
    arm_thread_state64_t state;
    if (_get_thread_state64(tl.threads[0], &state) != KERN_SUCCESS) {
        vm_deallocate(mach_task_self(), (vm_address_t)tl.threads,
                      tl.count * sizeof(thread_t));
        return KERN_FAILURE;
    }

    printf("\n\033[1mRegister dump (ARM64):\x1b[0m\n\n");
    for (int i = 0; i <= 28; ++i) {
        printf(" X%-2d: 0x%016" PRIx64 "%s",
               i, state.__x[i], (i % 2) ? "\n" : "    ");
    }
    printf(" FP: 0x%016" PRIx64 " LR: 0x%016" PRIx64 "\n",
           state.__fp, state.__lr);
    printf(" SP: 0x%016" PRIx64 " PC: 0x%016" PRIx64 "\n",
           state.__sp, state.__pc);
    printf(" CPSR: 0x%016" PRIx32 "\n\n", state.__cpsr);

    vm_deallocate(mach_task_self(), (vm_address_t)tl.threads,
                  tl.count * sizeof(thread_t));
    return KERN_SUCCESS;
}

// write to a specific register on the main thread
// TODO: allow a specific thread to be selected
kern_return_t mach_register_write(const char reg[], uint64_t value) {
    ThreadList tl = _get_thread_list(target_task);
    arm_thread_state64_t state;
    if (_get_thread_state64(tl.threads[0], &state) != KERN_SUCCESS) {
        vm_deallocate(mach_task_self(), (vm_address_t)tl.threads,
                      tl.count * sizeof(thread_t));
        return KERN_FAILURE;
    }

    if (strncmp(reg, "X", 1) == 0) {
        int idx = atoi(reg + 1);
        if (idx >= 0 && idx <= 28) state.__x[idx] = value;
        else if (idx == 29) state.__fp = value;
        else if (idx == 30) state.__lr = value;
        else fprintf(stderr, "Invalid X-register: %s\n", reg);
    } else if (strcmp(reg, "FP") == 0) state.__fp = value;
    else if (strcmp(reg, "LR") == 0) state.__lr = value;
    else if (strcmp(reg, "SP") == 0) state.__sp = value;
    else if (strcmp(reg, "PC") == 0) state.__pc = value;
    else fprintf(stderr, "Invalid register name: %s\n", reg);

    _set_thread_state64(tl.threads[0], &state);
    printf("Register %s set to 0x%016" PRIx64 "\n", reg, value);

    vm_deallocate(mach_task_self(), (vm_address_t)tl.threads,
                  tl.count * sizeof(thread_t));
    return KERN_SUCCESS;
}

// to set hardware breakpoint across all threads
int mach_set_breakpoint(int index, uint64_t addr) {
    ThreadList tl = _get_thread_list(target_task);
    // some weird thing got to do with privledges
    const uint64_t ENABLE          = (1ULL << 0);
    const uint64_t PRIV_USR_ONLY   = (1ULL << 3) | (1ULL << 2);
    const uint64_t MATCH_EXECUTE   = (0b00ULL << 1);
    const uint64_t SIZE_4_BYTES    = (0b11ULL << 5);
    uint64_t bcr_value = ENABLE | PRIV_USR_ONLY | MATCH_EXECUTE | SIZE_4_BYTES;

    // loop through each thread, set the hardware breakpoint
    for (mach_msg_type_number_t i = 0; i < tl.count; ++i) {
        arm_debug_state64_t dbg;
        if (_get_thread_debug_state64(tl.threads[i], &dbg) != KERN_SUCCESS)
            continue;
        dbg.__bvr[index] = addr;
        dbg.__bcr[index] = bcr_value;
        _set_thread_debug_state64(tl.threads[i], &dbg);
    }

    // safety
    vm_deallocate(mach_task_self(), (vm_address_t)tl.threads,
                  tl.count * sizeof(thread_t));

    printf("Requested breakpoint %d at 0x%016" PRIx64 " on %u threads\n",
           index, addr, tl.count);
    return KERN_SUCCESS;
}
