/*Non-Canonical Input Processing*/

#include "appAPI.h"

#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

int main(int argc, char** argv) {
    if (argc < 2) {
      printf("Usage:\tSerialPort FileName\n");
      printf("\tex: /dev/ttyS0 image.png\n");
      exit(1);
    }

  	if (appWrite(argv[1], argv[2]) == -1) {
      printf("appWrite() failed.\n");
      return -1;
    }

    return 0;
}
