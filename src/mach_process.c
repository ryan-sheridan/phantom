#include "mach_process.h"
#include <inttypes.h>
#include <mach/arm/thread_status.h>
#include <mach/exception_types.h>
#include <mach/kern_return.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef ARM_DEBUG_REG_MAX
#define ARM_DEBUG_REG_MAX 16
#endif

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
    fprintf(stderr, "[-] task_for_pid(%d) failed: %s (0x%x)\n", pid,
            mach_error_string(kr), kr);
  } else {
    printf("[+] Got task port 0x%x for PID %d\n", *task_out, pid);
  }
  return kr;
}

kern_return_t mach_suspend() {
  kern_return_t kr = task_suspend(target_task);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "[-] task_suspend failed: %s (0x%x)\n",
            mach_error_string(kr), kr);
    return 1;
  }
  printf("[+] Task [%d] interrupted (mach suspend)\n", target_task);

  return kr;
}

kern_return_t setup_exception_port(pid_t pid) {
  // grab task port for pid
  kern_return_t kr = get_task_port(pid, &target_task);
  if (kr != KERN_SUCCESS) {
    return kr;
  }

  mach_port_t task_self = mach_task_self();

  // suspend task before setting up exception ports
  mach_suspend();

  exception_mask_t mask = EXC_MASK_BAD_ACCESS | EXC_MASK_BREAKPOINT |
                          EXC_MASK_BAD_INSTRUCTION | EXC_MASK_ARITHMETIC |
                          EXC_MASK_SOFTWARE | EXC_MASK_SYSCALL;

  // capture old exception ports
  exc_count = EXC_TYPES_COUNT;
  kr = task_get_exception_ports(target_task, mask, &old_masks[0], old_ports,
                                &exc_count, &old_behaviors[0], &old_flavors[0]);
  if (kr != KERN_SUCCESS) {
    saved_exc_count = exc_count;
    fprintf(stderr, "[-] task_get_exception_ports failed: %s (0x%x)\n",
            mach_error_string(kr), kr);
    return kr;
  }

  // allocate a recieve right
  kr = mach_port_allocate(task_self, MACH_PORT_RIGHT_RECEIVE, &exc_port);
  if (kr != KERN_SUCCESS)
    return kr;

  // insert a send right so we can reply
  kr = mach_port_insert_right(task_self, exc_port, exc_port,
                              MACH_MSG_TYPE_MAKE_SEND);
  if (kr != KERN_SUCCESS)
    return kr;

  // register for all exceptions
  kr = task_set_exception_ports(target_task, mask, exc_port,
                                EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES,
                                THREAD_STATE_NONE);

  if (kr != KERN_SUCCESS)
    return kr;

  pthread_t thr;
  int err = pthread_create(&thr, NULL, exception_listener, &exc_port);

  if (err) {
    fprintf(stderr, "[-] Failed to create exception_listener: %s\n",
            strerror(err));
    return err;
  }

  pthread_detach(thr);
  return kr;
}

// think about this, think about it hard
kern_return_t mach_resume() { return task_resume(target_task); }

kern_return_t mach_detach() {
  kern_return_t kr;
  for (mach_msg_type_number_t i = 0; i < saved_exc_count; i++) {
    mach_port_t port = old_ports[i];
    if (port == MACH_PORT_NULL)
      continue;

    kr = task_set_exception_ports(target_task, old_masks[i], port,
                                  old_behaviors[i], old_flavors[i]);

    if (kr != KERN_SUCCESS) {
      fprintf(stderr, "[-] Error restoring exception port [%u]: %s\n", i,
              mach_error_string(kr));
    }
  }

  saved_exc_count = 0;

  // deallocate receive right for the allocated debugger port
  if (exc_port != MACH_PORT_NULL) {
    kr = mach_port_destroy(mach_task_self(), exc_port);
    if (kr != KERN_SUCCESS) {
      fprintf(stderr, "[-] Error destroying debugger port: %s\n",
              mach_error_string(kr));
    }
    exc_port = MACH_PORT_NULL;
  }
  return KERN_SUCCESS;
}

