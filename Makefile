# Makefile for phantom embedding Info.plist and codesigning

# Compiler and flags
CC       = gcc
CFLAGS   = -Iinclude -Wall -Wextra -O2 -pthread

# Info.plist and section flags
PLIST    = Info.plist
SECTFLAG = -Wl,-sectcreate,__TEXT,__info_plist,$(PLIST)

# Sources and targets
SRCS     = main.c \
           src/mach_process.c \
           src/shell.c \
           src/debugger.c \
           src/exception_listener.c \
           src/handlers.c \
           src/mach_excServer.c \
           src/mach_excUser.c

OBJS     = $(SRCS:.c=.o)
TARGET   = phantom

.PHONY: all clean run test

# Default target: build, embed Info.plist, then codesign
all: $(TARGET)

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

# Build the test helper
test:
	gcc test_proc/test_proc.c -o test_proc/test_proc

# Run phantom against the test helper
run: test $(TARGET)
	./test_proc/test_proc & \
	./$(TARGET)

# Clean up
clean:
	rm -f $(OBJS) $(TARGET)

