#include "mach/mach_process.h"
#include "exc/exception_listener.h"
#include <inttypes.h>
#include <mach-o/dyld_images.h>
#include <mach/arm/thread_status.h>
#include <mach/kern_return.h>
#include <mach/mach_types.h>
#include <mach/vm_map.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

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

// for restoring after vm_write
static vm_prot_t old_protections = 0;

// target task handle
// - the task port for the attached task
static task_t target_task;

// struct for thread arr
typedef struct {
  thread_act_port_array_t threads;
  mach_msg_type_number_t count;
} ThreadList;

// helper: fetch all threads in the task
// used for a few funcs
static ThreadList _get_thread_list(mach_port_t task) {
  ThreadList tl = {.threads = NULL, .count = 0};
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
  kern_return_t kr =
      thread_get_state(thread, ARM_THREAD_STATE64, (thread_state_t)out, &count);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "thread_get_state(thread 0x%x) failed: %s\n", thread,
            mach_error_string(kr));
  }
  return kr;
}

// helper: set ARM_THREAD_STATE64
// write to general registers
static kern_return_t _set_thread_state64(thread_act_t thread,
                                         const arm_thread_state64_t *in) {
  mach_msg_type_number_t count = ARM_THREAD_STATE64_COUNT;
  kern_return_t kr =
      thread_set_state(thread, ARM_THREAD_STATE64, (thread_state_t)in, count);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "thread_set_state(thread 0x%x) failed: %s\n", thread,
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
  kern_return_t kr =
      thread_get_state(thread, ARM_DEBUG_STATE64, (thread_state_t)out, &count);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "_get_thread_debug_state64(thread 0x%x) failed: %s\n",
            thread, mach_error_string(kr));
  }
  return kr;
}

static kern_return_t
_get_thread_exception_state64(thread_act_t thread,
                              arm_exception_state64_t *out) {

  mach_msg_type_number_t count = ARM_EXCEPTION_STATE64_COUNT;
  *out = (arm_exception_state64_t){0};
  kern_return_t kr = thread_get_state(thread, ARM_EXCEPTION_STATE64,
                                      (thread_state_t)out, &count);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "_get_thread_exception_state64(thread 0x%x) failed: %s\n",
            thread, mach_error_string(kr));
  }
  return kr;
}

// helper: set ARM_DEBUG_STATE64
// set hw breakpoint
static kern_return_t _set_thread_debug_state64(thread_act_t thread,
                                               const arm_debug_state64_t *in) {
  mach_msg_type_number_t count = ARM_DEBUG_STATE64_COUNT;
  kern_return_t kr =
      thread_set_state(thread, ARM_DEBUG_STATE64, (thread_state_t)in, count);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "thread_set_debug_state(thread 0x%x) failed: %s\n", thread,
            mach_error_string(kr));
  }
  return kr;
}

// grab a task port for a pid
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

kern_return_t mach_resume(void) { return task_resume(target_task); }

// setup exception port
kern_return_t setup_exception_port(pid_t pid) {
  kern_return_t kr = get_task_port(pid, &target_task);
  if (kr != KERN_SUCCESS)
    return kr;

  mach_suspend();

  exception_mask_t mask = EXC_MASK_BAD_ACCESS | EXC_MASK_BREAKPOINT |
                          EXC_MASK_BAD_INSTRUCTION | EXC_MASK_ARITHMETIC |
                          EXC_MASK_SOFTWARE | EXC_MASK_SYSCALL;

  mach_msg_type_number_t count = EXC_TYPES_COUNT;
  kr = task_get_exception_ports(target_task, mask, &old_masks[0], old_ports,
                                &count, old_behaviors, old_flavors);
  if (kr != KERN_SUCCESS) {
    saved_exc_count = count;
    fprintf(stderr, "[-] task_get_exception_ports failed: %s (0x%x)\n",
            mach_error_string(kr), kr);
    return kr;
  }

  exc_port = MACH_PORT_NULL;
  kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &exc_port);
  if (kr != KERN_SUCCESS)
    return kr;

  kr = mach_port_insert_right(mach_task_self(), exc_port, exc_port,
                              MACH_MSG_TYPE_MAKE_SEND);
  if (kr != KERN_SUCCESS)
    return kr;

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
  printf("[+] exception listener thread started\n");
  pthread_detach(thr);
  return KERN_SUCCESS;
}

