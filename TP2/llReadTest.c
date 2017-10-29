#include "llAPI.h"

int main() {
  char port[] = "/dev/ttyS0";

  int fd = llopen(port, RECEIVER);
  printf("fd = %d\n", fd);
}
