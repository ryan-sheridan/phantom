#include "dbg/bp_wp.h"
#include <mach/kern_return.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "mach/mach_process.h"

static int ensure_capacity(size_t min_capacity) {
  if(bp_capacity >= min_capacity) {
    // already big enough
    return 0;
  }

  // compute the new capacity
  size_t new_cap = bp_capacity ? bp_capacity * 2 : 4;
  if(new_cap < min_capacity) new_cap = min_capacity;

  // resize
  breakpoint_t *tmp = realloc(breakpoints, new_cap + sizeof(*breakpoints));
  if(!tmp) {
    // allocation failed, leave old pointers/capacity intact
    return -1;
  }

  breakpoints = tmp;
  bp_capacity = new_cap;

  return 0;
}

// some steps to consider
// - check for duplicate breakpoints, we dont want a breakpoint installed at the same addr twice
// - we need to make room, so we call ensure_capacity
// - append a new entry at the end
// - increment bp_count
int add_breakpoint(uint64_t addr) {
  // check duplicates
  for(size_t i = 0; i < bp_count; i++) {
    if(breakpoints[i].addr == addr) {
      // already have this addr
      return -1;
    }
  }

  // grow array if needed
  if(ensure_capacity(bp_count + 1) != 0) {
    return -1; // allocation err
  }

  // append new entry
  breakpoints[bp_count].index = (int)bp_count;
  breakpoints[bp_count].addr = addr;
  kern_return_t kr = mach_set_breakpoint((int)bp_count, addr);
  if(kr != KERN_SUCCESS) {
    printf("dunno what to tell you man\n");
    return -1;
  }

  return (int)(bp_count++);
}

// slide everything after idx down
int remove_breakpoint_at_index(size_t idx) {
  if(idx >= bp_count) {
    return -1;
  }
  for(size_t j = idx + 1; j < bp_count; j++) {
    breakpoints[j - 1] = breakpoints[j];
    breakpoints[j - 1].index = (int)(j - 1);
  }
  bp_count--;

  kern_return_t kr = mach_remove_breakpoint((int)idx);
  if(kr != KERN_SUCCESS) {
    printf("removing breakpoint failed\n");
    return -1;
  }

  return 0;
}

int remove_breakpoint_by_addr(uint64_t addr) {
  for(size_t i = 0; i < bp_count; i++) {
    if(breakpoints[i].addr == addr) {
      // found
      return remove_breakpoint_at_index(i);
    }
  }
  return -1;
}

void list_breakpoints(void) {
  printf("[i] currently %zu breakpoints:\n", bp_count);
  for(size_t i = 0; i < bp_count; i++) {
    // thx gpt i dont like formatting
    printf("  [%2d] @ 0x%016" PRIx64 "\n",
               breakpoints[i].index,
               breakpoints[i].addr);
  }
  return;
}
