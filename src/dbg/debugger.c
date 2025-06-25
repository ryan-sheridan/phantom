#include "dbg/debugger.h"
#include "interface/logger.h"
#include "mach/mach_process.h"
#include <capstone/capstone.h>
#include <inttypes.h>
#include <mach/kern_return.h>
#include <stdio.h>

int attach(pid_t pid) {
  kern_return_t kr = setup_exception_port(pid);
  if (kr != KERN_SUCCESS) {
    LOG_ERR("setup_exception_port failed: %s (0x%x)\n", mach_error_string(kr),
            kr);
    return 1;
  }
  printf("exception port setup configured successfully for: %d\n", pid);
  attached_pid = pid;
  return pid;
}

int resume(void) {
  kern_return_t kr = mach_resume();
  if (kr != KERN_SUCCESS) {
    LOG_ERR("task_resume failed: %s (0x%x)\n", mach_error_string(kr), kr);
  }

  printf("task resumed successfully\n");
  return 0;
}

int interrupt(void) {
  kern_return_t kr = mach_suspend();
  if (kr != KERN_SUCCESS) {
    LOG_ERR("task_suspend failed: %s (0x%x)\n", mach_error_string(kr), kr);
    return 1;
  }

  printf("task suspended successfully\n");
  return 0;
}

int detach(void) {
  kern_return_t kr = mach_detach();
  if (kr != KERN_SUCCESS) {
    LOG_ERR("mach_detach failed: %s (0x%x)\n", mach_error_string(kr), kr);
  } else {
    printf("mach exception port torn down\n");
  }

  printf("detached from %d\n", attached_pid);
  return 0;
}

int print_registers(void) {
  kern_return_t kr = mach_register_print();
  if (kr != KERN_SUCCESS) {
    LOG_ERR("mach_register_read failed: %s (0x%x)\n", mach_error_string(kr),
            kr);
  }
  return 0;
}

int write_registers(const char reg[], uint64_t value) {
  kern_return_t kr = mach_register_write(reg, value);
  if (kr != KERN_SUCCESS) {
    LOG_ERR("mach_register_write failed: %s (0x%x)\n", mach_error_string(kr),
            kr);
  }
  return 0;
}

