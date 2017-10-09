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
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP=FALSE;

#define FLAG 0x7E
#define A 0x03
#define C_SET 0x03
#define C_UA 0x07

#define TIMEOUT 3 //seconds

enum State {
	S1,
	S2,
	S3,
	END_READ
};

static bool timedOut = false;

void sigAlarmHandler(int sig) {
	timedOut = true;
}

bool validBCC(char received[]) {
printf("%x%x%x\n", received[0],received[1],received[2]);
	return (received[2] == (received[0] ^ received[1]));
}

int llopen(int fd) {
	bool end = false;
	int setMsgSize = 5;
	char ua_msg[setMsgSize];

	bzero(ua_msg, setMsgSize);

	ua_msg[0] = FLAG;
	ua_msg[1] = A;
	ua_msg[2] = C_UA;
	ua_msg[3] = A^C_UA;
	ua_msg[4] = FLAG;


		enum State state = S1;
	
		char buf[1];
		buf[0] = 0;
		int res = -1;
		char received[3];
		int ind = 0;
		while (((res = read(fd,buf,1)) != -1) && end==false) {
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
				received[ind] = buf[0];

				if (ind == 3) {
					if (!validBCC(received)) {
						printf("llopen(): invalid SET\n");
						return -1;
					}
				}
				ind++;
				break;
			case END_READ:
				printf("llopen(): received SET\n");
				end =true;
				break;
			}
		}
	printf("sending UA\n");
	write(fd, ua_msg, setMsgSize);
	printf("llopen Success\n");
	return 0;
}

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

    llopen(fd);



  /* 
    O ciclo WHILE deve ser alterado de modo a respeitar o indicado no guião 
  */
   


    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
