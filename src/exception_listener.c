#include "exception_listener.h"
#include "mach_exc.h"
#include <mach/mach_error.h>
#include <mach/message.h>
#include <stdio.h>
#include <stdlib.h>

extern kern_return_t mach_exc_server(mach_msg_header_t *InHeadP,
                                     mach_msg_header_t *OutHeadP);

// enough size for any exception message
#define EXC_MSG_BUF_SIZE (sizeof(mach_msg_header_t) + 1024)

// thread entry: each msg comes in on exc_port, we dispatch it
void *exception_listener(void *arg) {
  mach_port_t exc_port = *(mach_port_t *)arg;

  union __RequestUnion__mach_exc_subsystem in_msg; // holds largest request
  union __ReplyUnion__mach_exc_subsystem out_msg;

  for (;;) {
    // 1) receive an exception request
    kern_return_t kr = mach_msg(
        (mach_msg_header_t *)&in_msg, MACH_RCV_MSG | MACH_RCV_LARGE, 0,
        sizeof(in_msg), exc_port, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

    if (kr == MACH_RCV_INVALID_NAME || kr == MACH_RCV_PORT_CHANGED) {
      fprintf(
          stderr,
          "\n[i] exception_listener: receive port destroyed (%s) (detach)\n",
          mach_error_string(kr));
      printf("phantom> ");
      fflush(stdout);
      break;
    }
    if (kr != KERN_SUCCESS) {
      fprintf(stderr, "[-] exception_listener: mach_msg receive failed: %s\n",
              mach_error_string(kr));
      printf("phantom> ");
      fflush(stdout);
      continue;
    }

    // 2) dispatch to handlers
    boolean_t handled = mach_exc_server((mach_msg_header_t *)&in_msg,
                                        (mach_msg_header_t *)&out_msg);
    if (!handled) {
      fprintf(stderr, "[-] exception_listener: dispatch failed\n");
      printf("phantom> ");
      fflush(stdout);
      continue;
    }

    // 3) send the reply back to the kernel/faulting thread
    kr = mach_msg((mach_msg_header_t *)&out_msg, MACH_SEND_MSG,
                  out_msg.Reply_mach_exception_raise.Head.msgh_size, 0,
                  MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    if (kr != KERN_SUCCESS) {
      fprintf(stderr, "[-] exception_listener: mach_msg send failed: %s\n",
              mach_error_string(kr));
    }
  }

  return NULL;
}
