# Makefile for phantom embedding Info.plist

# Compiler and flags
CC       = gcc
CFLAGS   = -Iinclude -Wall -Wextra -O2

# Info.plist section embedding
PLIST    = Info.plist
SECTION  = -sectcreate __TEXT __info_plist $(PLIST)

# Sources and target
SRCS     = main.c src/mach_vm_helper.c src/shell.c src/debugger.c
OBJS     = $(SRCS:.c=.o)
TARGET   = phantom

# Default target: build with embedded Info.plist
all: $(TARGET)

# Link object files, embedding Info.plist for task_for_pid
$(TARGET): $(OBJS) $(PLIST)
	$(CC) $(CFLAGS) -sectcreate __TEXT __info_plist $(PLIST) $(OBJS) -o $@

# Compile .c to .o (works for files in src/ as well) .c to .o (works for files in src/ as well)
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

