#include "dbg/bp_wp.h"
#include "mach/mach_process.h"
#include <inttypes.h>
#include <mach/kern_return.h>
#include <stdio.h>
#include <stdlib.h>

// ensure dynamic array capacity for breakpoints
static int ensure_bp_capacity(size_t min_capacity) {
  if (bp_capacity >= min_capacity) return 0;

  size_t new_cap = bp_capacity ? bp_capacity * 2 : 4;
  if (new_cap < min_capacity) new_cap = min_capacity;

  breakpoint_t *tmp = realloc(breakpoints, new_cap * sizeof(*breakpoints));
  if (!tmp) return -1;

  breakpoints = tmp;
  bp_capacity = new_cap;
  return 0;
}

// ensure dynamic array capacity for watchpoints
static int ensure_wp_capacity(size_t min_capacity) {
  if (wp_capacity >= min_capacity) return 0;

  size_t new_cap = wp_capacity ? wp_capacity * 2 : 4;
  if (new_cap < min_capacity) new_cap = min_capacity;

  watchpoint_t *tmp = realloc(watchpoints, new_cap * sizeof(*watchpoints));
  if (!tmp) return -1;

  watchpoints = tmp;
  wp_capacity = new_cap;
  return 0;
}

// add breakpoint if not already set
int add_breakpoint(uint64_t addr) {
  for (size_t i = 0; i < bp_count; i++) {
    if (breakpoints[i].addr == addr) return -1;
  }

  if (ensure_bp_capacity(bp_count + 1) != 0) return -1;

  breakpoints[bp_count].index = (int)bp_count;
  breakpoints[bp_count].addr = addr;
  kern_return_t kr = mach_set_breakpoint((int)bp_count, addr);
  if (kr != KERN_SUCCESS) {
    printf("dunno what to tell you man\n");
    return -1;
  }

  return (int)(bp_count++);
}

// remove breakpoint by index
int remove_breakpoint_at_index(size_t idx) {
  if (idx >= bp_count) return -1;

  for (size_t j = idx + 1; j < bp_count; j++) {
    breakpoints[j - 1] = breakpoints[j];
    breakpoints[j - 1].index = (int)(j - 1);
  }
  bp_count--;

  kern_return_t kr = mach_remove_breakpoint((int)idx);
  if (kr != KERN_SUCCESS) {
    printf("removing breakpoint failed\n");
    return -1;
  }

  return 0;
}

// remove breakpopint by addr
int remove_breakpoint_by_addr(uint64_t addr) {
  for (size_t i = 0; i < bp_count; i++) {
    if (breakpoints[i].addr == addr) {
      return remove_breakpoint_at_index(i);
    }
  }
  return -1;
}

// add watchpoint if one doesnt already exist
int add_watchpoint(uint64_t addr) {
  for (size_t i = 0; i < wp_count; i++) {
    if (watchpoints[i].addr == addr) return -1;
  }

  if (ensure_wp_capacity(wp_count + 1) != 0) return -1;

  watchpoints[wp_count].index = (int)wp_count;
  watchpoints[wp_count].addr = addr;
  kern_return_t kr = mach_set_watchpoint((int)wp_count, addr);
  if (kr != KERN_SUCCESS) {
    printf("failed to set watchpoint\n");
    return -1;
  }

  return (int)(wp_count++);
}

// remove wp by index
int remove_watchpoint_at_index(size_t idx) {
  if (idx >= wp_count) return -1;

  for (size_t j = idx + 1; j < wp_count; j++) {
    watchpoints[j - 1] = watchpoints[j];
    watchpoints[j - 1].index = (int)(j - 1);
  }
  wp_count--;

  kern_return_t kr = mach_remove_watchpoint((int)idx);
  if (kr != KERN_SUCCESS) {
    printf("removing watchpoint failed\n");
    return -1;
  }

  return 0;
}

// remove wp by addr
int remove_watchpoint_by_addr(uint64_t addr) {
  for (size_t i = 0; i < wp_count; i++) {
    if (watchpoints[i].addr == addr) {
      return remove_watchpoint_at_index(i);
    }
  }
  return -1;
}

// list func
static void list_points(const char *label, size_t count, void *points, int is_watchpoint) {
  if (!count) {
    printf("0 %s set\n", label);
    return;
  }

  printf("[i] currently %zu %s:\n", count, label);
  for (size_t i = 0; i < count; i++) {
    if (is_watchpoint) {
      watchpoint_t *wp = &((watchpoint_t *)points)[i];
      printf("  [%2d] @ 0x%016" PRIx64 "\n", wp->index, wp->addr);
    } else {
      breakpoint_t *bp = &((breakpoint_t *)points)[i];
      printf("  [%2d] @ 0x%016" PRIx64 "\n", bp->index, bp->addr);
    }
  }
}

// wrap
void list_breakpoints(void) {
  list_points("breakpoints", bp_count, breakpoints, 0);
}

void list_watchpoints(void) {
  list_points("watchpoints", wp_count, watchpoints, 1);
}

