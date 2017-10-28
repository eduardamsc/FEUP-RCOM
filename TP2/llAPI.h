#include <signal.h>
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

//TODO - Ns and Nr.
#define FLAG 0x7E
#define A_READ_CMD 0x01
#define A_READ_RESP 0x03
#define A 0x03
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

#define I_FRAMES_SEQ_NUM_BIT(x) (x >> 6)
#define S_U_FRAMES_SEQ_NUM_BIT(x) (x >> 7)

#define TIMEOUT 3 //seconds
#define MAX_TIME_OUTS 5 // attempts
#define MAX_REJS 5 //attempts

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

enum State {
	S1,
	S2,
	S3,
	END_READ
};

enum Type {
  TRANSMITTER,
  RECEPTOR
};

static bool timedOut = false;
static struct termios oldtio;

void sigAlarmHandler(int sig) {
	timedOut = true;
}

bool validBCC1(char response[]) {
	return (response[BCC_IND_RESP] == (response[A_IND_RESP] ^ response[C_IND_RESP]));
}

bool validBCC2(char *buffer, int bufferLength, char BCC2) {
	char aux = 0;
	for (int i = 0; i < bufferLength; i++) {
		aux ^= buffer[i];
	}
	return aux == BCC2;
}

bool isRejected(char response[]) {
	return ((response[C_IND_RESP] & 0x7F) == C_REJ);
}

int sendReady(int fd, char seqNumber) {
	int responseSize = 5;
	char response[responseSize];

	bzero(response, responseSize);

	response[0] = FLAG;
	response[1] = A_READ_RESP;
	response[2] = C_RR | (seqNumber << 7);
	response[3] = A_READ_RESP ^ (C_RR | (seqNumber << 7));
	response[4] = FLAG;

	if (write(fd, response, responseSize) == -1) {
		printf("sendReady(): write failed\n");
		return -1;
	}

	return 0;
}

int setupConnection(char port[]) {

	/*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */

	int fd = open(port, O_RDWR | O_NOCTTY, S_IRWXU | S_IRWXG | S_IRWXO);
	if (fd <0) {perror(port); exit(-1); }

	struct termios newtio;
	if (tcgetattr(fd, &oldtio) == -1) { /* save current port settings */
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

	if (tcsetattr(fd,TCSANOW,&newtio) == -1) {
		perror("tcsetattr");
		exit(-1);
	}

	printf("New termios structure set\n");

	return fd;
}

int llopen_read(char port[]) {
	int fd = setupConnection(port);
	bool end = false;
	int setMsgSize = 5;
	char ua_msg[setMsgSize];

	bzero(ua_msg, setMsgSize);

	ua_msg[0] = FLAG;
	ua_msg[1] = A_READ_RESP;
	ua_msg[2] = C_UA;
	ua_msg[3] = A_READ_RESP^C_UA;
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
				if (ind >= 3) {
					state = O_S1;
					ind = 0;
					break;
				}
				received[ind++] = buf[0];
			}
			break;
		case O_S3:
			if (res != 0 && buf[0] == FLAG) {
				state = O_END_READ;
			}
			if (ind == 3) {
				state = O_S1;
				ind = 0;
				break;
			}
			received[ind++] = buf[0];

			if (ind == 3) {
				if (!validBCC1(received)) {
					printf("llopen(): invalid SET\n");
					return -1;
				}
			}
			break;
		case O_END_READ:
			#ifdef DEBUG
			printf("llopen(): received SET\n");
			#endif
			end = true;
			break;
		}
	}
	#ifdef DEBUG
	printf("sending UA\n");
	#endif
	if (write(fd, ua_msg, setMsgSize) == -1) {
		#ifdef DEBUG
		printf("llopen_read(): Failed to write UA to port.\n");
		#endif
		return -1;
	}
	#ifdef DEBUG
	printf("llopen Success\n");
	#endif
	return fd;
}

int llopen(char port[]) {
	int fd = setupConnection(port);
	int setMsgSize = 5;
	char set_msg[setMsgSize];
	bzero(set_msg, setMsgSize);

	set_msg[0] = FLAG;
	set_msg[1] = A;
	set_msg[2] = C_SET;
	set_msg[3] = A^C_SET;
	set_msg[4] = FLAG;

	int numTimeOuts = 0;

	do {
		timedOut = false;
		bool endRead = false;
		enum State state = S1;
		#ifdef DEBUG
		printf("llopen(): Sending SET\n");
		#endif
		write(fd, set_msg, setMsgSize);
		signal(SIGALRM, sigAlarmHandler);
		alarm(3);

		char buf[1];
		buf[0] = 0;
		int res = -1;
		char response[3];
		int ind = 0;
		while (!endRead && !timedOut && (res = read(fd,buf,1)) != -1) {
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
					if (!validBCC1(response)) {
						printf("llopen(): Invalid UA\n");
						return -1;
					}
				}
				ind++;
				break;
			case END_READ:
				#ifdef DEBUG
				printf("llopen(): Received UA\n");
				#endif
				endRead = true;
				break;
			}
		}
		if (timedOut) {
			numTimeOuts++;
			printf("Attempt %d of %d failed, retrying.\n", numTimeOuts, MAX_TIME_OUTS);
		}
	} while (timedOut && numTimeOuts < MAX_TIME_OUTS);

	if (timedOut) {
		printf("llopen(): Connection timed out\n");
		return -1;
	}
	printf("llopen(): Success\n");
	return fd;
}

