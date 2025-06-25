#include "interface/shell.h"
#include "dbg/bp_wp.h"
#include "dbg/debugger.h"
#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the process name that is attached
char *attached_name = NULL;
pid_t attached_pid = 0;

// built in command to exit the shell. if you type exit N
// it should close the shell and return N to operating system
static int cmd_exit(int argc, char **argv) {
  int status = 0;
  if (argc > 1) {
    status = atoi(argv[1]);
  }
  exit(status);
  return status; // never reached
}

// vibe
static int cmd_help(int argc, char **argv) {
  (void)argc;
  (void)argv;

  // 1) Find maximum command-name length
  size_t maxlen = 0;
  for (const builtin_cmd_t *b = builtins; b->name; ++b) {
    size_t len = strlen(b->name);
    if (len > maxlen)
      maxlen = len;
  }

  // 2) Print header
  printf("Available commands:\n\n");

  // 3) For each command, align descriptions in column (maxlen + 2 spaces)
  const size_t desc_indent = maxlen + 4; // name + padding
  const size_t term_width = 80;
  for (const builtin_cmd_t *b = builtins; b->name; ++b) {
    // Print the command name in bold
    printf("  \x1b[1m%-*s\x1b[0m  | ", (int)maxlen, b->name);

    // Wrap the help text if it's too long
    const char *desc = b->help;
    size_t col = desc_indent;
    for (const char *p = desc; *p; ++p) {
      if (*p == '\n') {
        putchar('\n');
        for (size_t i = 0; i < desc_indent; ++i)
          putchar(' ');
        col = desc_indent;
      } else {
        putchar(*p);
        if (++col >= term_width && *p == ' ') {
          // break line here
          putchar('\n');
          for (size_t i = 0; i < desc_indent; ++i)
            putchar(' ');
          col = desc_indent;
        }
      }
    }
    putchar('\n');
  }

  return 0;
}

// check for attached process, return non-zero and print error if none
static int require_attached(void) {
  if (!attached_pid) {
    printf("You have to attach to a process first!\n");
    return 1;
  }
  return 0;
}

// prompt printer: (phantom) in gray, process-name in green if attached
void print_prompt(void) {
  const char *LIGHT_GRAY = "\x1b[2;37m";
  const char *GREEN = "\x1b[32m";
  const char *RESET = "\x1b[0m";
  printf("%s(phantom)%s", LIGHT_GRAY, RESET);
  if (attached_name) {
    printf(" %s%s%s", GREEN, attached_name, RESET);
  }
  printf(" > ");
  fflush(stdout);
}

static int cmd_attach(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: attach <pid_or_name>\n");
    return 1;
  }

  const char *arg = argv[1];
  pid_t pid = 0;

  // detect numeric pid
  if (strspn(arg, "0123456789") == strlen(arg)) {
    pid = (pid_t)atoi(arg);
  } else {
    if (strcmp(arg, "phantom") == 0) {
      printf("Cannot attach to self!\n");
      return 1;
    }
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/usr/bin/pgrep -x %s", arg);
    FILE *fp = popen(cmd, "r");
    if (!fp) {
      perror("pgrep");
      return 1;
    }
    if (fscanf(fp, "%d", &pid) != 1) {
      printf("Process '%s' not found\n", arg);
      pclose(fp);
      return 1;
    }
    pclose(fp);
  }

  if (getpid() == pid) {
    printf("Cannot attach to self!\n");
    return 1;
  }

  if (attach(pid) != pid) {
    printf("Process attach failed with pid %d\n", pid);
    return 1;
  }

  attached_pid = pid;

  if (attached_name)
    free(attached_name);
  attached_name = strdup(arg);
  return 0;
}

static int cmd_continue(int argc, char **argv) {
  (void)argc;
  (void)argv;
  if (require_attached())
    return 1;
  resume();
  return 0;
}

static int cmd_interrupt(int argc, char **argv) {
  (void)argc;
  (void)argv;
  if (require_attached())
    return 1;
  interrupt();
  return 0;
}

static int cmd_detach(int argc, char **argv) {
  (void)argc;
  (void)argv;
  if (require_attached())
    return 1;
  detach();
  attached_pid = 0;
  if (attached_name) {
    free(attached_name);
    attached_name = NULL;
  }

  return 0;
}

static int cmd_reg(int argc, char **argv) {
  if (require_attached())
    return 1;
  if (argc < 2) {
    printf("Usage: register [read|write]\n");
    return 1;
  }
  if (strcmp(argv[1], "read") == 0) {
    print_registers();
  } else if (strcmp(argv[1], "write") == 0) {
    if (argc != 4) {
      printf("Usage: register write takes exactly 2 arguments: <reg-name> "
             "<value>\n");
      return 1;
    }
    write_registers(argv[2], strtoull(argv[3], NULL, 0));
  } else {
    printf("Usage: register [read|write]\n");
    return 1;
  }
  return 0;
}