// detach: restore exception ports and cleanup
kern_return_t mach_detach(void) {
  for (mach_msg_type_number_t i = 0; i < saved_exc_count; ++i) {
    if (old_ports[i] != MACH_PORT_NULL) {
      task_set_exception_ports(target_task, old_masks[i], old_ports[i],
                               old_behaviors[i], old_flavors[i]);
    }
  }
  saved_exc_count = 0;
  if (exc_port != MACH_PORT_NULL) {
    mach_port_destroy(mach_task_self(), exc_port);
    exc_port = MACH_PORT_NULL;
  }
  return KERN_SUCCESS;
}

kern_return_t mach_get_pc(uintptr_t *pc) {
  ThreadList tl = _get_thread_list(target_task);
  arm_thread_state64_t state64;
  if (_get_thread_state64(tl.threads[0], &state64) != KERN_SUCCESS)
    return 1;

  *pc = state64.__pc;
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
      printf("BVR[%u]=0x%016llx BCR[%u]=0x%016llx\n", j, dbg.__bvr[j], j,
             dbg.__bcr[j]);
    }
    for (unsigned j = 0; j < ARM_DEBUG_REG_MAX; ++j) {
      printf("WVR[%u]=0x%016llx WCR[%u]=0x%016llx\n", j, dbg.__wvr[j], j,
             dbg.__wcr[j]);
    }

    printf("mdscr_el1: 0x%016llx\n", dbg.__mdscr_el1);
    printf("\n");
  }
  vm_deallocate(mach_task_self(), (vm_address_t)tl.threads,
                tl.count * sizeof(thread_t));
  return KERN_SUCCESS;
}

kern_return_t mach_register_exception_print(void) {
  ThreadList tl = _get_thread_list(target_task);
  for (mach_msg_type_number_t i = 0; i < tl.count; ++i) {
    arm_exception_state64_t exc;
    if (_get_thread_exception_state64(tl.threads[i], &exc) != KERN_SUCCESS)
      continue;

    printf("=== THREAD %u EXCEPTION REGISTERS ===\n", i);
    printf("far: 0x%016llx\n", exc.__far);
    printf("esr: 0x%08" PRIx32 "\n", exc.__esr);
    printf("exception: 0x%08" PRIx32 "\n", exc.__exception);

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
    printf(" X%-2d: 0x%016" PRIx64 "%s", i, state.__x[i],
           (i % 2) ? "\n" : "    ");
  }
  printf(" FP: 0x%016" PRIx64 " LR: 0x%016" PRIx64 "\n", state.__fp,
         state.__lr);
  printf(" SP: 0x%016" PRIx64 " PC: 0x%016" PRIx64 "\n", state.__sp,
         state.__pc);
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
    if (idx >= 0 && idx <= 28)
      state.__x[idx] = value;
    else if (idx == 29)
      state.__fp = value;
    else if (idx == 30)
      state.__lr = value;
    else
      fprintf(stderr, "Invalid X-register: %s\n", reg);
  } else if (strcmp(reg, "FP") == 0)
    state.__fp = value;
  else if (strcmp(reg, "LR") == 0)
    state.__lr = value;
  else if (strcmp(reg, "SP") == 0)
    state.__sp = value;
  else if (strcmp(reg, "PC") == 0)
    state.__pc = value;
  else
    fprintf(stderr, "Invalid register name: %s\n", reg);

  _set_thread_state64(tl.threads[0], &state);
  printf("Register %s set to 0x%016" PRIx64 "\n", reg, value);

  vm_deallocate(mach_task_self(), (vm_address_t)tl.threads,
                tl.count * sizeof(thread_t));
  return KERN_SUCCESS;
}