kern_return_t mach_register_debug_print() {
  thread_act_port_array_t thread_list;
  mach_msg_type_number_t thread_count;
  kern_return_t kr;

  // get threads in task
  kr = task_threads(target_task, &thread_list, &thread_count);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "task_threads failed: %s\n", mach_error_string(kr));
    return kr;
  }

  // grab the first thread 64-bit state
  arm_debug_state64_t dbg;
  mach_msg_type_number_t state_count = ARM_DEBUG_STATE64_COUNT;

  kr = thread_get_state(thread_list[0], ARM_DEBUG_STATE64, (thread_state_t)&dbg,
                        &state_count);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "thread_get_state failed: %s\n", mach_error_string(kr));
    // clean up
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                  thread_count * sizeof(thread_t));
    return kr;
  }

  // 3) Print all breakpoint value (BVR) and control (BCR) registers
  printf("=== ARM64 Breakpoint Registers ===\n");
  for (unsigned i = 0; i < ARM_DEBUG_REG_MAX; i++) {
    printf("BVR[%u] = 0x%016llx    BCR[%u] = 0x%016llx\n", i, dbg.__bvr[i], i,
           dbg.__bcr[i]);
  }

  // 4) Print all watchpoint value (WVR) and control (WCR) registers
  printf("\n=== ARM64 Watchpoint Registers ===\n");
  for (unsigned i = 0; i < ARM_DEBUG_REG_MAX; i++) {
    printf("WVR[%u] = 0x%016llx    WCR[%u] = 0x%016llx\n", i, dbg.__wvr[i], i,
           dbg.__wcr[i]);
  }

  // 5) Clean up the thread list
  vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                thread_count * sizeof(thread_t));

  return KERN_SUCCESS;
}

kern_return_t mach_register_print() {
  thread_act_port_array_t thread_list;
  mach_msg_type_number_t thread_count;
  kern_return_t kr;

  // get threads in task
  kr = task_threads(target_task, &thread_list, &thread_count);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "task_threads failed: %s\n", mach_error_string(kr));
    return kr;
  }

  // grab the first thread 64-bit state
  arm_thread_state64_t arm64;
  mach_msg_type_number_t state_count = ARM_THREAD_STATE64_COUNT;

  kr = thread_get_state(thread_list[0], ARM_THREAD_STATE64,
                        (thread_state_t)&arm64, &state_count);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "thread_get_state failed: %s\n", mach_error_string(kr));
    // clean up
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                  thread_count * sizeof(thread_t));
    return kr;
  }

  // print out general-purpose registers x0–x28
  printf("\n\033[1mRegister dump (ARM64):\x1b[0m\n\n");
  for (int i = 0; i <= 28; i++) {
    printf("    \033[1mX%-2d:\x1b[0m 0x%016" PRIx64 "%s", i, arm64.__x[i],
           (i % 2 == 1) ? "\n" : "    ");
  }
  // FP (x29) and LR (x30)
  printf("    \033[1mFP:\x1b[0m 0x%016" PRIx64
         "    \033[1mLR:\x1b[0m 0x%016" PRIx64 "\n",
         arm64.__fp, arm64.__lr);
  // SP, PC, CPSR
  printf("    \033[1mSP:\x1b[0m 0x%016" PRIx64
         "    \033[1mPC:\x1b[0m 0x%016" PRIx64 "\n",
         arm64.__sp, arm64.__pc);
  printf("    \033[1mCPSR:\x1b[0m 0x%016" PRIx32 "\n\n", arm64.__cpsr);

  // clean up
  vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                thread_count * sizeof(thread_t));

  return kr;
}

