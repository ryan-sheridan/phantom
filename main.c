#include "interface/shell.h"
#include "interface/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool verbose;

int main(int argc, char **argv) {
  verbose = false;
  if(argc > 1) {
    if(strcmp(argv[1], "-h") == 0) {
      printf("usage:\n\tphantom [-h help] [-v verbose]\n");
      return 0;
    }
    if(strcmp(argv[1], "-v") == 0) {
      verbose = true;
      LOG_INFO("verbose mode enabled\n");
    }
  }
  shell_loop();
  return 0;
}
