#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <stdbool.h>

extern bool verbose;

#define LOG_INFO(...)    if (verbose) fprintf(stderr, "[i] " __VA_ARGS__)
#define LOG_ACTION(...)  if (verbose) fprintf(stderr, "[+] " __VA_ARGS__)
#define LOG_ERR(...)     if (verbose) fprintf(stderr, "[-] " __VA_ARGS__)
#define LOG_ERR_FORCE(...)            fprintf(stderr, "[-] " __VA_ARGS__)

#endif

