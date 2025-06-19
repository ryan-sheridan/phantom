#include "debugger.h"
#include "mach_process.h"
#include <inttypes.h>
#include <mach/kern_return.h>
#include <stdio.h>

int attach(pid_t pid) {
  kern_return_t kr = setup_exception_port(pid);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "[-] setup_exception_port failed: %s (0x%x)\n",
            mach_error_string(kr), kr);
    return 1;
  }
  printf("[+] Exception port setup configured successfully for: %d\n", pid);
  attached_pid = pid;
  return pid;
}

int resume(void) {
  kern_return_t kr = mach_resume();
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "[-] task_resume failed: %s (0x%x)\n",
            mach_error_string(kr), kr);
  }

  printf("[+] task resumed successfully\n");
  return 0;
}

int interrupt(void) {
  kern_return_t kr = mach_suspend();
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "[-] task_suspend failed: %s (0x%x)\n",
            mach_error_string(kr), kr);
    return 1;
  }
  return 0;
}

int detach(void) {
  kern_return_t kr = mach_detach();
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "[-] mach_detach failed: %s (0x%x)\n",
            mach_error_string(kr), kr);
  } else {
    printf("[+] mach exception port torn down\n");
  }

  printf("[+] detached from %d\n", attached_pid);
  return 0;
}

int print_registers(void) {
  kern_return_t kr = mach_register_print();
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "[-] mach_register_read failed: %s (0x%x)\n",
            mach_error_string(kr), kr);
  }
  return 0;
}

int write_registers(const char reg[], uint64_t value) {
  kern_return_t kr = mach_register_write(reg, value);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "[-] mach_register_write failed: %s (0x%x)\n",
            mach_error_string(kr), kr);
  }
  return 0;
}

int print_debug_registers(void) {
  kern_return_t kr = mach_register_debug_print();
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "[-] mach_register_debug_read failed: %s (0x%x)\n",
            mach_error_string(kr), kr);
  }
  return 0;
}

static void _hex_dump(const void *data, size_t size) {
  char ascii[17];
  size_t i, j;
  ascii[16] = '\0';
  for (i = 0; i < size; ++i) {
    printf("%02X ", ((unsigned char *)data)[i]);
    if (((unsigned char *)data)[i] >= ' ' &&
        ((unsigned char *)data)[i] <= '~') {
      ascii[i % 16] = ((unsigned char *)data)[i];
    } else {
      ascii[i % 16] = '.';
    }
    if ((i + 1) % 8 == 0 || i + 1 == size) {
      printf(" ");
      if ((i + 1) % 16 == 0) {
        printf("|  %s \n", ascii);
      } else if (i + 1 == size) {
        ascii[(i + 1) % 16] = '\0';
        if ((i + 1) % 16 <= 8) {
          printf(" ");
        }
        for (j = (i + 1) % 16; j < 16; ++j) {
          printf("   ");
        }
        printf("|  %s \n", ascii);
      }
    }
  }
}

int read64(uint64_t addr) {
  uint64_t out;
  kern_return_t kr = mach_read64(addr, &out);
  if (kr != KERN_SUCCESS) {
    fprintf(stderr, "[-] mach_read64 failed: %s (0x%x)\n",
            mach_error_string(kr), kr);
    return 1;
  }

  _hex_dump(&out, sizeof(uint64_t));

  return 0;
}
