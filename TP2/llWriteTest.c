#include "llAPI.h"

#define FLAG 0x7E
#define ESC 0x7D

int main() {
    char port[] = "/dev/ttyS0";
    char msg[] = "qwertyABCDE 012345";
    int fd = llopen(port, TRANSMITTER);
    int bytesWritten = llwrite(fd, msg, strlen(msg));
    printf("bytesWritten = %d\n", bytesWritten);
}
