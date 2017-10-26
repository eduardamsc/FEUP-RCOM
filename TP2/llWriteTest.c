#include "llAPI.h"

int main() {
  char port[] = "/dev/ttyS0";

  char msg[] = "abc";
  int fd = llopen(port);
  int bytesWritten = llwrite(fd, msg, strlen(msg));
  printf("Bytes written = %d\n", bytesWritten);
}
