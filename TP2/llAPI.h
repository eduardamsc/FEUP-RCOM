#include <signal.h>

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

#define I_FRAMES_SEQ_NUM_BIT(x) (x >> 7)
#define S_U_FRAMES_SEQ_NUM_BIT(x) (x >> 8)

#define TIMEOUT 3 //seconds
#define MAX_TIME_OUTS 5 // attempts

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

static bool timedOut = false;
static int receivedSeqNum = -1;

void sigAlarmHandler(int sig) {
	timedOut = true;
	printf("alarm, timedOut = %d\n", timedOut);
}

bool validBCC1(char response[]) {
	return (response[BCC_IND_RESP] == (response[A_IND_RESP] ^ response[C_IND_RESP]));
}

bool validBCC2(char BCC1, char* buffer, int bufferLength, char BCC2) {
	char aux=BCC1;
	for (int i=0; i<bufferLength; i++) {
		aux^=buffer[i];
	}

	return aux==BCC2;
}

bool isRejected(char response[]) {
	return (response[C_IND_RESP] == C_REJ);
}

int sendReady(int fd) {
	int responseSize = 5;
	char response[responseSize];

	bzero(response, responseSize);

	response[0] = FLAG;
	response[1] = A_READ_RESP;
	response[2] = C_RR;
	response[3] = A_READ_RESP^C_RR;
	response[4] = FLAG;

	if (write(fd, response, responseSize) == -1) {
		printf("sendReady(): write failed\n");
		return -1;
	}

	return 0;
}

int llopen_read(int fd) {
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
				printf("llopen(): Received UA\n");
				endRead = true;
				break;
			}
		}
	} while (timedOut);

	printf("llopen(): Success\n");

	return 0;
}

int stuffPacket(char packet[], int packetLength, char *stuffedPacket[], int *stuffedPacketLength) {
	*stuffedPacketLength = 0;
	*stuffedPacket = realloc(*stuffedPacket, packetLength);
	if (*stuffedPacket == NULL) {
		printf("stuffPacket(): first realloc() failed\n");
		return -1;
	}

	for (int stuffedInd = 0, unstuffedInd = 0; unstuffedInd < packetLength; stuffedInd++, unstuffedInd++) {
		if (packet[unstuffedInd] == FLAG || packet[unstuffedInd] == ESC) {
			(*stuffedPacket)[stuffedInd++] = ESC;
			(*stuffedPacketLength)++;
			*stuffedPacket = realloc(*stuffedPacket, *stuffedPacketLength);
		}
		(*stuffedPacket)[stuffedInd] = packet[unstuffedInd] ^ 0x20;
		(*stuffedPacketLength)++;
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

int makeFrame(char stuffedPacket[], int stuffedPacketLength, char *frame[], int *frameLength) {
	*frameLength = stuffedPacketLength + 6;
	*frame = malloc(*frameLength);
	if (*frame == NULL) {
		printf("makeFrame(): malloc() failed\n");
		return -1;
	}
	(*frame)[0] = FLAG;
	(*frame)[1] = A;
	(*frame)[2] = C_I;
	(*frame)[3] = A ^ C_I;
	for (int ind = 4, packetInd = 0; ind < *frameLength - 2; ind++, packetInd++) {
		(*frame)[ind] = stuffedPacket[packetInd];
	}
	(*frame)[*frameLength - 2] = calculateBCC2((*frame)[3], stuffedPacket, stuffedPacketLength);
	(*frame)[*frameLength - 1] = FLAG;

	return 0;
}

int sendFrame(int fd, char frame[], int stuffedPacketLength, int frameLength) {
	printf("Frame addr = %p\n", frame);
	if (write(fd, frame, frameLength) == -1) {
		perror("sendFrame");
		return -1;
	}

		printf("stuffedPacketLength = %d\n", stuffedPacketLength);
	return stuffedPacketLength;
}

int llwrite(int fd, char *data, int dataLength) {
	signal(SIGALRM, sigAlarmHandler);
	int bytesWritten = 0;
	char *stuffedData = NULL;
	int stuffedDataLength = -1;
	bool rejected = false;
	bool success = false;
	int numTimeOuts = 0;
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
		printf("llwrite(): Computing and sending frame\n");
		if (makeFrame(stuffedData, stuffedDataLength, &frame, &frameLength) == -1) {
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
					response[ind++] = buf[0];
				}
				break;
			case S3:
				printf("In S3\n");
				if (res != 0 && buf[0] == FLAG) {
					state = END_READ;
				}
				response[ind] = buf[0];
				if (ind == 2) {
					if (!validBCC1(response)) {
						printf("llwrite(): Invalid response\n");
						return -1;
					}
					if (isRejected(response)) {
						printf("llwrite(): Rejected frame\n");
						rejected = true;
						break;
					}
					if (receivedSeqNum == S_U_FRAMES_SEQ_NUM_BIT(response[C_IND_RESP])) {
						printf("llwrite(): Frame received out of order\n");

					}
				}
				ind++;
				break;
			case END_READ:
				alarm(0);
				printf("llwrite(): Received response\n");
				receivedSeqNum = S_U_FRAMES_SEQ_NUM_BIT(response[C_IND_RESP]);
				endRead = true;
				success = true;
				break;
			}
		}
		if (timedOut || rejected) {
			numTimeOuts++;
		}
	} while ((timedOut && numTimeOuts < MAX_TIME_OUTS) || rejected);

	if (success) {
		printf("llwrite(): Success\n");
	} else {
		printf("llwrite(): Failed to send packet\n");
	}

	return bytesWritten;
}

int sendRejection(int fd) {
	int responseSize = 5;
	char response[responseSize];

	bzero(response, responseSize);

	response[0] = FLAG;
	response[1] = A_READ_RESP;
	response[2] = C_REJ;
	response[3] = A_READ_RESP^C_REJ;
	response[4] = FLAG;

	if (write(fd, response, responseSize) == -1) {
		printf("sendRejection(): write failed\n");
		return -1;
	}

	return 0;
}

int unstuffPacket(char* stuffedPacket, int stuffedPacketLength, char** buffer, int* bufferLength) {
	*bufferLength=0;
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
				if (ind == 2) {
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
				printf("%x\n",buf[0]);}
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

	char BCC2 = stuffedPacket[stuffedPacketLength-1];
	stuffedPacket = realloc(stuffedPacket, stuffedPacketLength-1);
	stuffedPacketLength--;

	unstuffPacket(stuffedPacket, stuffedPacketLength, &buffer, &bufferLength);
printf("bl %x\n",bufferLength);
	if (!validBCC2(received[2], buffer, bufferLength, BCC2)) {
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