// to set hardware breakpoint across all threads
kern_return_t mach_set_breakpoint(int index, uint64_t addr) {
  if (slide_enabled)
    addr = addr + slide;

  ThreadList tl = _get_thread_list(target_task);

  if (tl.count == 0 || tl.threads == NULL)
    return KERN_FAILURE;

  bool did_step = false;

  // some weird thing got to do with privledges
  const uint64_t ENABLE = (1ULL << 0);
  const uint64_t PRIV_USR_ONLY = (1ULL << 3) | (1ULL << 2);
  const uint64_t MATCH_EXECUTE = (0b00ULL << 1);
  const uint64_t SIZE_4_BYTES = (0b11ULL << 5);
  uint64_t bcr_value = ENABLE | PRIV_USR_ONLY | MATCH_EXECUTE | SIZE_4_BYTES;

  // loop through each thread, set the hardware breakpoint
  for (mach_msg_type_number_t i = 0; i < tl.count; ++i) {
    arm_debug_state64_t dbg;
    if (_get_thread_debug_state64(tl.threads[i], &dbg) != KERN_SUCCESS)
      continue;
    dbg.__bvr[index] = addr;
    dbg.__bcr[index] = bcr_value;
    if (_set_thread_debug_state64(tl.threads[i], &dbg) == KERN_SUCCESS) {
      did_step = true;
    }
  }

  // safety
  vm_deallocate(mach_task_self(), (vm_address_t)tl.threads,
                tl.count * sizeof(thread_t));

  printf("Requested breakpoint %d at 0x%016" PRIx64 " on %u threads\n", index,
         addr, tl.count);

  return did_step ? KERN_SUCCESS : KERN_FAILURE;
}

kern_return_t mach_step(void) {
  ThreadList tl = _get_thread_list(target_task);

  if (tl.count == 0 || tl.threads == NULL) {
    return KERN_FAILURE;
  }

  bool did_step = false;

  for (mach_msg_type_number_t i = 0; i < tl.count; i++) {
    arm_debug_state64_t dbg;

    if (_get_thread_debug_state64(tl.threads[i], &dbg) != KERN_SUCCESS)
      continue;
    // set single step bit to 1
    dbg.__mdscr_el1 |= (1ULL << 0);

    if (_set_thread_debug_state64(tl.threads[i], &dbg) == KERN_SUCCESS) {
      did_step = true;
    }
  }

  vm_deallocate(mach_task_self(), (vm_address_t)tl.threads,
                tl.count * sizeof(thread_t));

  return did_step ? KERN_SUCCESS : KERN_FAILURE;
}

kern_return_t mach_remove_breakpoint(int idx) {
  ThreadList tl = _get_thread_list(target_task);

  if (tl.count == 0 || tl.threads == NULL)
    return KERN_FAILURE;

  bool did_step = false;

  for (mach_msg_type_number_t i = 0; i < tl.count; i++) {
    arm_debug_state64_t dbg;
    if (_get_thread_debug_state64(tl.threads[i], &dbg) == KERN_SUCCESS)
      continue;
    dbg.__bvr[idx] = 0x0000000000000000ULL;
    dbg.__bcr[idx] = 0x0000000000000000ULL;

    if (_set_thread_debug_state64(tl.threads[i], &dbg) == KERN_SUCCESS) {
      did_step = true;
    }
  }

  vm_deallocate(mach_task_self(), (vm_address_t)tl.threads,
                tl.count * sizeof(thread_t));

  return did_step ? KERN_SUCCESS : KERN_FAILURE;
}

// read helper
kern_return_t mach_read(uintptr_t addr, void *out, size_t size, bool aslr) {
  if (slide & aslr)
    addr = addr + slide;

  if (out == NULL) {
    return KERN_INVALID_ARGUMENT;
  }

  vm_size_t bytes_read = 0;

  kern_return_t kr =
      vm_read_overwrite(target_task, (vm_address_t)addr, (vm_size_t)size,
                        (vm_address_t)out, &bytes_read);

  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "mach_read: vm_read_overwrite failed: %s\n",
            mach_error_string(kr));
  }

  if (bytes_read != size) {
    fprintf(stderr, "vm_read_overwrite read only %zu bytes instead of %zu\n",
            (size_t)bytes_read, size);
    return KERN_FAILURE;
  }

  return KERN_SUCCESS;
}

typedef struct {
  uintptr_t aligned_addr;
  size_t aligned_size;
} Page;

