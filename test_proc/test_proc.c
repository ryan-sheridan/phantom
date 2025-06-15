#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("PID %d: will crash in 15 seconds…\n", getpid());
    fflush(stdout);

    sleep(15);

    // Deliberately crash: write to address 0
    volatile int *p = NULL;
    *p = 42;

    // Should never reach here
    return 0;
}

