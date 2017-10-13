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
#include <signal.h>

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

//TODO - Ns and Nr.
#define FLAG 0x7E
#define A 0x03
#define C_I 0x0
#define C_SET 0x03
#define C_UA 0x07
#define C_RR 0x05
#define C_REJ 0x01
#define C_DISC 0xB
#define ESC 0x7D

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
	printf("alarm, timedOut = %d\n", timedOut);
}

bool validBCC(char response[]) {
	return (response[2] == (response[0] ^ response[1]));
}

bool rejected(char response[]) {
	return (response[1] == C_REJ);	
}

int llopen(int fd) {
	signal(SIGALRM, sigAlarmHandler);
	int setMsgSize = 5;
	char set_msg[setMsgSize];
	bzero(set_msg, setMsgSize);

	set_msg[0] = FLAG;
	set_msg[1] = A;
	set_msg[2] = C_SET;
	set_msg[3] = A^C_SET;
	set_msg[4] = FLAG;

	do {
		timedOut = false;
		bool endRead = false;
		enum State state = S1;
		printf("llopen(): Sending SET\n");
		write(fd, set_msg, setMsgSize);
		alarm(3);

		char buf[1];
		buf[0] = 0;
		int res = -1;
		char response[3];
		int ind = 0;
		while (!endRead && !timedOut && (res = read(fd,buf,1)) != -1) {
			printf("buf[0] = %x\n", buf[0]);
			switch (state) {
			case S1:
				printf("In S1\n");
				if (res != 0 && buf[0] == FLAG) {
					state = S2;
				}
				break;
			case S2:
				printf("In S2\n");
				if (res != 0 && buf[0] != FLAG) {
					state = S3;
				}
				break;
			case S3:
				printf("In S3\n");
				if (res != 0 && buf[0] == FLAG) {
					state = END_READ;
				}
				response[ind] = buf[0];
				if (ind == 3) {
					if (!validBCC(response)) {
						printf("llopen(): Invalid UA\n");
						return -1;
					}
				}
				ind++;
				break;
			case END_READ:
				printf("llopen(): Received UA\n");
				endRead = true;
				break;
			}
		}
	} while (timedOut);

	printf("llopen(): Success\n");

	return 0;
}

int stuffPacket(char packet[], int packetLength, char stuffedPacket[], int *stuffedPacketLength) {
	stuffedPacket = realloc(stuffedPacket, packetLength);
	if (stuffedPacket == NULL) {
		printf("stuffPacket(): first realloc() failed\n");
		return -1;
	}
	for (int stuffedInd = 0, unstuffedInd = 0; unstuffedInd < packetLength; stuffedInd++, unstuffedInd++) {
		if (packet[unstuffedInd] == FLAG || packet[unstuffedInd] == ESC) {
			stuffedPacket[stuffedInd++] = ESC;
			*stuffedPacketLength++;
			stuffedPacket = realloc(stuffedPacket, *stuffedPacketLength);
		}
		stuffedPacket[stuffedInd] = packet[unstuffedInd];
	}
	return 0;
}

char calculateBCC2(char BCC1, char stuffedPacket[], int stuffedPacketLength) {
	int res = 0;
	for (int ind = 0; ind < stuffedPacketLength; ind++) {
		res ^= stuffedPacket[ind];
	}
	return BCC1 ^ res;
}

int makeFrame(char stuffedPacket[], int stuffedPacketLength, char frame[], int *frameLength) {
	*frameLength = stuffedPacketLength + 6;
	frame = malloc(*frameLength);
	if (frame == NULL) {
		printf("makeFrame(): malloc() failed\n");
		return -1;
	}
	frame[0] = FLAG;
	frame[1] = A;
	frame[2] = C_I;
	frame[3] = A ^ C_I;
	for (int ind = 4, packetInd = 0; ind < *frameLength - 2; ind++, packetInd++) {
		frame[ind] = stuffedPacket[packetInd];
	}
	frame[*frameLength - 2] = calculateBCC2(frame[3], stuffedPacket, stuffedPacketLength);
	frame[*frameLength - 1] = FLAG;

	return 0;
}

int sendFrame(int fd, char frame[], int stuffedPacketLength, int frameLength) {
	if (write(fd, frame, frameLength) == -1) {
		printf("sendFrame(): write failed\n");
		return -1;
	}
	return stuffedPacketLength;
}

int llwrite(int fd, char *data, int dataLength) {
	signal(SIGALRM, sigAlarmHandler);
	int bytesWritten = 0;
	char *stuffedData = NULL;
	int stuffedDataLength = -1;
	bool rejected = false;
	if (stuffPacket(data, dataLength, stuffedData, &stuffedDataLength) == -1) {
		printf("llwrite(): stuffPacket() failed\n");
		return -1;
	}

	do {
		char *frame = NULL;
		int frameLength = -1;
		timedOut = false;
		bool endRead = false;
		enum State state = S1;
		printf("llwrite(): Computing and sending frame\n");
		if (makeFrame(stuffedData, stuffedDataLength, frame, &frameLength) == -1) {
			printf("llwrite(): Error making frame\n");
			return -1;
		}
		if ((bytesWritten = sendFrame(fd, frame, stuffedDataLength, frameLength)) == -1) {
			printf("llwrite(): Error sending frame\n");
			return -1;
		}
		alarm(3);

		char buf[1];
		buf[0] = 0;
		int res = -1;
		char response[3];
		int ind = 0;
		while (!endRead && !timedOut && !rejected && (res = read(fd,buf,1)) != -1) {
			printf("buf[0] = %x\n", buf[0]);
			switch (state) {
			case S1:
				printf("In S1\n");
				if (res != 0 && buf[0] == FLAG) {
					state = S2;
				}
				break;
			case S2:
				printf("In S2\n");
				if (res != 0 && buf[0] != FLAG) {
					state = S3;
				}
				break;
			case S3:
				printf("In S3\n");
				if (res != 0 && buf[0] == FLAG) {
					state = END_READ;
				}
				response[ind] = buf[0];
				if (ind == 3) {
					if (!validBCC(response)) {
						printf("llwrite(): Invalid response\n");
						return -1;
					}
					if (rejected(response)) {
						printf("llwrite(): Rejected frame\n");
						rejected = true;
						break;
					}
				}
				ind++;
				break;
			case END_READ:
				printf("llwrite(): Received response\n");
				endRead = true;
				break;
			}
		}
	} while (timedOut || rejected);

	printf("llwrite(): Success\n");

	return bytesWritten;
}

volatile int STOP=FALSE;

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

    newtio.c_cc[VTIME]    = 1;
    newtio.c_cc[VMIN]     = 0;



  /*
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a
    leitura do(s) pr�ximo(s) caracter(es)
  */



    tcflush(fd, TCIOFLUSH);

    if ( tcsetattr(fd,TCSANOW,&newtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }

    printf("New termios structure set\n");

	/********************/

	char msg[] = "qwerty0123456789";

	if (-1 == llopen(fd)) {
		printf("Invalid UA response\n");
		exit(-1);
	}

	if (-1 == llwrite(fd, msg, strlen(msg))) {
		printf("llwrite() failed\n");
		exit(-1);
	}

  /*
    O ciclo FOR e as instru��es seguintes devem ser alterados de modo a respeitar
    o indicado no gui�o
  */




    if ( tcsetattr(fd,TCSANOW,&oldtio) == -1) {
      perror("tcsetattr");
      exit(-1);
    }




    close(fd);
    return 0;
}
