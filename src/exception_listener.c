#include <mach/message.h>
#include <mach/mach_error.h>
#include "mach_exc.h"
#include <stdio.h>

extern boolean_t mach_exc_server(
    mach_msg_header_t *InHeadP,
    mach_msg_header_t *OutHeadP);

// enough size for any exception message
#define EXC_MSG_BUF_SIZE (sizeof(mach_msg_header_t) + 1024)

// thread entry: each msg comes in on exc_port, we dispatch it
void *exception_listener(void *arg) {
  mach_port_t exc_port = *(mach_port_t *)arg;
  char buffer[EXC_MSG_BUF_SIZE];

  for(;;) {
    mach_msg_header_t *msg = (mach_msg_header_t *) buffer;
    kern_return_t kr = mach_msg(
            msg,
            MACH_RCV_MSG | MACH_RCV_LARGE,
            0,
            EXC_MSG_BUF_SIZE,
            exc_port,
            MACH_MSG_TIMEOUT_NONE,
            MACH_PORT_NULL
        );
        if(kr == MACH_RCV_INVALID_NAME ||
           kr == MACH_RCV_PORT_CHANGED) {
          fprintf(stderr, "\n[-] exception_listener: receive port destroyed (%s)\n",
                    mach_error_string(kr));
          break;
        }
        if (kr != KERN_SUCCESS) {
            fprintf(stderr, "[-] mach_msg receive failed: %s\n",
                    mach_error_string(kr));
            continue;
        }

        // dispatch to handlers
        boolean_t handled = mach_exc_server(msg, msg);
        if(!handled) {
          fprintf(stderr, "[-] Exception message not handled\n");
        }

  }

  return 0;
}
