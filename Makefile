# Makefile for phantom embedding Info.plist and codesigning

# Compiler and flags
CC       = gcc
CFLAGS   = -Iinclude -Wall -Wextra -O2

# Info.plist and section flags
PLIST    = Info.plist
SECTFLAG = -Wl,-sectcreate,__TEXT,__info_plist,$(PLIST)

# Sources and targets
SRCS     = main.c src/mach_vm_helper.c src/shell.c src/debugger.c
OBJS     = $(SRCS:.c=.o)
TARGET   = phantom

.PHONY: all clean run test

# Default target: build, embed Info.plist, then codesign
all: $(TARGET)

# Link and embed, then codesign with entitlements
$(TARGET): $(OBJS) $(PLIST)
	@echo "Linking with embedded Info.plist…"
	$(CC) $(CFLAGS) $(SECTFLAG) $(OBJS) -o $@
	@echo "Codesigning $(TARGET) with entitlements from $(PLIST)…"
	codesign --force --sign - \
	         --entitlements $(PLIST) \
	         $@

# Compile .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Run a test target in parallel
run: $(TARGET)
	./test_proc/test_proc & \
	./$(TARGET)

# Build the test helper
test:
	gcc test_proc/test_proc.c -o test_proc/test_proc

# Clean up
clean:
	rm -f $(OBJS) $(TARGET)

