#include "shell.h"
#include "bp_wp.h"
#include "debugger.h"
#include <inttypes.h>
#include <string.h>

// built in command to exit the shell. if you type exit N
// it should close the shell and return N to operating system
int cmd_exit(int argc, char **argv) {
  int status = 0;
  // if user types number after exit
  if (argc > 1) {
    // use it as a return code
    status = atoi(argv[1]);
  }

  // exit entire program now
  exit(status);
  // this return is never reached but sure
  return status;
}

// built in command to print a list of all availiable commands
// with there short descriptions
int cmd_help(int argc, char **argv) {
  (void)argc; // unused
  (void)argv; // unused
  printf("Available commands:\n");
  // loop through builtins array until we reach null
  for (const builtin_cmd_t *b = builtins; b->name; ++b) {
    // print command name and its help text
    printf("  %-8s  %s\n", b->name, b->help);
  }
  return 0; // success
}

int cmd_attach(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: attach <pid_or_name>\n");
    return 1;
  }

  // we need to determine if the argument is all digits (pid)
  char *arg = argv[1];
  int all_digits = 1;
  for (size_t i = 0; i < strlen(arg); i++) {
    if (!isdigit((unsigned char)arg[i])) {
      all_digits = 0;
      break;
    }
  }

  pid_t pid = 0;
  if (all_digits) {
    pid = (pid_t)atoi(arg);
  } else {
    // we should not be able to attach to self
    if (strcmp(arg, "phantom") == 0) {
      printf("Cannot attach to self!\n");
      return 1;
    }

    // argv[1] is a name, we can use pgrep to find its pid
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/usr/bin/pgrep -n %s", arg);

    // popen runs the command and gives a FILE* to read the output
    // we expect a single line containing the PID
    FILE *fp = popen(cmd, "r");
    if (!fp) {
      perror("pgrep");
      return 1;
    }

    // we need to read the integer from the pipe
    // if this fails there was probably no process matched
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

  if (!attach(pid) == pid) {
    printf("Process attach failed with pid %d\n", pid);
    return 1;
  }

  attached_pid = pid;
  return 0;
}

int cmd_continue(int argc, char **argv) {
  if (!attached_pid) {
    printf("You have to attach to a process first!\n");
    return 1;
  }

  resume();

  return 0;
}

int cmd_interrupt(int argc, char **argv) {
  if (!attached_pid) {
    printf("You have to attach to a process first!\n");
    return 1;
  }

  interrupt();

  return 0;
}

int cmd_detach(int argc, char **argv) {
  if (!attached_pid) {
    printf("You have to attach to a process first!\n");
    return 1;
  }

  detach();

  attached_pid = 0;

  return 0;
}

int cmd_reg(int argc, char **argv) {
  if (argc < 2) {
    printf("Usage: reg [read|write]\n");
    return 1;
  }

  char *arg = argv[1];

  // determine if a read or write
  if (strcmp(arg, "read") == 0) {
    print_registers();
  } else {
    if (argc < 4) {
      printf(
          "Usage: reg write takes exactly 2 arguments: <reg-name> <value>\n");
      return 1;
    }

    const char *reg = argv[2];
    uint64_t value = strtoull(argv[3], NULL, 0);

    write_registers(reg, value);
  }

  if (!attached_pid) {
    printf("You have to attach a process first!\n");
    return 1;
  }

  return 0;
}

int cmd_reg_dbg(int argc, char **argv) {
  if (!attached_pid) {
    printf("You have to attach a process first!\n");
    return 1;
  }
  print_debug_registers();
  return 0;
}

int cmd_br(int argc, char **argv) {
  // if(!attached_pid) {
  //   printf("You have to attach a process first\n");
  //   return 1;
  // }

  if (argc < 2) {
    printf("Usage: br set <address> | br delete <address> | br list\n");
    return 1;
  }

  const char *arg = argv[1];
  uint64_t addr;

  if (strcmp(arg, "list") == 0) {
    if (argc != 2) {
      printf("Usage: br list\n");
      return 1;
    }
    list_breakpoints();
    return 0;
  }

  if (strcmp(arg, "set") == 0) {
    if (argc != 3) {
      printf("Usage: br set <address>\n");
      return 1;
    }
    addr = strtoull(argv[2], NULL, 0);
    add_breakpoint(addr);
    return 0;
  }

  if (strcmp(arg, "delete") == 0) {
    if (argc != 3) {
      printf("Usage: br delete <address|index>\n");
      return 1;
    }
    const char *param = argv[2];

    // if it's all digits (no "0x" prefix), treat as breakpoint index
    if (strspn(param, "0123456789") == strlen(param)) {
      int idx = atoi(param);
      remove_breakpoint_at_index(idx);
    } else {
      // otherwise parse as an address (hex or decimal)
      addr = strtoull(param, NULL, 0);
      remove_breakpoint_by_addr(addr);
    }

    return 0;
  }

  printf("Usage: br set <address> | br delete <address> | br list\n");
  return 1;
}

// array of builtin commands, each entry has a
// - name - word that you type
// - func - the function to call
// - help - a short desc of the function
const builtin_cmd_t builtins[] = {
    {"help", cmd_help, "shows the help page"},
    {"attach", cmd_attach, "attach to a process by pid or name"},
    {"c", cmd_continue, "continuneattached process execution"},
    {"suspend", cmd_interrupt, "suspend attached process execution"},
    {"detach", cmd_detach, "detach from attached process"},
    {"reg", cmd_reg,
     "read or write to registers\n\t syntax: reg [read|write] <reg> [value]"},
    { "regdbg",  cmd_reg_dbg,    "read debug registers" },
    {"br", cmd_br,
     "list, set or delete a breakpoint by address\n\t syntax: br set <address> "
     "| br delete <address> | br list"},
    // { "wp",      cmd_br,         "list, set or delete a watchpoint by
    // address\n\t syntax: wp set <address> | wp delete <address> | wp list" },
    {"q", cmd_exit, "exits the program"},
    {NULL, NULL, NULL}};

// here we check if the first word (argv[0]) matches any builtin command
// if it does, call that command function and return the result
// if not, retunr -1 to signal "not found"
int dispatch_builtin(int argc, char **argv) {
  if (argc == 0)
    return -1; // no command typed, return -1

  // try each builtin in order
  for (const builtin_cmd_t *b = builtins; b->name; ++b) {
    if (strcmp(argv[0], b->name) == 0) {
      // found match, now we run it and return its result
      return b->func(argc, argv);
    }
  }
  // not a builtin
  return -1;
}

// a few steps for this important function
// 1. print a prompt, being "phantom> "
// 2. read a line of input
// 3. split it into words, argv[]
// 4. if its a builtin, run it
// 5. otherwise show "command not found"
// 6. repeat until EOF
void shell_loop(void) {
  char *line = NULL;
  size_t len;

  // keep running until getline() returns <= 0 (EOF or err)
  while (printf("phantom> "), getline(&line, &len, stdin) > 0) {
    char *argv[64]; // arr to hold 63 words + NULL
    int argc = 0;

    // break line into tokens seperated by space/tab/newline
    char *tok = strtok(line, " \t\r\n");
    while (tok && argc < 63) {
      argv[argc++] = tok;
      tok = strtok(NULL, " \t\r\n");
    }
    argv[argc] = NULL; // argv must end with NULL pointer

    // if user just pressed enter, go back to prompt
    if (argc == 0)
      continue;

    // try builtin function
    if (dispatch_builtin(argc, argv) == -1) {
      // not a builtin
      printf("phantom: command not found: %s\n", line);
    }
  }
  // free buffer allocated by getline
  free(line);
}