int print_debug_registers(void) {
  kern_return_t kr = mach_register_debug_print();
  if (kr != KERN_SUCCESS) {
    LOG_ERR("mach_register_debug_read failed: %s (0x%x)\n",
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

static int _read_code(uintptr_t addr, void *out, size_t size) {
  // round down to the nearest 4byte boundary
  uintptr_t aligned_addr = addr & ~(uintptr_t)(0x3);
  kern_return_t kr = mach_read(aligned_addr, out, size, false);
  if (kr != KERN_SUCCESS) {
    LOG_ERR("mach_read failed: %s (0x%x)\n", mach_error_string(kr), kr);
    return 1;
  }
  return 0;
}

int read_arb(uintptr_t addr, size_t size) {
  void *out = NULL;
  out = malloc(size);
  if(out == NULL) {
    LOG_ERR_FORCE("allocation error in read\n");
    return 1;
  }

  kern_return_t kr = mach_read(addr, out, size, false);
  if (kr != KERN_SUCCESS) {
    LOG_ERR_FORCE("mach_read failed: %s (0x%x)\n", mach_error_string(kr), kr);
    return 1;
  }

  _hex_dump(out, size);

  return 0;
}

int write_arb(uintptr_t addr, void *bytes, size_t size) {
  kern_return_t kr = mach_write(addr, bytes, size);
  if(kr != KERN_SUCCESS) {
    LOG_ERR_FORCE("mach_write failed: %s (0x%x)\n", mach_error_string(kr), kr);
    return 1;
  }

  if(!read_arb(addr, size))
    return 1;

  return 0;
}


int read64(uintptr_t addr) {
  uint64_t out;
  kern_return_t kr = mach_read64(addr, &out);
  if (kr != KERN_SUCCESS) {
    LOG_ERR("mach_read64 failed: %s (0x%x)\n", mach_error_string(kr), kr);
    return 1;
  }

  _hex_dump(&out, sizeof(uint64_t));

  return 0;
}

int read32(uintptr_t addr) {
  uint32_t out;
  kern_return_t kr = mach_read32(addr, &out);
  if (kr != KERN_SUCCESS) {
    LOG_ERR("mach_read32 failed: %s (0x%x)\n", mach_error_string(kr), kr);
    return 1;
  }

  _hex_dump(&out, sizeof(uint32_t));

  return 0;
}

int write64(uintptr_t addr, uint64_t bytes) {
  kern_return_t kr = mach_write64(addr, bytes);
  if (kr != KERN_SUCCESS) {
    LOG_ERR("mach_write64 failed: %s (0x%x)\n", mach_error_string(kr), kr);
    return 1;
  }

  // hexdump final changes
  read64(addr);

  return 0;
}

int write32(uintptr_t addr, uint32_t bytes) {
  kern_return_t kr = mach_write32(addr, bytes);
  if (kr != KERN_SUCCESS) {
    LOG_ERR("mach_write32 failed: %s (0x%x)\n", mach_error_string(kr), kr);
    return 1;
  }

  // hexdump final changes
  read32(addr);

  return 0;
}

bool slide_enabled = false;

int toggle_slide(void) {
  kern_return_t kr = mach_set_auto_slide_enabled(!slide_enabled);

  if (kr != KERN_SUCCESS) {
    LOG_ERR("mach_set_auto_slide_enabled failed: %s (0x%x)\n",
            mach_error_string(kr), kr);
    return 1;
  }

  const char *str_slide_enabled = (slide_enabled == true) ? "true" : "false";

  printf("aslr slide enabled: %s\n", str_slide_enabled);

  return 0;
}

mach_vm_address_t slide = (mach_vm_address_t)0;

int print_slide(void) {
  mach_vm_address_t slide;
  kern_return_t kr = mach_get_aslr_slide(&slide);

  if (kr != KERN_SUCCESS) {
    LOG_ERR("mach_get_aslr_slide failed: %s (0x%x)\n", mach_error_string(kr),
            kr);
    return 1;
  }

  printf("aslr slide: 0x%" PRIx64 "\n", (uint64_t)slide);

  return 0;
}

int step(void) {
  kern_return_t kr = mach_step();
  if (kr != KERN_SUCCESS) {
    LOG_ERR("mach_step failed: %s (0x%x)\n", mach_error_string(kr), kr);
    return 1;
  }
  resume();
  return 0;
}

int disasm(uintptr_t addr, size_t size) {
  csh handle;
  cs_insn *insn = NULL;
  void *code = NULL;
  int ret = 1;

  // init capstone
  cs_err err = cs_open(CS_ARCH_ARM64, CS_MODE_ARM, &handle);
  if (err != CS_ERR_OK) {
    LOG_ERR("capstone error: %s\n", cs_strerror(err));
    return 1;
  }

  // allocate some memory for our code/bytes
  code = malloc(size);
  if (code == NULL) {
    LOG_ERR("Error: Failed to allocate %zu bytes for code buffer.\n", size);
    goto cleanup_capstone;
  }

  // read it
  if (_read_code(addr, code, size) != 0) {
    LOG_ERR("disasm: _read_code: failed to read memory at 0x%" PRIxPTR
            " (size %zu)\n",
            addr, size);
    goto cleanup_code_buffer;
  }

  // disasm
  size_t count = cs_disasm(handle, code, size, addr, 0, &insn);
  if (count > 0) {
    // for each ins
    for (size_t j = 0; j < count; j++) {
      // print asm
      printf("0x%" PRIx64 ":\t%s\t\t%s\n", insn[j].address, insn[j].mnemonic,
             insn[j].op_str);
    }
    cs_free(insn, count);
    ret = 0;
  } else {
    cs_err err = cs_errno(handle);
    LOG_ERR_FORCE("ERROR: Failed to disassemble given code at 0x%" PRIxPTR
                  ". Capstone error: %s\n",
                  addr, cs_strerror(err));
  }

cleanup_code_buffer:
  free(code);
cleanup_capstone:
  cs_close(&handle);
  return ret;
}

uintptr_t pc(void) {
  uintptr_t pc;
  kern_return_t kr = mach_get_pc(&pc);
  if (kr != KERN_SUCCESS) {
    LOG_ERR("pc: mach_get_pc\n");
    return 1;
  }
  return pc;
}
