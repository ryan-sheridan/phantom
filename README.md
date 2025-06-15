# phantom

`phantom` is a lightweight debugger for Darwin (macOS) systems. It allows users to attach to running processes, interrupt their execution, read and write register values, and resume or detach from the process. It provides a basic command-line interface for interacting with the target process, leveraging Mach APIs for process control and exception handling.

## Features

* **Attach**: Attaches to a process using its PID or name.
* **Interrupt**: Suspends the execution of the attached process.
* **Resume**: Resumes the execution of the attached process.
* **Register Manipulation**: Reads and writes register values of the attached process.
* **Detach**: Detaches from the attached process.
* **Exception Handling**: Catches exceptions in the target process using Mach exception ports.

## File Structure

```
└── ryan-sheridan-phantom/
    ├── README.md           # This file
    ├── Info.plist          # Property list file for entitlements
    ├── Makefile            # Build instructions
    ├── main.c              # Entry point for the debugger
    ├── include/            # Header files
    │   ├── debugger.h      # Debugger function declarations
    │   ├── mach_exc.h      # Mach exception handling definitions
    │   ├── mach_process.h  # Mach process related function declarations
    │   └── shell.h         # Shell interface definitions
    ├── src/                # Source files
    │   ├── debugger.c         # Debugger functions implementation
    │   ├── exception_listener.c # Exception listener thread
    │   ├── handlers.c         # Exception handlers
    │   ├── mach_exc.defs      # Mach exception definitions (MIG)
    │   ├── mach_excServer.c   # Mach exception server (generated)
    │   ├── mach_excUser.c     # Mach exception user (generated)
    │   ├── mach_process.c     # Mach process related functions implementation
    │   └── shell.c            # Shell interface implementation
    └── test_proc/           # Test process
        └── test_proc.c      # Simple program to test the debugger
```

## Building

To build the project, run:

```bash
make
```

This command will compile the source files, embed the `Info.plist` for code signing entitlements, and codesign the resulting binary.

## Running

To run the debugger against the test process, use:

```bash
make run
```

This will first compile and run the test process (`test_proc/test_proc.c`) in the background and then start the debugger.

## Commands

* `help`: Shows a list of available commands.
* `attach <pid|name>`: Attaches to a process by its PID or name.
* `resume`: Resumes the execution of the attached process.
* `suspend`: Suspends the execution of the attached process.
* `detach`: Detaches from the attached process.
* `reg read`: Reads and prints the register values of the attached process.
* `reg write <reg> <value>`: Writes a value to a specific register.
* `q`: Exits the debugger.

## Code Overview

* `main.c`: Contains the main function, which initializes and starts the shell loop.
* `shell.c`: Implements the command-line interface, including command parsing and dispatching.
* `debugger.c`: Provides the core debugging functions such as attaching, detaching, interrupting, resuming, and register manipulation.
* `mach_process.c`: Contains functions for interacting with Mach APIs to control processes and set up exception ports.
* `exception_listener.c`: Implements a thread that listens for exceptions from the target process.
* `handlers.c`: Contains placeholder functions for handling specific exceptions (currently not fully implemented).
* `mach_exc*.c`: These files define the Mach exception interface using MIG (Mach Interface Generator).

## Entitlements

The `Info.plist` file sets the following entitlements:

* `com.apple.security.get-task-allow`: Allows the debugger to access the task port of other processes.
* `com.apple.security.cs.debugger`: Allows the debugger to attach to and debug other processes.

These entitlements are necessary for debugging and must be properly codesigned.

## Future Improvements

* Implement more comprehensive exception handling in `handlers.c`.
* Add support for setting breakpoints and stepping through code.
* Implement memory read/write capabilities.
* Improve the command-line interface with features like tab completion and history.
