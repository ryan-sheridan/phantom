#include "debugger.h"

int attach(pid_t pid) {
  // attach to process with ptrace
  if(ptrace(PT_ATTACH, pid, (caddr_t)0, 0) == -1) {
    perror("ptrace attach");
    return 1;
  }

  // we need to wait until the process stops
  int status;
  waitpid(pid, &status, 0);
  printf("Attached to process %d\n", pid);

  if(WIFSTOPPED(status)) {
    int sig = WSTOPSIG(status);
    printf("Process %d stopped on signal %s\n", pid, strsignal(sig));
  }

  if(WIFEXITED(status)) {
    printf("Process %d exited with code %d\n",
        pid, WEXITSTATUS(status));
  }

  return pid;
}
