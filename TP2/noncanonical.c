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
    int fd;
    if ((fd = llopen_read(argv[1])) == -1) {
        printf("llopen() failed\n");
        exit(-1);
    }

    if (llread(fd, msg) == -1) {
        printf("llread() failed\n");
        exit(-1);
    }

    if (llclose_Receiver(fd) == -1) {
        printf("llclose_Receiver() failed\n");;
  		  exit(-1);
    }

    return 0;
}
