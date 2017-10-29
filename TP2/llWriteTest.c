#include "llAPI.h"

#define FLAG 0x7E
#define ESC 0x7D

int main() {
    char port[] = "/dev/ttyS0";
    int fd = llopen(port, TRANSMITTER);
    printf("fd = %d\n", fd);
}
