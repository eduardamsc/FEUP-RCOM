/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdbool.h>
#include "llAPI.h"

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

// #define FLAG 0x7E
// #define A_CMD 0x01
// #define A_RESP 0x03
// #define C_I 0x0
// #define C_SET 0x03
// #define C_UA 0x07
// #define C_RR 0x05
// #define C_REJ 0x01
// #define C_DISC 0xB
// #define ESC 0x7D

// S and U frames
// A | C | BCC
// #define A_IND_RESP 0
// #define C_IND_RESP 1
// #define BCC_IND_RESP 2

// #define I_FRAMES_SEQ_NUM_BIT(x) (x >> 7)
// #define S_U_FRAMES_SEQ_NUM_BIT(x) (x >> 8)
//
// #define TIMEOUT 3 //seconds



int main(int argc, char** argv)
{
    int fd;
    struct termios oldtio,newtio;

    if ( (argc < 2) ||
  	     ((strcmp("/dev/ttyS0", argv[1])!=0) &&
  	      (strcmp("/dev/ttyS1", argv[1])!=0) )) {
      printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
      exit(1);
    }


  /*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */


    fd = open(argv[1], O_RDWR | O_NOCTTY );
    if (fd <0) {perror(argv[1]); exit(-1); }

    if ( tcgetattr(fd,&oldtio) == -1) { /* save current port settings */
      perror("tcgetattr");
      exit(-1);
    }

    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME]    = 1;   /* inter-character timer unused (em 100 ms)*/
    newtio.c_cc[VMIN]     = 0;   /* blocking read until 0 chars received */

    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");

    char *msg = NULL;
    llopen_read(fd);
    llread(fd, msg);

    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
