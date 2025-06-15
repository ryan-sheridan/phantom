#include "debugger.h"
#include "mach_vm_helper.h"
#include <mach/kern_return.h>

pid_t attached_pid = 0;

int attach(pid_t pid) {
  kern_return_t kr = setup_exception_port(pid);
  if (kr != KERN_SUCCESS) {
      fprintf(stderr,
              "[-] setup_exception_port failed: %s (0x%x)\n",
              mach_error_string(kr),
              kr);
      return 1;
  }
  printf("[+] Exception port setup configured successfully for: %d\n", pid);


  attached_pid = pid;
  return pid;
}

int resume(void) {
  kern_return_t kr = mach_resume();
  if(kr != KERN_SUCCESS) {
    fprintf(stderr,
              "[-] task_resume failed: %s (0x%x)\n",
              mach_error_string(kr),
              kr);
  }

  printf("[+] task resumed successfully\n");
  return 0;
}

int interrupt(void) {
  kern_return_t kr = mach_suspend();
  if(kr != KERN_SUCCESS) {
    fprintf(stderr, "[-] task_suspend failed: %s (0x%x)\n",
          mach_error_string(kr), kr);
    return 1;
  }
  return 0;
}

int detach(void) {
  kern_return_t kr = mach_detach();
  if(kr != KERN_SUCCESS) {
    fprintf(stderr,
              "[-] mach_detach failed: %s (0x%x)\n",
              mach_error_string(kr),
              kr);
  } else {
    printf("[+] mach exception port torn down\n");
  }

  printf("[+] detached from %d\n", attached_pid);
  return 0;
}
