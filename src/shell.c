#include "shell.h"
#include "debugger.h"
#include <string.h>

// built in command to exit the shell. if you type exit N
// it should close the shell and return N to operating system
int cmd_exit(int argc, char **argv) {
  int status = 0;
  // if user types number after exit
  if(argc > 1) {
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
  (void)argc;  // unused
  (void)argv;  // unused
  printf("Available commands:\n");
  // loop through builtins array until we reach null
  for (const builtin_cmd_t *b = builtins; b->name; ++b) {
      // print command name and its help text
      printf("  %-8s  %s\n", b->name, b->help);
  }
  return 0; // success
}

int cmd_attach(int argc, char **argv) {
  if(argc < 2) {
    printf("Usage: attach <pid_or_name>\n");
    return 1;
  }

  // we need to determine if the argument is all digits (pid)
  char *arg = argv[1];
  int all_digits = 1;
  for(size_t i = 0; i < strlen(arg); i++) {
    if(!isdigit((unsigned char)arg[i])) {
      all_digits = 0;
      break;
    }
  }

  pid_t pid = 0;
  if(all_digits) {
    pid = (pid_t)atoi(arg);
  } else {
    // we should not be able to attach to self
    if(strcmp(arg, "phantom") == 0) {
      printf("Cannot attach to self!\n");
      return 1;
    }

    // argv[1] is a name, we can use pgrep to find its pid
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "/usr/bin/pgrep -n %s", arg);

    // popen runs the command and gives a FILE* to read the output
    // we expect a single line containing the PID
    FILE *fp = popen(cmd, "r");
    if(!fp) {
      perror("pgrep");
      return 1;
    }

    // we need to read the integer from the pipe
    // if this fails there was probably no process matched
    if(fscanf(fp, "%d", &pid) != 1) {
      printf("Process '%s' not found\n", arg);
      pclose(fp);
      return 1;
    }
    pclose(fp);
  }

  if(getpid() == pid) {
    printf("Cannot attach to self!\n");
    return 1;
  }

  if(!attach(pid) == pid) {
    printf("Process attach failed with pid %d\n", pid);
    return 1;
  }

  attached_pid = pid;
  return 0;
}

int cmd_resume(int argc, char **argv) {
  if(!attached_pid) {
    printf("You have to attach to a process first!\n");
    return 1;
  }

  resume();

  return 0;
}

int cmd_interrupt(int argc, char **argv) {
  if(!attached_pid) {
    printf("You have to attach to a process first!\n");
    return 1;
  }

  interrupt();

  return 0;
}

int cmd_detach(int argc, char **argv) {
  if(!attached_pid) {
    printf("You have to attach to a process first!\n");
    return 1;
  }

  detach();

  attached_pid = 0;

  return 0;
}

int cmd_reg(int argc, char **argv) {
  if(argc < 2) {
    printf("Usage: reg [read|write]\n");
    return 1;
  }

  char *arg = argv[1];

  // determine if a read or write
  if(strcmp(arg, "read") == 0) {
    print_registers();
  } else {
    printf("write");
  }

  if(!attached_pid) {
    printf("You have to attach a process first!\n");
    return 1;
  }

  return 0;
}

// array of builtin commands, each entry has a
// - name - word that you type
// - func - the function to call
// - help - a short desc of the function
const builtin_cmd_t builtins[] = {
 { "help", cmd_help, "shows the help page" },
 { "attach", cmd_attach, "attach to a process by pid or name" },
 { "resume", cmd_resume, "resume attached process execution" },
 { "suspend", cmd_interrupt, "suspend attached process execution" },
 { "detach", cmd_detach, "detach from attached process" },
 { "reg", cmd_reg, "read or write to registers syntax: reg [read|write]" },
 { "q", cmd_exit, "exits the program" },
 { NULL, NULL, NULL } // end marker
};

// here we check if the first word (argv[0]) matches any builtin command
// if it does, call that command function and return the result
// if not, retunr -1 to signal "not found"
int dispatch_builtin(int argc, char **argv) {
 if(argc == 0) return -1; // no command typed, return -1

 // try each builtin in order
 for(const builtin_cmd_t *b = builtins; b->name; ++b) {
    if(strcmp(argv[0], b->name) == 0) {
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
  while(printf("phantom> "), getline(&line, &len, stdin) > 0) {
    char *argv[64]; // arr to hold 63 words + NULL
    int argc = 0;

    // break line into tokens seperated by space/tab/newline
    char *tok = strtok(line, " \t\r\n");
    while(tok && argc < 63) {
      argv[argc++] = tok;
      tok = strtok(NULL, " \t\r\n");
    }
    argv[argc] = NULL; // argv must end with NULL pointer

    // if user just pressed enter, go back to prompt
    if(argc == 0) continue;

    // try builtin function
    if(dispatch_builtin(argc, argv) == -1) {
      // not a builtin
      printf("phantom: command not found: %s\n", line);
    }
  }
  // free buffer allocated by getline
  free(line);
}

