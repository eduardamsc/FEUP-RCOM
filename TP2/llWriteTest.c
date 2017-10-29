#include "llAPI.h"

#define FLAG 0x7E
#define ESC 0x7D

int main() {
    char port[] = "/dev/ttyS0";
    int msgSize = 12;
  //  char msg[12] = {FLAG, ESC, 'a', 0x5, '0', ESC, 0, FLAG, 5, ESC, FLAG, ESC};
    char msg[12] = {ESC, 'a', 0x5, '0', 0, 5};
    int fd = llopen(port, TRANSMITTER);
    int bytesWritten = llwrite(fd, msg, msgSize);
    printf("bytesWritten = %d\n", bytesWritten);
}