static Page _get_aligned_page(mach_vm_address_t addr, size_t size) {
  Page p;

  uintptr_t page_size = getpagesize();
  p.aligned_addr = ((uintptr_t)addr) & ~(page_size - 1);
  p.aligned_size = ((size + page_size - 1) / page_size) * page_size;

  return p;
}

static void _print_protections(vm_prot_t protections) {
  fprintf(stderr, "Protections: ");
  if (protections & VM_PROT_READ) {
    fprintf(stderr, "r");
  } else {
    fprintf(stderr, "-");
  }
  if (protections & VM_PROT_WRITE) {
    fprintf(stderr, "w");
  } else {
    fprintf(stderr, "-");
  }
  if (protections & VM_PROT_EXECUTE) {
    fprintf(stderr, "x");
  } else {
    fprintf(stderr, "-");
  }
  fprintf(stderr, "\n");
}

static kern_return_t _mach_get_region_protections(mach_vm_address_t addr,
                                                  vm_prot_t *protections) {
  kern_return_t kr;
  mach_vm_size_t region_size;
  mach_vm_address_t region_addr = addr;
  vm_region_flavor_t flavor = VM_REGION_BASIC_INFO_64;
  vm_region_basic_info_data_64_t info;
  mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
  mach_port_t object_name;

  kr = vm_region_64(target_task, (vm_address_t *)&region_addr,
                    (vm_address_t *)&region_size, flavor,
                    (vm_region_info_t)&info, &info_count, &object_name);

  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "_mach_get_region_protections: vm_region: failed: %s\n",
            mach_error_string(kr));
    return KERN_FAILURE;
  }

  *protections = info.protection;

  if (object_name != MACH_PORT_NULL) {
    mach_port_deallocate(mach_task_self(), object_name);
  }

  return KERN_SUCCESS;
}

// this is needed as we cannot just write anywhere in memory, for example
// in the future if we want to add software breakpoints, we need read and write
// access to the __TEXT segment, the __TEXT segment in a MachO binary holds
// instructions that the cpu can execute, the protections for this reason
// as i believe currently, are set to r/x, we cannot write to this segment
// as we will get an KERN_INVALID_ADDRESS if we call vm_write in this segment
//
// the steps are as follow
// - see the protections of the page we are writing to
// _mach_get_region_protections->vm_region
// - if current_protections == new_protections throw a KERN_SUCCESS obv ..
// - we cant just protect individual bytes or arbitrary ranges in memory, we
// need to protect entire pages
//   so using the address we want to write to, we find the base address of the
//   page this piece of memory is in
// - then we do a vm_protect with our new_protections
static kern_return_t _mach_set_region_protections(mach_vm_address_t addr,
                                                  mach_vm_size_t size,
                                                  vm_prot_t new_protections) {

  kern_return_t kr;
  vm_prot_t current_protections;
  kr = _mach_get_region_protections(addr, &current_protections);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr,
            "_mach_set_region_proections: _mach_get_region_protections: "
            "failed: %s\n",
            mach_error_string(kr));
    return KERN_FAILURE;
  }

  old_protections = current_protections;

  if (current_protections == new_protections) {
    return KERN_SUCCESS;
  }

  Page p = _get_aligned_page(addr, size);

  fprintf(stderr, "[i] Region 0x%lx - 0x%lx (size: 0x%lx) has ", p.aligned_addr,
          p.aligned_addr + p.aligned_size, p.aligned_size);
  _print_protections(new_protections);

  kr = vm_protect(target_task, p.aligned_addr, p.aligned_size, FALSE,
                  new_protections);

  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "_mach_set_region_protections: vm_protect failed: %s\n",
            mach_error_string(kr));
    return KERN_FAILURE;
  }

  return KERN_SUCCESS;
}

static kern_return_t _mach_set_region_writeable(mach_vm_address_t addr,
                                                mach_vm_size_t size) {
  kern_return_t kr = _mach_set_region_protections(
      addr, size, VM_PROT_WRITE | VM_PROT_READ | VM_PROT_COPY);

  if (kr != KERN_SUCCESS) {
    fprintf(
        stderr,
        "mach_set_region_writeable: _mach_set_region_protections failed: %s\n",
        mach_error_string(kr));
    return KERN_FAILURE;
  }

  return KERN_SUCCESS;
}

