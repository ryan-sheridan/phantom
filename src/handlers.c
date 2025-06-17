#include "mach_exc.h"
#include "mach_process.h"
#include "shell.h"
#include <_stdio.h>
#include <mach/exc.h>
#include <mach/exception_types.h>
#include <mach/mach_error.h>
#include <stdio.h>

const char *exception_name(exception_type_t type) {
  switch (type) {
  case EXC_BAD_ACCESS:
    return "EXC_BAD_ACCESS"; // 1
  case EXC_BAD_INSTRUCTION:
    return "EXC_BAD_INSTRUCTION"; // 2
  case EXC_ARITHMETIC:
    return "EXC_ARITHMETIC"; // 3
  case EXC_EMULATION:
    return "EXC_EMULATION"; // 4
  case EXC_SOFTWARE:
    return "EXC_SOFTWARE"; // 5
  case EXC_BREAKPOINT:
    return "EXC_BREAKPOINT"; // 6
  case EXC_SYSCALL:
    return "EXC_SYSCALL"; // 7
  case EXC_MACH_SYSCALL:
    return "EXC_MACH_SYSCALL"; // 8
  case EXC_RPC_ALERT:
    return "EXC_RPC_ALERT"; // 9
  case EXC_CRASH:
    return "EXC_CRASH"; // 10
  case EXC_RESOURCE:
    return "EXC_RESOURCE"; // 11
  case EXC_GUARD:
    return "EXC_GUARD"; // 12
  case EXC_CORPSE_NOTIFY:
    return "EXC_CORPSE_NOTIFY"; // 13
  default:
    return "UNKNOWN_EXCEPTION";
  }
}

kern_return_t catch_mach_exception_raise(mach_port_t exception_port,
                                         mach_port_t thread, mach_port_t task,
                                         exception_type_t exception,
                                         mach_exception_data_t code,
                                         mach_msg_type_number_t codeCnt) {
  (void)exception_port;
  (void)task;
  (void)codeCnt;

  printf("\n[!] Caught exception %s on thread 0x%x, code=0x%llx\n",
         exception_name(exception), thread, code[0]);
  mach_suspend();
  print_prompt();
  return KERN_SUCCESS;
}

kern_return_t catch_mach_exception_raise_state(
    mach_port_t exception_port, exception_type_t exception,
    const mach_exception_data_t code, mach_msg_type_number_t codeCnt,
    int *flavor, const thread_state_t old_state,
    mach_msg_type_number_t old_stateCnt, thread_state_t new_state,
    mach_msg_type_number_t *new_stateCnt) {

  (void)exception_port;
  (void)exception;
  (void)code;
  (void)codeCnt;
  (void)flavor;
  (void)old_state;
  (void)old_stateCnt;
  (void)new_state;
  (void)new_stateCnt;

  printf("testing raise state\n");
  return KERN_FAILURE;
}

kern_return_t catch_mach_exception_raise_state_identity(
    mach_port_t exception_port, mach_port_t thread, mach_port_t task,
    exception_type_t exception, mach_exception_data_t code,
    mach_msg_type_number_t codeCnt, int *flavor, thread_state_t old_state,
    mach_msg_type_number_t old_stateCnt, thread_state_t new_state,
    mach_msg_type_number_t *new_stateCnt) {

  (void)exception_port;
  (void)thread;
  (void)task;
  (void)exception;
  (void)code;
  (void)codeCnt;
  (void)flavor;
  (void)old_state;
  (void)old_stateCnt;
  (void)new_state;
  (void)new_stateCnt;

  printf("testing raise state identity\n");
  return KERN_FAILURE;
}