static int cmd_reg_dbg(int argc, char **argv) {
  (void)argc;
  (void)argv;
  if (require_attached())
    return 1;
  print_debug_registers();
  return 0;
}

static int cmd_dp(int argc, char **argv) {
  if (require_attached())
    return 1;
  if (argc < 2) {
    printf("Usage: breakpoint set <address> | breakpoint delete <address> | "
           "breakpoint list\n");
    printf("       watchpoint set <address> | watchpoint delete <address> | "
           "watchpoint list\n");
    return 1;
  }

  const char *type = argv[0];

  if (strcmp(type, "breakpoint") != 0 && strcmp(type, "watchpoint") != 0) {
    printf("Usage: breakpoint set <address> | breakpoint delete <address> | "
           "breakpoint list\n");
    printf("       watchpoint set <address> | watchpoint delete <address> | "
           "watchpoint list\n");
    return 1;
  }

  const char *cmd = argv[1];

  if (strcmp(cmd, "list") == 0 && argc == 2) {
    // we should really have this exported from bp_wp to the debugger
    // TODO: this will make modularisation cleaner
    if (strcmp(type, "breakpoint") == 0) {
      list_breakpoints();
    } else {
      list_watchpoints();
    }
    return 0;
  } else if (strcmp(cmd, "set") == 0 && argc == 3) {
    if (strcmp(type, "breakpoint") == 0) {
      add_breakpoint(strtoull(argv[2], NULL, 0));
    } else {
      add_watchpoint(strtoull(argv[2], NULL, 0));
    }
    return 0;
  } else if (strcmp(cmd, "delete") == 0 && argc == 3) {
    const char *param = argv[2];
    if (strspn(param, "0123456789") == strlen(param)) {
      if (strcmp(type, "breakpoint") == 0) {
        remove_breakpoint_at_index(atoi(param));
      } else {
        remove_watchpoint_at_index(atoi(param));
      }
    } else {
      if (strcmp(type, "breakpoint") == 0) {
        remove_breakpoint_by_addr(strtoull(param, NULL, 0));
      } else {
        remove_watchpoint_by_addr(strtoull(param, NULL, 0));
      }
    }
    return 0;
  }

  if (strcmp(type, "breakpoint") == 0) {
    printf("Usage: breakpoint set <address> | breakpoint delete <address> | "
           "breakpoint list\n");
  } else {
    printf("Usage: watchpoint set <address> | watchpoint delete <address> | "
           "watchpoint list\n");
  }
  return 1;
}

static int cmd_read(int argc, char **argv) {
  if(require_attached())
    return 1;

  if(argc < 3) {
    printf("Usage: read <addr> <size>\n");
    return 1;
  }

  uintptr_t addr = strtoull(argv[1], NULL, 0);
  size_t size = strtoull(argv[2], NULL, 0);

  read_arb(addr, size);

  return 0;
}

static int cmd_write(int argc, char **argv) {
  if(require_attached())
    return 1;

  if(argc < 3) {
    printf("Usage: write <addr> <bytes>\n");
    return 1;
  }

  uintptr_t addr = strtoull(argv[1], NULL, 0);
  uint64_t bytes = strtoull(argv[2], NULL, 0);
  size_t size = 0;

  uint64_t temp = bytes;
  while(temp) {
    size++;
    temp>>=8;
  }

  if(size == 0) size = 1;

  write_arb(addr, &bytes, size);

  return 0;
}

// TODO: you know what todo pal
static int cmd_r64(int argc, char **argv) {
  if (require_attached())
    return 1;

  if (argc < 2) {
    printf("Usage: read64 <addr>\n");
  }

  uint64_t addr = strtoull(argv[1], NULL, 0);
  read64(addr);

  return 0;
}

static int cmd_w64(int argc, char **argv) {
  if (require_attached())
    return 1;

  if (argc < 3) {
    printf("Usage: write64 <addr> <bytes>\n");
  }

  uint64_t addr = strtoull(argv[1], NULL, 0);
  uint64_t bytes = strtoull(argv[2], NULL, 0);

  write64(addr, bytes);

  return 0;
}

static int cmd_r32(int argc, char **argv) {
  if (require_attached())
    return 1;

  if (argc < 2) {
    printf("Usage: read32 <addr>\n");
    return 1;
  }

  uint64_t addr = strtoull(argv[1], NULL, 0);
  read32(addr); // Assumes you have a read32 function

  return 0;
}

static int cmd_w32(int argc, char **argv) {
  if (require_attached())
    return 1;

  if (argc < 3) {
    printf("Usage: write32 <addr> <bytes>\n");
    return 1;
  }

  uint64_t addr = strtoull(argv[1], NULL, 0);
  uint32_t bytes = strtoul(argv[2], NULL, 0); // 32-bit version

  write32(addr, bytes); // Assumes you have a write32 function

  return 0;
}

int cmd_slide(int argc, char **argv) {
  (void)argc;
  (void)argv;

  if (require_attached())
    return 1;

  print_slide();

  return 0;
}

