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
#define A_CMD 0x01
#define A_RESP 0x03
#define C_I 0x0
#define C_SET 0x03
#define C_UA 0x07
#define C_RR 0x05
#define C_REJ 0x01
#define C_DISC 0xB
#define ESC 0x7D

// S and U frames
// A | C | BCC
#define A_IND_RESP 0
#define C_IND_RESP 1
#define BCC_IND_RESP 2

#define I_FRAMES_SEQ_NUM_BIT(x) (x >> 7)
#define S_U_FRAMES_SEQ_NUM_BIT(x) (x >> 8)

#define TIMEOUT 3 //seconds

enum LLOpenState {
	O_S1,
	O_S2,
	O_S3,
	O_END_READ
};

enum TransfState {
	T_S1,
	T_S2,
	T_S3,
	T_S4,
	T_FOUND_ESC,
	T_END_READ
};

static bool timedOut = false;

void sigAlarmHandler(int sig) {
	timedOut = true;
}

bool validBCC1(char received[]) {
	return (received[2] == (received[0] ^ received[1]));
}

bool validBCC2(char BCC1, char* buffer, int bufferLength, char BCC2) {
	char aux=BCC1;
	printf("BCC1 %x\n", BCC1);
	for (int i=0; i<bufferLength; i++) {
		aux^=buffer[i];
	printf("buffer[i] %x\n", buffer[i]);
	}

	return aux==BCC2;
}

int llopen(int fd) {
	bool end = false;
	int setMsgSize = 5;
	char ua_msg[setMsgSize];

	bzero(ua_msg, setMsgSize);

	ua_msg[0] = FLAG;
	ua_msg[1] = A_RESP;
	ua_msg[2] = C_UA;
	ua_msg[3] = A_RESP^C_UA;
	ua_msg[4] = FLAG;


	enum LLOpenState state = O_S1;
	char buf[1];
	buf[0] = 0;
	int res = -1;
	char received[3];
	int ind = 0;
	while (((res = read(fd,buf,1)) != -1) && end==false) {
		switch (state) {
		case O_S1:
			if (res != 0 && buf[0] == FLAG) {
				state = O_S2;
			}
			break;
		case O_S2:
			if (res != 0 && buf[0] != FLAG) {
				state = O_S3;
				received[ind++] = buf[0];
			}
			break;
		case O_S3:
			if (res != 0 && buf[0] == FLAG) {
				state = O_END_READ;
			}
			received[ind] = buf[0];

			if (ind == 3) {
				if (!validBCC1(received)) {
					printf("llopen(): invalid SET\n");
					return -1;
				}
			}
			ind++;
			break;
		case O_END_READ:
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

int sendRejection(int fd) {
	int responseSize = 5;
	char response[responseSize];

	bzero(response, responseSize);

	response[0] = FLAG;
	response[1] = A_RESP;
	response[2] = C_REJ;
	response[3] = A_RESP^C_REJ;
	response[4] = FLAG;

	if (write(fd, response, responseSize) == -1) {
		printf("sendRejection(): write failed\n");
		return -1;
	}

	return 0;
}

int unstuffPacket(char* stuffedPacket, int stuffedPacketLength, char** buffer, int* bufferLength) {
printf("stuffedPacketLength %x\n",stuffedPacketLength);
	*buffer = realloc(*buffer, stuffedPacketLength);
	if (*buffer == NULL) {
		printf("unstuffPacket(): first realloc() failed\n");
		return -1;
	}

	for (int stuffedInd = 0, bufferInd = 0; bufferInd < stuffedPacketLength; stuffedInd++, bufferInd++) {
		if (stuffedPacket[stuffedInd] == ESC) {

			if (stuffedPacket[stuffedInd+1] == 0x5e) {
				(*buffer)[bufferInd] = FLAG;

				stuffedInd++;
			}

			if (stuffedPacket[stuffedInd+1] == 0x5d) {
				(*buffer)[bufferInd] = ESC;

				stuffedInd++;
			}
		} else {

			(*buffer)[bufferInd] = stuffedPacket[stuffedInd];
		}

		(*bufferLength)++;
	}

	*buffer = realloc(*buffer, *bufferLength);

	return 0;
}

int sendReady(int fd) {
	int responseSize = 5;
	char response[responseSize];

	bzero(response, responseSize);

	response[0] = FLAG;
	response[1] = A_RESP;
	response[2] = C_RR;
	response[3] = A_RESP^C_RR;
	response[4] = FLAG;

	if (write(fd, response, responseSize) == -1) {
		printf("sendReady(): write failed\n");
		return -1;
	}

	return 0;
}

int llread(int fd, char *buffer) {
	bool end = false;
	int bufferLength = 0, stuffedPacketLength = 0;
	char *stuffedPacket = NULL;

	enum TransfState state = T_S1;
	char buf[1];
	buf[0] = 0;
	int res = -1;
	char received[3];
	int ind = 0;

	while (((res = read(fd,buf,1)) != -1) && end==false) {
		printf("llread(): Read byte %x\n", buf[0]);
		switch (state) {
		case T_S1:
			if (res != 0 && buf[0] == FLAG) {
				state = T_S2;
			}
			break;
		case T_S2:
			if (res != 0 && buf[0] != FLAG) {
				state = T_S3;
				received[ind++] = buf[0];
			}
			break;
		case T_S3:
			if (res != 0) {
				received[ind] = buf[0];
				if (ind == 3) {
					if (!validBCC1(received)) {
						printf("llread(): invalid SET\n");
						return -1;
					}
					ind = 0;
					state = T_S4;
					break;
				}
				ind++;
			}
			break;
		case T_S4:
			if (res != 0) {
				if (buf[0] == ESC) {
					state = T_FOUND_ESC;
				} else if (buf[0] == FLAG) {
					state = T_END_READ;
				} else {
					stuffedPacketLength++;
					stuffedPacket = realloc(stuffedPacket, stuffedPacketLength);
					stuffedPacket[stuffedPacketLength - 1] = buf[0];
				}
			}
			break;
		case T_FOUND_ESC:
			if (res != 0) {
				stuffedPacketLength++;
				stuffedPacket = realloc(stuffedPacket, stuffedPacketLength);
				stuffedPacket[stuffedPacketLength - 1] = buf[0];
				state = T_S4;
			}
			break;
		case T_END_READ:
			printf("llread(): received I frame\n");
			end = true;
			break;
		}
	}

	char BCC2 = stuffedPacket[stuffedPacketLength - 2];
	stuffedPacket = realloc(stuffedPacket, stuffedPacketLength - 2);
	stuffedPacketLength -= 2;

	unstuffPacket(stuffedPacket, stuffedPacketLength, &buffer, &bufferLength);
printf("bl %x\n",bufferLength);

	if (!validBCC2(stuffedPacket[3], buffer, bufferLength, BCC2)) {
		printf("llread(): Invalid BCC2, sending REJ \n");
		if (sendRejection(fd) == -1) {
			printf("llread(): sendRejection error\n");
			return -1;
		}
	} else {
		printf("llread(): Sending RR\n");
		if (sendReady(fd) == -1) {
			printf("llread(): sendReady error\n");
			return -1;
		}
		printf("llread(): Success\n");
	}

	return bufferLength;
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


    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");

    char *msg = NULL;
    llopen(fd);
    llread(fd, msg);


    tcsetattr(fd,TCSANOW,&oldtio);
    close(fd);
    return 0;
}
