#include <stdio.h>
#include <unistd.h>    // for sleep()

int main(void) {
    unsigned long counter = 0;

    while (1) {
        printf("%lu\n", counter++);
        fflush(stdout); // ensure it's printed immediately
        sleep(1);       // pause for 1 second
    }

    return 0; // never reached
}

