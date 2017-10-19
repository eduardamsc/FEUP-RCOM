/*Non-Canonical Input Processing*/

#include "llAPI.h"

#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

int main(int argc, char** argv) {
    if ( (argc < 2) ||
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) &&
  	      (strcmp("/dev/ttyS1", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }

    char *msg = NULL;
    int fd = llopen_read(argv[1]);
    llread(fd, msg);
    llclose_Receiver(fd);

    return 0;
}
