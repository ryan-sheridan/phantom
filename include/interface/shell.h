// this header file sets up the parts needed for "built-in"
// commands in our shell like interface, a built in command
// being one that the shell handles itself

// guards: the following tells the compiler to not include this
// file more than once, if SHELL_H is not defined yet, we #define
// SHELL_H, otherwise, skip everything until the #endif
#ifndef SHELL_H
#define SHELL_H

#include <ctype.h>
#include <stdio.h>  // for printf, getline
#include <stdlib.h> // for exit, atoi
#include <string.h> // for strcmp, strtok
#include <unistd.h>

// builtin_f
// a function pointer tupe for built in commands, every build in
// command function must match this shape:
// - takes an int, argc, which is the count of words in the cmd
// - takes an array of strings, argv, which are the words themselves
// - returns 0 if the command worked, or non-zero if there was an err
typedef int (*builtin_fn)(int argc, char **argv);

// builtin_cmd_t
// a simple struct to describe a builtin command, we denote this
// struct type with _t, to describe that it is a type, just like size_t
// - name - the word you type to run the command (e.g. help)
// - func - the c function that gets called when you run the command
// - help - a short message about the command
typedef struct {
  const char *name;
  builtin_fn func;
  const char *help;
} builtin_cmd_t;

// builtins[]
// this is an array of builtin commands, we can deifne and fill this array
// in shell.c, the last entry will be all NULL's to mark the end
// the shell code will loop through the list to find and run the right
// command when you type something
extern const builtin_cmd_t builtins[];

// shell_loop
// the loop which is called at the start of the main function
// once this returns, the program exits
void shell_loop(void);

void print_prompt(void);

#endif
