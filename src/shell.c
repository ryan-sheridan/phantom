#include "shell.h"
#include <stdio.h>  // for printf, getline
#include <stdlib.h> // for exit, atoi
#include <string.h> // for strcmp, strtok

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

// array of builtin commands, each entry has a
// - name - word that you type
// - func - the function to call
// - help - a short desc of the function
const builtin_cmd_t builtins[] = {
 { "help", cmd_help, "shows the help page" },
 { "exit", cmd_exit, "exits the program" },
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

