#include "llAPI.h"

#define FLAG 0x7E
#define ESC 0x7D

int main() {
  setbuf(stdout, NULL);
  char port[] = "/dev/ttyS0";

  int msgSize = sizeof("abc") + 3;
  char *msg = malloc(msgSize);
  msg[0] = FLAG;
  strncpy(msg + 1, "abc", sizeof("abc")); //sizeof includes \0
  msg[5] = ESC;
  msg[6] = FLAG;
  // int fd = llopen(port);
  // int bytesWritten = llwrite(fd, msg, strlen(msg));
  // printf("Bytes written = %d\n", bytesWritten);

  for (int i = 0; i < msgSize; i++) {
      printf("%x ", msg[i]);
  }
  printf("\n");

  char *stuffedMsg = NULL;
  int stuffedMsgLength = -1;
  stuffPacket(msg, msgSize, &stuffedMsg, &stuffedMsgLength);
  for (int i = 0; i < stuffedMsgLength; i++) {
      printf("%x ", stuffedMsg[i]);
  }
  printf("\n");
  printf("stuffedMsgLength = %d\n", stuffedMsgLength);

  char *res = NULL;
  int resLength = -1;
  unstuffPacket(stuffedMsg, stuffedMsgLength, &res, &resLength);
  for (int i = 0; i < resLength; i++) {
      printf("%x ", res[i]);
  }
  printf("\n");
  printf("resLength = %d\n", resLength);
}