int stuffPacket(char packet[], int packetLength, char *stuffedPacket[], int *stuffedPacketLength) {
	*stuffedPacketLength = 0;
	*stuffedPacket = malloc(packetLength);
	if (*stuffedPacket == NULL) {
		printf("stuffPacket(): first realloc() failed\n");
		return -1;
	}

	for (int stuffedInd = 0, unstuffedInd = 0; unstuffedInd < packetLength; stuffedInd++, unstuffedInd++) {
		if (packet[unstuffedInd] == FLAG || packet[unstuffedInd] == ESC) {
			(*stuffedPacket)[stuffedInd++] = ESC;
			(*stuffedPacketLength)++;
			*stuffedPacket = realloc(*stuffedPacket, *stuffedPacketLength);
			(*stuffedPacket)[stuffedInd] = packet[unstuffedInd] ^ 0x20;
			(*stuffedPacketLength)++;
		} else {
			(*stuffedPacket)[stuffedInd] = packet[unstuffedInd];
			(*stuffedPacketLength)++;
		}
	}
	return 0;
}

char calculateBCC2(char stuffedPacket[], int stuffedPacketLength) {
	int res = 0;
	for (int ind = 0; ind < stuffedPacketLength; ind++) {
		res ^= stuffedPacket[ind];
	}
	return res;
}

int makeFrame(char stuffedPacket[], int stuffedPacketLength, char *frame[], int *frameLength, int seqNumber) {
	*frameLength = stuffedPacketLength + 6;
	*frame = malloc(*frameLength);
	if (*frame == NULL) {
		printf("makeFrame(): malloc() failed\n");
		return -1;
	}
	(*frame)[0] = FLAG;
	(*frame)[1] = A;
	(*frame)[2] = C_I | (seqNumber << 6);
	(*frame)[3] = A ^ (C_I | (seqNumber << 6));
	for (int ind = 4, packetInd = 0; ind < *frameLength - 2; ind++, packetInd++) {
		(*frame)[ind] = stuffedPacket[packetInd];
	}
	(*frame)[*frameLength - 2] = calculateBCC2(stuffedPacket, stuffedPacketLength);
	(*frame)[*frameLength - 1] = FLAG;

	return 0;
}

int sendFrame(int fd, char frame[], int stuffedPacketLength, int frameLength) {
	if (write(fd, frame, frameLength) == -1) {
		perror("sendFrame");
		return -1;
	}

	return stuffedPacketLength;
}

