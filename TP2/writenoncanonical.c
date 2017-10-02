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

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define FLAG 0x7E
#define A 0x03
#define C_SET 0x03
#define C_UA 0x07

enum State {
	S1,
	S2,
	S3,
	END_READ
};

bool validBCC(char response[]) {
	return (response[2] == (response[0] ^ response[1]));
}

int llopen(int fd) {
	enum State state = S1;
	int setMsgSize = 5;
	char set_msg[setMsgSize];

	set_msg[0] = FLAG;
	set_msg[1] = A;
	set_msg[2] = C_SET;
	set_msg[3] = A^C_SET;
	set_msg[4] = FLAG;

	write(fd, set_msg, setMsgSize);

	char buf[1];
	buf[0] = 0;
	int res = -1;
	char response[3];
	int ind = 0;
	while ((res = read(fd,buf,1)) != -1) {
		switch (state) {
		case S1:
			if (res != 0 && buf[0] == FLAG) {
				state = S2;
			}
			break;
		case S2:
			if (res != 0 && buf[0] != FLAG) {
				state = S3;
			}
			break;
		case S3:
			if (res != 0 && buf[0] == FLAG) {
				state = END_READ;
			}
			response[ind] = buf[0];
			if (ind == 3) {
				if (!validBCC(response)) {
					return -1;
				}
			}
			ind++;
			break;
		case END_READ:
			break;
		}
	}
	
	return 0;
}

volatile int STOP=FALSE;

int main(int argc, char** argv)
{
    int fd, res;
    struct termios oldtio,newtio;
    char buf[255];
    
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

    newtio.c_cc[VTIME]    = 1;
    newtio.c_cc[VMIN]     = 0;



  /* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) próximo(s) caracter(es)
  */



    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");

	/********************/
	
	llopen(fd);	

  /* 
    O ciclo FOR e as instruções seguintes devem ser alterados de modo a respeitar 
    o indicado no guião 
  */



   
    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

	


    close(fd);
    return 0;
}
