# phantom

`phantom` is a lightweight debugger for macOS. It allows you to attach to running processes, inspect their state (registers), and control their execution (resume, suspend, detach). It's built using Mach APIs for process control and exception handling.  It also supports setting breakpoints to pause execution at specific addresses.

## Usage

1.  **Build:** `make`
2.  **Run:** `make run` (This compiles and runs a test program, then starts the debugger).
3.  **Commands:**
    *   `attach <pid|name>`: Attach to a process.
    *   `resume`: Resume execution.
    *   `suspend`: Suspend execution.
    *   `detach`: Detach from the process.
    *   `reg read`: Read register values.
    *   `reg write <reg> <value>`: Write to a register.
    *   `br`: list, set or delete a breakpoint by address or index syntax: br set <address> | br delete <address|index> | br list
    *   `q`: Exit.

## Key Files

*   [main.c](https://github.com/ryan-sheridan/phantom/blob/main/main.c):  Entry point, initializes the shell.
*   [shell.c](https://github.com/ryan-sheridan/phantom/blob/main/src/shell.c): Command-line interface.
*   [debugger.c](https://github.com/ryan-sheridan/phantom/blob/main/src/debugger.c): Core debugging functions (attach, detach, etc.).
*   [mach_process.c](https://github.com/ryan-sheridan/phantom/blob/main/src/mach_process.c): Mach API interactions.

## Entitlements

`Info.plist` includes entitlements for debugging (`com.apple.security.get-task-allow`, `com.apple.security.cs.debugger`).  These are required for debugging and must be codesigned.

