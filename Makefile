# Makefile for phantom embedding Info.plist and codesigning

# Compiler and flags
CC        = gcc
# Add -I/opt/homebrew/include for Apple Silicon, or -I/usr/local/include for Intel Macs
# Also, include -lcapstone in LDFLAGS to link the library
CFLAGS = -std=gnu11 -Iinclude -D_DARWIN_C_SOURCE -DDEV \
         -Wall -Wextra -Wstrict-prototypes -O2 -pthread

# Auto-detect Homebrew prefix for include and lib paths
# For Apple Silicon, it's typically /opt/homebrew
# For Intel Macs, it's typically /usr/local
HOMEBREW_PREFIX := $(shell brew --prefix)
# Add Capstone include path and link flags
CFLAGS += -I$(HOMEBREW_PREFIX)/include
LDFLAGS = -L$(HOMEBREW_PREFIX)/lib -lcapstone

# Info.plist and section flags
PLIST     = Info.plist
SECTFLAG  = -Wl,-sectcreate,__TEXT,__info_plist,$(PLIST)

# Sources and targets
SRCS      := main.c $(wildcard src/*/*.c)
OBJS      = $(SRCS:.c=.o)
TARGET    = phantom

.PHONY: all clean run test

# Default target: build, embed Info.plist, then codesign
all: $(TARGET)

$(TARGET): $(OBJS) $(PLIST)
	@echo "Linking with embedded Info.plist…"
	$(CC) $(CFLAGS) $(SECTFLAG) $(OBJS) $(LDFLAGS) -o $@  # Added $(LDFLAGS) here
	@echo "Codesigning $(TARGET) with entitlements from $(PLIST)…"
	codesign --force --sign - \
	         --entitlements $(PLIST) \
	         $@

# Compile .c to .o
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build the test helper
test:
	gcc -O0 -fno-pie -Wl,-no_pie test_proc/test_proc.c -o test_proc/test

# Run phantom against the test helper
run: test $(TARGET)
	./test_proc/test_proc & \
	./$(TARGET)

# Clean up
clean:
	rm -f $(OBJS) $(TARGET)
