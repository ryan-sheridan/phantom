#ifndef BP_WP_H
#define BP_WP_H

#include <stdlib.h>

typedef struct {
 int index;
 uint64_t addr;
} breakpoint_t;

int add_breakpoint(uint64_t addr);
int remove_breakpoint_at_addr(uint64_t addr);
int remove_breakpoint_at_index(size_t idx);
void list_breakpoints(void);

#endif