int llwrite(int fd, char *data, int dataLength) {
	static char receivedSeqNum = 0;
	int bytesWritten = 0;
	char *stuffedData = NULL;
	int stuffedDataLength = -1;
	bool rejected = false;
	#ifdef DEBUG
	bool success = false;
	#endif
	int numTimeOuts = 0, numRejects = 0;
	if (stuffPacket(data, dataLength, &stuffedData, &stuffedDataLength) == -1) {
		printf("llwrite(): stuffPacket() failed\n");
		return -1;
	}

	do {
		char *frame = NULL;
		int frameLength = -1;
		timedOut = false;
		rejected = false;
		bool endRead = false;
		enum State state = S1;
		#ifdef DEBUG
		printf("llwrite(): Computing and sending frame\n");
		#endif
		if (makeFrame(stuffedData, stuffedDataLength, &frame, &frameLength, receivedSeqNum) == -1) {
			printf("llwrite(): Error making frame\n");
			return -1;
		}
		if ((bytesWritten = sendFrame(fd, frame, stuffedDataLength, frameLength)) == -1) {
			printf("llwrite(): Error sending frame\n");
			return -1;
		}
		free(frame);
		signal(SIGALRM, sigAlarmHandler);
		alarm(3);

		char buf[1];
		buf[0] = 0;
		int res = -1;
		char response[3];
		int ind = 0;
		while (!endRead && !timedOut && !rejected && (res = read(fd,buf,1)) != -1) {
			#ifdef DEBUG
			printf("buf[0] = %x\n", buf[0]);
			#endif
			switch (state) {
			case S1:
				if (res != 0 && buf[0] == FLAG) {
					state = S2;
				}
				break;
			case S2:
				if (res != 0 && buf[0] != FLAG) {
					state = S3;
					response[ind++] = buf[0];
				}
				break;
			case S3:
				if (res != 0 && buf[0] == FLAG) {
					state = END_READ;
				}
				response[ind] = buf[0];
				if (ind == 2) {
					if (!validBCC1(response)) {
						#ifdef DEBUG
						printf("llwrite(): Invalid response\n");
						#endif
						return -1;
					}
					if (isRejected(response)) {
						#ifdef DEBUG
						printf("llwrite(): Rejected frame\n");
						#endif
						rejected = true;
						break;
					}
				}
				ind++;
				break;
			case END_READ:
				alarm(0);
				#ifdef DEBUG
				printf("llwrite(): Received response\n");
				#endif
				receivedSeqNum = S_U_FRAMES_SEQ_NUM_BIT(response[C_IND_RESP]);
				endRead = true;
				#ifdef DEBUG
				success = true;
				#endif
				break;
			}
		}
		if (timedOut) {
			numTimeOuts++;
			printf("%d/%d: Timed out, resending.\n", numTimeOuts, MAX_TIME_OUTS);
			continue;
		}
		if (rejected) {
			numRejects++;
			printf("%d/%d: Packet rejected, resending.\n", numRejects, MAX_REJS);
			continue;
		}
	} while ((timedOut && numTimeOuts < MAX_TIME_OUTS) || (rejected && numRejects < MAX_REJS));
	
	free(stuffedData);

	#ifdef DEBUG
	if (success) {
		printf("llwrite(): Success.\n");
	} else {
		printf("llwrite(): Failed to send packet.\n");
	}
	#endif

	if (numTimeOuts >= MAX_TIME_OUTS || numRejects >= MAX_REJS) {
		return -1;
	}

	return bytesWritten;
}

int sendRejection(int fd, char seqNumber) {
	int responseSize = 5;
	char response[responseSize];

	bzero(response, responseSize);

	response[0] = FLAG;
	response[1] = A_READ_RESP;
	response[2] = C_REJ | (seqNumber << 7);
	response[3] = A_READ_RESP ^ (C_REJ << 7);
	response[4] = FLAG;

	if (write(fd, response, responseSize) == -1) {
		printf("sendRejection(): write failed\n");
		return -1;
	}

	return 0;
}

int unstuffPacket(char* stuffedPacket, int stuffedPacketLength, char *buffer[], int *bufferLength) {
	*bufferLength = 0;
	*buffer = malloc(stuffedPacketLength);
	if (*buffer == NULL) {
		printf("unstuffPacket(): malloc failed\n");
		return -1;
	}

	for (int stuffedInd = 0, bufferInd = 0; stuffedInd < stuffedPacketLength; stuffedInd++, bufferInd++) {
		if (stuffedPacket[stuffedInd] == ESC) {
			switch (stuffedPacket[stuffedInd + 1]) {
				case 0x5E:
					(*buffer)[bufferInd] = FLAG;
					stuffedInd++;
					break;
				case 0x5D:
					(*buffer)[bufferInd] = ESC;
					stuffedInd++;
					break;
			}
		} else {
			(*buffer)[bufferInd] = stuffedPacket[stuffedInd];
		}

		(*bufferLength)++;
	}

	*buffer = realloc(*buffer, *bufferLength);

	return 0;
}

