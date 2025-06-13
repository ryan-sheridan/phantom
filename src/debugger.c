#include "debugger.h"
#include "mach_vm_helper.h"

int attach(pid_t pid) {
  // attach to process with ptrace
  if(ptrace(PT_ATTACH, pid, (caddr_t)0, 0) == -1) {
    perror("ptrace attach");
    return -1;
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

  kern_return_t kr = setup_exception_port(pid);
  if (kr != KERN_SUCCESS) {
      fprintf(stderr,
              "setup_exception_port failed: %s (0x%x)\n",
              mach_error_string(kr),
              kr);
      return 1;
  }
  printf("Exception port setup configured successfully for: %d\n", pid);

  return pid;
}

int resume(void) {
  if (ptrace(PT_CONTINUE, attached_pid, (caddr_t)1, 0) < 0) {
      perror("ptrace(PT_CONTINUE)");
      return -1;
  }
  return 0;
}

int stop(void) {

  return 0;
}

int detach(void) {
  return 0;
}
