#include "mach_vm_helper.h"

mach_port_t return_tp(pid_t pid) {
  mach_port_t task_port;
  kern_return_t kr;

  kr = task_for_pid(mach_task_self(), pid, &task_port);

  if(kr != KERN_SUCCESS) {
    printf("[ >>> ] %d -> %x [%d]\n", pid, task_port, kr);
    return 1;
  }

  printf("[ >>> ] %d -> %x [%d]\n", pid, task_port, kr);

  return task_port;
}