int cmd_autoslide(int argc, char **argv) {
  (void)argc;
  (void)argv;

  if (require_attached())
    return 1;

  toggle_slide();

  return 0;
}

int cmd_step(int argc, char **argv) {
  if (require_attached())
    return 1;

  (void)argc;
  (void)argv;

  step();

  return 0;
}

int cmd_disasm(int argc, char **argv) {
  if (require_attached())
    return 1;

  uintptr_t addr;

  if (argc < 2) {
    printf("Usage: disassemble <bytes>\n");
    return 1;
  }

  addr = pc();
  uint64_t bytes = strtoull(argv[1], NULL, 0);

  disasm(addr, bytes);

  return 0;
}

// array of builtin commands, each entry has a
// - name - word that you type
// - func - the function to call
const builtin_cmd_t builtins[] = {
    {"help", cmd_help, "Show this help page with available commands"},

    {"attach", cmd_attach, "Attach to a process by PID or name"},
    {"suspend", cmd_interrupt, "Suspend execution of the attached process"},
    {"continue", cmd_continue, "Resume execution of the attached process"},
    {"detach", cmd_detach, "Detach from the currently attached process"},

    {"register", cmd_reg,
     "Read from or write to general-purpose registers\n\tsyntax: register "
     "[read|write] <reg> [value]"},
    {"registerd", cmd_reg_dbg, "Read values from debug registers"},

    {"breakpoint", cmd_dp,
     "Manage breakpoints by address or index\n\t"
     "syntax: breakpoint set <address> | breakpoint delete <address|index> | "
     "breakpoint list"},
    {"watchpoint", cmd_dp,
     "Manage watchpoints by address or index\n\t"
     "syntax: watchpoint set <address> | watchpoint delete <address|index> | "
     "watchpoint list"},
    {"step", cmd_step, "Step into the next machine instruction"},


    {"read", cmd_read, "Dump an arbitrary amount of memory to console\n\tsyntax: read <addr> <size>"},
    {"write", cmd_write, "Write an airbitrary amount of memory to task\n\tsyntax: write <addr> <bytes>"},

    {"read64", cmd_r64,
     "Read 64 bits from memory at a specified address\n\tsyntax: memory read64 "
     "<address>"},
    {"write64", cmd_w64,
     "Write 64 bits to memory at a specified address\n\tsyntax: memory write64 "
     "<address> <bytes>"},
    {"read32", cmd_r32,
     "Read 32 bits from memory at a specified address\n\tsyntax: memory read32 "
     "<address>"},
    {"write32", cmd_w32,
     "Write 32 bits to memory at a specified address\n\tsyntax: memory write32 "
     "<address> <bytes>"},

    {"slide", cmd_slide, "Print the ASLR slide of the attached process"},
    {"autoslide", cmd_autoslide,
     "Enable automatic ASLR slide calculation on memory read/write"},

    {"disassemble", cmd_disasm,
     "Disassemble from the current program counter\n\tsyntax: disassemble "
     "[bytes]"},

    {"quit", cmd_exit, "Exit the debugger"},

    {NULL, NULL, NULL}};

// dispatch or signal not found
static int dispatch_builtin(int argc, char **argv) {
  if (argc == 0)
    return -1;
  for (const builtin_cmd_t *b = builtins; b->name; ++b) {
    if (strncmp(b->name, argv[0], strlen(argv[0])) == 0) {
      return b->func(argc, argv);
    }
  }
  return -1;
}

// thank you gpt
// for some reason this fixes segfault that happens when i use -O2
// but we need -O2 because for some reason exception ports dont work
// - man i love c
static char *read_line(void) {
  size_t cap = 128;
  size_t len = 0;
  char *buf = malloc(cap);
  if (!buf) {
    perror("malloc");
    return NULL;
  }

  int c;
  while ((c = getchar()) != EOF && c != '\n') {
    if (len + 1 >= cap) {
      cap *= 2;
      char *tmp = realloc(buf, cap);
      if (!tmp) {
        free(buf);
        perror("realloc");
        return NULL;
      }
      buf = tmp;
    }
    buf[len++] = (char)c;
  }

  if (c == EOF && len == 0) {
    free(buf);
    return NULL;
  }

  buf[len++] = '\n'; // retain newline for tokenization consistency
  buf[len] = '\0';
  return buf;
}

void shell_loop(void) {
  char *line;
  while (print_prompt(), (line = read_line()) != NULL) {
    char *argv[64];
    int argc = 0;

    // tokenize on whitespace
    char *tok = strtok(line, " \t\r\n");
    while (tok && argc < 63) {
      argv[argc++] = tok;
      tok = strtok(NULL, " \t\r\n");
    }
    argv[argc] = NULL;

    if (argc > 0) {
      if (dispatch_builtin(argc, argv) == -1) {
        printf("phantom: command not found: %s\n", argv[0]);
      }
    }

    free(line);
  }
}
