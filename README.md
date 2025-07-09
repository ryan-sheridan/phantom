# phantom

> [!WARNING]  
> this can break things.

`phantom` is a lightweight debugger for macOS. It allows you to attach to running processes, inspect their state (registers), control their execution (resume, suspend, detach), and read/write process memory. It's built using Mach APIs for process control and exception handling. It also supports setting breakpoints to pause execution at specific addresses. Furthermore, phantom can read and write memory and handle ASLR slides.

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
    *   `br`: list, set or delete a breakpoint by address or index syntax: `br set <address>` | `br delete <address|index>` | `br list`
    *   `r64 <address>`: Read 64 bits from memory at the given address.
    *   `w64 <address> <value>`: Write a 64-bit value to memory at the given address.
    *   `r32 <address>`: Read 32 bits from memory at the given address.
    *   `w32 <address> <value>`: Write a 32-bit value to memory at the given address.
    *   `slide`: Print the ASLR slide value.
    *   `autoslide`: Toggle automatic ASLR slide calculation.
    *   `q`: Exit.

## Key Files

*   [main.c](https://github.com/ryan-sheridan/phantom/blob/main/main.c):  Entry point, initializes the shell.
*   [shell.c](https://github.com/ryan-sheridan/phantom/blob/main/src/shell.c): Command-line interface.
*   [debugger.c](https://github.com/ryan-sheridan/phantom/blob/main/src/debugger.c): Core debugging functions (attach, detach, etc.).
*   [mach_process.c](https://github.com/ryan-sheridan/phantom/blob/main/src/mach_process.c): Mach API interactions.

## Entitlements

`Info.plist` includes entitlements for debugging (`com.apple.security.get-task-allow`, `com.apple.security.cs.debugger`).  These are required for debugging and must be codesigned.

