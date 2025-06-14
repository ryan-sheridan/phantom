#include "debugger.h"
#include "mach_vm_helper.h"

pid_t attached_pid = 0;

int attach(pid_t pid) {
  // attach to process with ptrace
  if(ptrace(PT_ATTACH, pid, (caddr_t)0, 0) == -1) {
    perror("ptrace attach");
    return -1;
  }

  // we need to wait until the process stops
  int status;
  waitpid(pid, &status, 0);
  printf("[+] Attached to process %d\n", pid);

  if(WIFSTOPPED(status)) {
    int sig = WSTOPSIG(status);
    printf("[+] Process %d stopped on signal %s\n", pid, strsignal(sig));
  }

  if(WIFEXITED(status)) {
    printf("[+] Process %d exited with code %d\n",
        pid, WEXITSTATUS(status));
  }

  kern_return_t kr = setup_exception_port(pid);
  if (kr != KERN_SUCCESS) {
      fprintf(stderr,
              "[-] setup_exception_port failed: %s (0x%x)\n",
              mach_error_string(kr),
              kr);
      return 1;
  }
  printf("[+] Exception port setup configured successfully for: %d\n", pid);

  if (ptrace(PT_CONTINUE, pid, (caddr_t)1, 0) == -1) {
    perror("ptrace continue");
    return 1;
  }

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

int stop(void) {
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

  // then we detach ptrace
  if(ptrace(PT_DETACH, attached_pid, (caddr_t)0, 0) == -1) {
    perror("ptrace detach");
  }

  printf("[+] ptrace detached from %d\n", attached_pid);
  return 0;

}
