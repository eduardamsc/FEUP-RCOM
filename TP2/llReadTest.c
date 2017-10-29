#include "llAPI.h"

int main() {
  char port[] = "/dev/ttyS0";
  int fd = llopen(port, RECEIVER);
  if (fd == -1) {
    printf("llopen failed.\n");
  }

  char *msg = NULL;
  int msgSize = llread(fd, &msg);
  printf("Data size: %d | Data received: ", msgSize);
  for (int i = 0; i < msgSize; i++) {
    printf("%x ", msg[i]);
  }
  printf("\n");
}
