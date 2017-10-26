#include "llAPI.h"

int main() {
  char port[] = "/dev/ttyS0";

  char *msg;
  int fd = llopen_read(port);
  llread(fd, &msg);
  printf("%s\n", msg);
}