kern_return_t mach_register_write(const char reg[], uint64_t value) {
  thread_act_port_array_t thread_list;
  mach_msg_type_number_t thread_count;
  kern_return_t kr;

  // 1. get threads in task
  kr = task_threads(target_task, &thread_list, &thread_count);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "task_threads failed: %s\n", mach_error_string(kr));
    return kr;
  }

  // 2. fetch the 64-bit register state of the first thread
  arm_thread_state64_t state64;
  mach_msg_type_number_t count64 = ARM_THREAD_STATE64_COUNT;
  kr = thread_get_state(thread_list[0], ARM_THREAD_STATE64,
                        (thread_state_t)&state64, &count64);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "thread_get_state failed: %s\n", mach_error_string(kr));
    vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                  thread_count * sizeof(thread_t));
    return kr;
  }

  // 3. Update the requested register
  if (strncmp(reg, "X", 1) == 0) {
    // X0..X28 or X29/X30
    int idx = atoi(reg + 1);
    if (idx >= 0 && idx <= 28) {
      state64.__x[idx] = value;
    } else if (idx == 29) {
      state64.__fp = value;
    } else if (idx == 30) {
      state64.__lr = value;
    } else {
      fprintf(stderr, "[!] Invalid X-register: %s\n", reg);
    }

  } else if (strcmp(reg, "FP") == 0) {
    state64.__fp = value;
  } else if (strcmp(reg, "LR") == 0) {
    state64.__lr = value;
  } else if (strcmp(reg, "SP") == 0) {
    state64.__sp = value;
  } else if (strcmp(reg, "PC") == 0) {
    state64.__pc = value;
  } else {
    fprintf(stderr, "[!] Invalid register name: %s\n", reg);
  }

  // 4. write back the modified state
  kr = thread_set_state(thread_list[0], ARM_THREAD_STATE64,
                        (thread_state_t)&state64, count64);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "thread_set_state failed: %s\n", mach_error_string(kr));
  } else {
    printf("Register %s set to 0x%016" PRIx64 "\n", reg, value);
  }

  // 5. clean up
  vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                thread_count * sizeof(thread_t));
  return kr;
}

int mach_set_breakpoint(int index, uint64_t addr) {
  thread_act_port_array_t thread_list;
  mach_msg_type_number_t thread_count;
  kern_return_t kr;

  // get all threads in target task
  kr = task_threads(target_task, &thread_list, &thread_count);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "task_threads failed: %s\n", mach_error_string(kr));
    return kr;
  }

  // something i read online
  const uint64_t ENABLE = (1ULL << 0);
  const uint64_t PRIV_USR_ONLY = (1ULL << 3) | (1ULL << 2);
  const uint64_t MATCH_EXECUTE = (0b00ULL << 1);
  const uint64_t SIZE_4_BYTES = (0b11ULL << 5);
  uint64_t bcr_value = ENABLE | PRIV_USR_ONLY | MATCH_EXECUTE | SIZE_4_BYTES;

  // iterate over every thread once
  for (mach_msg_type_number_t i = 0; i < thread_count; i++) {
    arm_debug_state64_t dbg;
    mach_msg_type_number_t state_count = ARM_DEBUG_STATE64_COUNT;

    // fetch debug state
    kr = thread_get_state(thread_list[i], ARM_DEBUG_STATE64,
                          (thread_state_t)&dbg, &state_count);
    if (kr != KERN_SUCCESS) {
      fprintf(stderr, "thread_get_state[%u] failed: %s\n", i,
              mach_error_string(kr));
      continue;
    }

    // program breakpoint
    dbg.__bvr[index] = addr;
    dbg.__bcr[index] = bcr_value;

    // write that state back
    kr = thread_set_state(thread_list[i], ARM_DEBUG_STATE64,
                          (thread_state_t)&dbg, state_count);
    if (kr != KERN_SUCCESS) {
      fprintf(stderr, "thread_set_state[%u] failed: %s\n", i,
              mach_error_string(kr));
    }
  }

  // clean shit up
  vm_deallocate(mach_task_self(), (vm_address_t)thread_list,
                thread_count * sizeof(thread_t));

  printf("Requested breakpoint %d at 0x%016" PRIx64 " on %u threads\n", index,
         addr, thread_count);
  return KERN_SUCCESS;
}
