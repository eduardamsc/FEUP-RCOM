/*Non-Canonical Input Processing*/

#include "llAPI.h"

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
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

    int fd;
  	char msg[] = "ab";

  	if ((fd = llopen(argv[1])) == -1) {
  		printf("llopen() failed\n");
  		exit(-1);
  	}

  	if (-1 == llwrite(fd, msg, strlen(msg))) {
  		printf("llwrite() failed\n");
  		exit(-1);
  	}

  	if (-1 == llclose_Transmitter(fd)) {
  		printf("llclose_Transmitter() failed\n");;
  		exit(-1);
  	}




    close(fd);
    return 0;
}
