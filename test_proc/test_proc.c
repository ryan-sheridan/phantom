#include <unistd.h>
#include <stdint.h>

static const char hidden_str[] = "This is the secret string!";

int main(void) {
    __asm__ volatile (
        "ldr x8, %0\n"
        :
        : "m"(hidden_str)
        : "x8"
    );

    for (;;) {
        sleep(1);
    }
    return 0;
}

