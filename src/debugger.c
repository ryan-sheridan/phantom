#include "debugger.h"

int attach(pid_t pid) {
  // attach to process with ptrace
  if(ptrace(PT_ATTACH, pid, (caddr_t)0, 0) == -1) {
    perror("ptrace attach");
    return 1;
  }

  // we need to wait until the process stops
  waitpid(pid, NULL, 0);
  printf("Attached to process %d\n", pid);
  return pid;
}