static kern_return_t _mach_restore_region(mach_vm_address_t addr,
                                          mach_vm_size_t size) {
  kern_return_t kr = _mach_set_region_protections(addr, size, old_protections);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr,
            "mach_restore_region: _mach_set_region_protections failed: %s\n",
            mach_error_string(kr));
    return KERN_FAILURE;
  }

  return KERN_SUCCESS;
}

// write helper
kern_return_t mach_write(uintptr_t addr, void *bytes, size_t size) {
  kern_return_t kr;

  if (slide)
    addr = addr + slide;

  kr = _mach_set_region_writeable(addr, size);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "mach_write: _mach_set_region_writeable failed: %s\n",
            mach_error_string(kr));
    return kr;
  }

  kr = vm_write(target_task, (vm_address_t)addr, (vm_offset_t)bytes, size);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "mach_write: vm_write failed: %s\n", mach_error_string(kr));
    return kr;
  }

  kr = _mach_restore_region(addr, size);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "mach_write: _mach_restore_region failed: %s\n",
            mach_error_string(kr));
    return kr;
  }

  return KERN_SUCCESS;
}

kern_return_t mach_read64(uintptr_t addr, uint64_t *out) {
  return mach_read(addr, out, sizeof(*out), true);
}

kern_return_t mach_read32(uintptr_t addr, uint32_t *out) {
  return mach_read(addr, out, sizeof(*out), true);
}

kern_return_t mach_write64(uintptr_t addr, uint64_t bytes) {
  kern_return_t kr = mach_write(addr, (uint64_t *)bytes, sizeof(uint64_t));

  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "mach_write64 failed: %s\n", mach_error_string(kr));
  }

  return KERN_SUCCESS;
}

kern_return_t mach_write32(uintptr_t addr, uint32_t bytes) {
  kern_return_t kr = mach_write(addr, &bytes, sizeof(uint32_t));

  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "mach_write32 failed: %s\n", mach_error_string(kr));
  }

  return KERN_SUCCESS;
}

// aslr stuff
kern_return_t mach_get_aslr_slide(mach_vm_address_t *out_slide) {
  task_dyld_info_data_t dyld_info;
  mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;

  kern_return_t kr =
      task_info(target_task, TASK_DYLD_INFO, (task_info_t)&dyld_info, &count);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "mach_get_aslr_slide: task_info failed: %s\n",
            mach_error_string(kr));
    return kr;
  }

  // read the dyld_all_image_infos structure from target process
  struct dyld_all_image_infos image_infos;
  mach_vm_size_t size = sizeof(image_infos);

  kr = vm_read_overwrite(target_task, dyld_info.all_image_info_addr, size,
                         (mach_vm_address_t)&image_infos, (vm_size_t *)&size);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "mach_get_aslr_slide: mach_vm_read failed: %s\n",
            mach_error_string(kr));
    return kr;
  }

  if (image_infos.infoArrayCount > 0) {
    // find make excutable
    struct dyld_image_info main_image;
    mach_vm_size_t size = sizeof(main_image);

    kr = vm_read_overwrite(
        target_task, (mach_vm_address_t)image_infos.infoArray, size,
        (mach_vm_address_t)&main_image, (vm_address_t *)&size);
    if (kr == KERN_SUCCESS) {
      *out_slide =
          (mach_vm_address_t)main_image.imageLoadAddress - 0x100000000ULL;
    }
  }

  return KERN_SUCCESS;
}

kern_return_t mach_set_auto_slide_enabled(bool enabled) {
  if (!enabled) {
    // disable auto aslr slide
    slide = (mach_vm_address_t)0;
    slide_enabled = enabled;
    return KERN_SUCCESS;
  }

  kern_return_t kr = mach_get_aslr_slide(&slide);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr,
            "mach_set_auto_slide_enabled: mach_get_aslr_slide: failed: %s\n",
            mach_error_string(kr));
    return KERN_FAILURE;
  }

  slide_enabled = enabled;

  return KERN_SUCCESS;
}

kern_return_t mach_set_slide_value(mach_vm_address_t new_slide) {
  slide = new_slide;
  if (!slide_enabled)
    slide_enabled = !slide_enabled;
  return KERN_SUCCESS;
}
