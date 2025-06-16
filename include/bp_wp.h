#ifndef BP_WP_H
#define BP_WP_H

#include <stdlib.h>

typedef struct {
 int index;
 uint64_t addr;
} breakpoint_t;

// TODO: watchpoints
static breakpoint_t *breakpoints = NULL;
static size_t bp_count = 0;
static size_t bp_capacity = 0;

int add_breakpoint(uint64_t addr);
int remove_breakpoint_by_addr(uint64_t addr);
int remove_breakpoint_at_index(size_t idx);
void list_breakpoints(void);

#endif
