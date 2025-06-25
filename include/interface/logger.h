#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

#define LOG_ERR(...) fprintf(stderr, __VA_ARGS__)
#define LOG_INFO(...) printf(stderr, "[i]" __VA_ARGS__)
#define LOG_ACTION(...) printf(stderr, "[+]" __VA_ARGS__)
#define LOG_FAILURE(...) printf(stderr, "[-]" __VA_ARGS__)

#endif