int llread(int fd, char **buffer) {
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
		#ifdef DEBUG
		printf("llread(): Read byte %x\n", buf[0]);
		#endif
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
				if (ind == 2) {
					if (!validBCC1(received)) {
						#ifdef DEBUG
						printf("llread(): invalid SET\n");
						#endif
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

	char BCC2 = stuffedPacket[stuffedPacketLength - 1];
	stuffedPacket = realloc(stuffedPacket, stuffedPacketLength - 1);
	stuffedPacketLength--;

	unstuffPacket(stuffedPacket, stuffedPacketLength, buffer, &bufferLength);
	char receivedSeqNum = !((*buffer)[2] >> 6);
	if (!validBCC2(*buffer, bufferLength, BCC2)) {
		printf("llread(): Invalid BCC2, sending REJ \n");
		if (sendRejection(fd, receivedSeqNum) == -1) {
			printf("llread(): sendRejection error\n");
			return -1;
		}
	} else {
		#ifdef DEBUG
		printf("llread(): Sending RR\n");
		#endif
		if (sendReady(fd, receivedSeqNum) == -1) {
			printf("llread(): sendReady error\n");
			return -1;
		}
		#ifdef DEBUG
		printf("llread(): Success\n");
		#endif
	}

	return bufferLength;
}

int llclose_Transmitter(int fd) {
	int msgSize = 5;
	char disc_msg[msgSize], ua_msg[msgSize];
	bzero (disc_msg, msgSize);
	bzero (ua_msg, msgSize);

	disc_msg[0] = FLAG;
	disc_msg[1] = A;
	disc_msg[2] = C_DISC;
	disc_msg[3] = A^C_DISC;
	disc_msg[4] = FLAG;

	ua_msg[0] = FLAG;
	ua_msg[1] = A;
	ua_msg[2] = C_UA;
	ua_msg[3] = A^C_UA;
	ua_msg[4] = FLAG;

	int numTimeOuts = 0;

	do {
		timedOut = false;
		bool endRead = false;
		enum State state = S1;
		#ifdef DEBUG
		printf("llclose(): Sending DISC\n");
		#endif
		write(fd, disc_msg, msgSize);
		signal(SIGALRM, sigAlarmHandler);
		alarm(3);

		char buf[1];
		buf[0] = 0;
		int res = -1;
		char response[3];
		int ind = 0;
		while (!endRead && !timedOut && (res = read(fd,buf,1)) != -1) {
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
					if (!validBCC1(response)) {
						printf("llclose(): Invalid DISC\n");
						return -1;
					}
				}
				ind++;
				break;
			case END_READ:
				#ifdef DEBUG
				printf("llclose(): Received DISC\n");
				#endif
				endRead = true;
				break;
			}
		}
		if (timedOut) {
			numTimeOuts++;
			printf("Attempt %d of %d failed, retrying.\n", numTimeOuts, MAX_TIME_OUTS);
		}
	} while (timedOut);

	write(fd, ua_msg, msgSize);

	if (tcsetattr(fd,TCSANOW,&oldtio) == -1) {
		perror("tcsetattr");
		return -1;
	}
	if (close(fd) == -1) {
		perror("close");
		return -1;
	}

	#ifdef DEBUG
	printf("llclose(): Success\n");
	#endif
	return 0;
}

int llclose_Receiver(int fd) {
		int msgSize = 5;
		char disc_msg[msgSize], ua_msg[msgSize];
		bzero (disc_msg, msgSize);
		bzero (ua_msg, msgSize);

		disc_msg[0] = FLAG;
		disc_msg[1] = A;
		disc_msg[2] = C_DISC;
		disc_msg[3] = A^C_DISC;
		disc_msg[4] = FLAG;

		ua_msg[0] = FLAG;
		ua_msg[1] = A;
		ua_msg[2] = C_UA;
		ua_msg[3] = A^C_UA;
		ua_msg[4] = FLAG;

		bool endRead = false;
		enum State state = S1;

		char buf[1];
		buf[0] = 0;
		int res = -1;
		char response[3];
		int ind = 0;
		while (!endRead  && (res = read(fd,buf,1)) != -1) {
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
					if (!validBCC1(response)) {
						printf("llclose(): Invalid DISC\n");
						return -1;
					}
					if(response[2] != C_DISC){
						printf("llclose(): Could not find DISC\n");
						return -1;
					}
				}
				ind++;
				break;
			case END_READ:
				printf("llclose(): Received DISC\n");
				endRead = true;
				break;
			}
		}

	int numTimeOuts = 0;

	do {
		timedOut = false;
		bool endRead = false;
		enum State state = S1;
		write(fd, disc_msg, msgSize);
		signal(SIGALRM, sigAlarmHandler);
		alarm(3);
		printf("llclose(): Sending DISC\n");

		char buf[1];
		buf[0] = 0;
		int res = -1;
		char response[3];
		int ind = 0;
		while (!endRead && !timedOut && (res = read(fd,buf,1)) != -1) {
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
					if (!validBCC1(response)) {
						printf("llclose(): Invalid UA\n");
						return -1;
					}
					if(response[2] != C_UA){
						printf("llclose(): Could not find UA\n");
						return -1;
					}
				}
				ind++;
				break;
			case END_READ:
				printf("llclose(): Received UA\n");
				endRead = true;
				break;
			}
		}
		if (timedOut) {
			numTimeOuts++;
			printf("Attempt %d of %d failed, retrying.\n", numTimeOuts, MAX_TIME_OUTS);
		}
	} while (timedOut && numTimeOuts < MAX_TIME_OUTS);

	if (tcsetattr(fd,TCSANOW,&oldtio) == -1) {
		perror("tcsetattr");
		return -1;
	}
	if (close(fd) == -1) {
		perror("close");
		return -1;
	}

	printf("llclose(): Success\n");
	return 0;
}
