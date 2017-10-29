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
#define F1 0x7D
#define F2 0x5E
#define A_3 0x03
#define A_1 0x01
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

static bool timedOut = false;
static struct termios oldtio;
static enum CommsType global_type;

void sigAlarmHandler(int sig) {
	timedOut = true;
}

enum FrameTypeRes {
	DATA,
	SET,
	DISC,
	UA,
	RR,
	REJ,
	IGNORE
};

enum ReadFrameState {
	AWAITING_FLAG,
	AWAITING_A,
	AWAITING_C,
	// C begin
	FOUND_I,
	FOUND_SET,
	FOUND_DISC,
	FOUND_UA,
	FOUND_RR,
	FOUND_REJ,
	UNKNOWN_C,
	// C end
	VALIDATED_BCC_I,
	VALIDATED_BCC_OTHERS,
	READING_I_DATA
};

enum ReadFrameState interpretC(char c) {
	switch (c & 0x3F) { // ignore sequence number
		case 0x0:
			return FOUND_I;
		case 0x3:
			return FOUND_SET;
		case 0xB:
			return FOUND_DISC;
		case 0x7:
			return FOUND_UA;
		case 0x5:
			return FOUND_RR;
		case 0x1:
			return FOUND_REJ;
		default:
			return UNKNOWN_C;
	}
}

bool validBCC1(char A_BYTE, char C, char BCC1) {
	return BCC1 == (A_BYTE ^ C);
}

enum FrameTypeRes readFrame(int fd, char **frame, int *frameLength) {
	*frame = malloc(5);
	*frameLength = 5;
	(*frame)[0] = FLAG;
	enum ReadFrameState state = AWAITING_FLAG;
	enum FrameTypeRes frameTypeRes;
	char buf;
	int bytesRead;
	while ((bytesRead = read(fd, &buf, 1)) != -1) {
		if (bytesRead == 0) {
			continue;
		}
		switch (state) {
			case AWAITING_FLAG:
				if (buf == FLAG) {
					state = AWAITING_A;
				}
				break;
			case AWAITING_A:
				if (buf != FLAG) {
					state = AWAITING_C;
					(*frame)[1] = buf;
				}
				break;
			case AWAITING_C:
				state = interpretC(buf);
				(*frame)[2] = buf;
				break;
			case UNKNOWN_C:
				return IGNORE;
				break;
			case FOUND_I:
				state = VALIDATED_BCC_I;
				(*frame)[3] = buf;
				if (!validBCC1((*frame)[1], (*frame)[2], (*frame)[3])) {
					return IGNORE;
				}
				break;
			case FOUND_SET:
				state = VALIDATED_BCC_OTHERS;
				frameTypeRes = SET;
				(*frame)[3] = buf;
				if (!validBCC1((*frame)[1], (*frame)[2], (*frame)[3])) {
					return IGNORE;
				}
				break;
			case FOUND_DISC:
				state = VALIDATED_BCC_OTHERS;
				frameTypeRes = DISC;
				(*frame)[3] = buf;
				if (!validBCC1((*frame)[1], (*frame)[2], (*frame)[3])) {
					return IGNORE;
				}
				break;
			case FOUND_UA:
				state = VALIDATED_BCC_OTHERS;
				frameTypeRes = UA;
				(*frame)[3] = buf;
				if (!validBCC1((*frame)[1], (*frame)[2], (*frame)[3])) {
					return IGNORE;
				}
				break;
			case FOUND_RR:
				state = VALIDATED_BCC_OTHERS;
				frameTypeRes = RR;
				(*frame)[3] = buf;
				if (!validBCC1((*frame)[1], (*frame)[2], (*frame)[3])) {
					return IGNORE;
				}
				break;
			case FOUND_REJ:
				state = VALIDATED_BCC_OTHERS;
				frameTypeRes = REJ;
				(*frame)[3] = buf;
				if (!validBCC1((*frame)[1], (*frame)[2], (*frame)[3])) {
					return IGNORE;
				}
				break;
			case VALIDATED_BCC_I:
				(*frame)[4] = buf;
				if (buf == FLAG) {
					return DATA;
				} else {
					state = READING_I_DATA;
				}
				break;
			case VALIDATED_BCC_OTHERS:
				if (buf != FLAG) {
					return IGNORE;
				} else {
					(*frame)[4] = FLAG;
					return frameTypeRes;
				}
			case READING_I_DATA:
				(*frameLength)++;
				*frame = realloc(*frame, *frameLength);
				(*frame)[*frameLength - 1] = buf;
				if (buf == FLAG) {
					return DATA;
				}
		}
	}
	#ifdef DEBUG
	printf("readFrame(): Low level reading state machine in invalid state. Exitting.\n");
	exit(-1);
	#endif
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

enum CommsType {
	TRANSMITTER,
	RECEIVER
};

/**
 * setMsg must already be allocated with 5 chars.
 */
void makeSetMsg(char *setMsg) {
	setMsg[0] = FLAG;
	setMsg[1] = A_3;
	setMsg[2] = C_SET;
	setMsg[3] = A_3 ^ C_SET;
	setMsg[4] = FLAG;
}

/**
 * uaMsg must already be allocated with 5 chars.
 */
void makeUaMsg(char *uaMsg) {
	uaMsg[0] = FLAG;
	uaMsg[1] = A_3;
	uaMsg[2] = C_UA;
	uaMsg[3] = A_3 ^ C_UA;
	uaMsg[4] = FLAG;
}

int llopenTransmitter(int fd) {
	int numTimeOuts = 0;
	char setMsg[5];
	int setMsgSize = 5;
	makeSetMsg(setMsg);
	do {
		timedOut = false;
		if (write(fd, setMsg, setMsgSize) == -1) {
			perror("llopenTransmitter - write");
			return -1;
		}
		alarm(3);
		signal(SIGALRM, sigAlarmHandler);

		enum FrameTypeRes res;
		char *frame = NULL;
		int frameLength = 0;
		do {
			res = readFrame(fd, &frame, &frameLength);
		} while (res != UA && !timedOut);

		if (timedOut) {
			numTimeOuts++;
			if (numTimeOuts < MAX_TIME_OUTS) {
				printf("%d/%d: Timed out on connection establishment. Retrying.\n", numTimeOuts, MAX_TIME_OUTS);
			} else {
				printf("%d/%d: Timed out on connection establishment. Exiting.\n", numTimeOuts, MAX_TIME_OUTS);
				return -1;
			}
		}
	} while (timedOut && numTimeOuts < MAX_TIME_OUTS);

	return 0;
}

int llopenReceiver(int fd) {
	char *frame = NULL;
	int frameLength = 0;
	while (SET != readFrame(fd, &frame, &frameLength)) {}
	free(frame);

	char uaMsg[5];
	int uaMsgSize = 5;
	makeUaMsg(uaMsg);
	if (write(fd, uaMsg, uaMsgSize) == -1) {
		perror("llopenReceiver - write");
		return -1;
	}

	return 0;
}

int llopen(char port[], enum CommsType type) {
	global_type = type;
	int fd = setupConnection(port);
	switch (type) {
		case TRANSMITTER:
			if (llopenTransmitter(fd) == -1) {
				#ifdef DEBUG
				printf("llopen(): llopenTransmitter failed.\n");
				#endif
				return -1;
			}
			break;
		case RECEIVER:
			if (llopenReceiver(fd) == -1) {
				#ifdef DEBUG
				printf("llopen(): llopenReceiver failed.\n");
				#endif
				return -1;
			}
			break;
	}
	return fd;
}

int makeFrame(char *data, int dataLength, char seqNum, char **frame, int *frameLength) {
	*frameLength = 4 + dataLength + 2;
	*frame = malloc(*frameLength);
	if (*frame == NULL) {
		perror("makeFrame - malloc");
		return -1;
	}

	(*frame)[0] = FLAG;
	(*frame)[1] = A_3;
	(*frame)[2] = C_I | (seqNum << 6);
	(*frame)[3] = (*frame)[1] ^ (*frame)[2];

	char BCC2 = 0;
	for (int dataInd = 0, frameInd = 4; dataInd < dataLength; dataInd++, frameInd++) {
		(*frame)[frameInd] = data[dataInd];
		BCC2 ^= data[dataInd];
	}

	(*frame)[*frameLength - 2] = BCC2;
	(*frame)[*frameLength - 1] = FLAG;

	return 0;
}

/**
 * Outputs stuffedFrame, allocating it. Its length is greater or equal to the original frame's length.
 * Start and stop flags aren't stuffed.
 */
int stuffFrame(char *frame, int frameLength, char **stuffedFrame, int *stuffedFrameLength) {
	*stuffedFrameLength = frameLength;
	*stuffedFrame = malloc(*stuffedFrameLength);
	if (*stuffedFrame == NULL) {
		printf("stuffFrame - malloc");
		return -1;
	}

	(*stuffedFrame)[0] = FLAG;
	for (int unstuffedInd = 1, stuffedInd = 1; unstuffedInd < frameLength - 1; unstuffedInd++, stuffedInd++) {
		switch (frame[unstuffedInd]) {
			case FLAG:
				(*stuffedFrameLength)++;
				*stuffedFrame = realloc(*stuffedFrame, *stuffedFrameLength);
				if (*stuffedFrame == NULL) {
					perror("stuffFrame - realloc");
					return -1;
				}
				(*stuffedFrame)[stuffedInd++] = ESC;
				(*stuffedFrame)[stuffedInd] = FLAG ^ 0x20;
				break;
			case ESC:
				(*stuffedFrameLength)++;
				*stuffedFrame = realloc(*stuffedFrame, *stuffedFrameLength);
				if (*stuffedFrame == NULL) {
					perror("stuffFrame - realloc");
					return -1;
				}
				(*stuffedFrame)[stuffedInd++] = ESC;
				(*stuffedFrame)[stuffedInd] = ESC ^ 0x20;
				break;
			default:
				(*stuffedFrame)[stuffedInd] = frame[unstuffedInd];
		}
	}
	(*stuffedFrame)[*stuffedFrameLength - 1] = FLAG;

	return 0;
}

int unstuffFrame(char *stuffedFrame, int stuffedFrameLength, char **frame, int *frameLength) {
	*frameLength = stuffedFrameLength;
	*frame = malloc(*frameLength);
	if (*frame == NULL) {
		perror("unstuffFrame - malloc");
		return -1;
	}

	(*frame)[0] = FLAG;
	for (int stuffedInd = 1, unstuffedInd = 1; stuffedInd < stuffedFrameLength - 1; stuffedInd++, unstuffedInd++) {
		switch (stuffedFrame[stuffedInd]) {
			case ESC:
				(*frameLength)--;
				*frame = realloc(*frame, *frameLength);
				stuffedInd++;
				(*frame)[unstuffedInd] = stuffedFrame[stuffedInd] ^ 0x20;
				break;
			default:
				(*frame)[unstuffedInd] = stuffedFrame[stuffedInd];
				break;
		}
	}
	(*frame)[*frameLength - 1] = FLAG;

	return 0;
}

int extractPacket(char **packet, int *packetLength, char *frame, int frameLength) {
	*packetLength = -4 + frameLength - 2;
	*packet = malloc(*packetLength);
	if (*packet == NULL) {
		perror("extractPacket - malloc");
		return -1;
	}
	memcpy(*packet, frame + 4, *packetLength);
	return 0;
}

bool validPacketBCC(char *packet, int packetLength, char BCC2) {
	char acc = 0;
	for (int i = 0; i < packetLength; i++) {
		acc ^= packet[i];
	}
	return BCC2 == acc;
}

bool frameIsDuplicated(char *frame, char previousSeqNum) {
	return previousSeqNum == (I_FRAMES_SEQ_NUM_BIT(frame[2]));
}

int sendReady(int fd, char seqNumber) {
	int responseSize = 5;
	char response[responseSize];

	bzero(response, responseSize);

	response[0] = FLAG;
	response[1] = A_3;
	response[2] = C_RR | (seqNumber << 7);
	response[3] = A_3 ^ (C_RR | (seqNumber << 7));
	response[4] = FLAG;

	if (write(fd, response, responseSize) == -1) {
		printf("sendReady(): write failed\n");
		return -1;
	}

	return 0;
}

int sendRejection(int fd, char seqNumber) {
	int responseSize = 5;
	char response[responseSize];

	bzero(response, responseSize);

	response[0] = FLAG;
	response[1] = A_3;
	response[2] = C_REJ | (seqNumber << 7);
	response[3] = A_3 ^ (C_REJ | (seqNumber << 7));
	response[4] = FLAG;

	if (write(fd, response, responseSize) == -1) {
		printf("sendReady(): write failed\n");
		return -1;
	}

	return 0;
}

/**
 * @return Buffer length (bytes read), -1 if error.
 */
int llread(int fd, char **packet) {
	static char previousSeqNum = -1;
	bool rejected = false;
	int numRejects = 0;
	int packetLength = 0;

	char frameC = 0;

	do {
		rejected = false;
		char *stuffedFrame = NULL;
		int stuffedFrameLength = 0;
		while (true) {
			enum FrameTypeRes res = readFrame(fd, &stuffedFrame, &stuffedFrameLength);
			if (res == DATA) {
				break;
			}
		}

		char *frame = NULL;
		int frameLength = 0;
		if (unstuffFrame(stuffedFrame, stuffedFrameLength, &frame, &frameLength) == -1) {
			#ifdef DEBUG
			printf("llread(): unstuffFrame failed.\n");
			#endif
			return -1;
		}
		frameC = frame[2];

		if (extractPacket(packet, &packetLength, frame, frameLength) == -1) {
			#ifdef DEBUG
			printf("llread(): extractPacket failed.\n");
			#endif
			return -1;
		}
		char BCC2 = frame[frameLength - 2];
		if (!validPacketBCC(*packet, packetLength, BCC2)) {
			if (frameIsDuplicated(frame, previousSeqNum)) {
				sendReady(fd, !previousSeqNum);
				rejected = false;
			} else {
				sendRejection(fd, !previousSeqNum);
				rejected = true;
			}
		} else {
			sendReady(fd, !previousSeqNum);
			rejected = false;
		}

		if (rejected) {
			numRejects++;
			#ifdef DEBUG
			printf("llread(): Packet rejected.\n");
			#endif
		}
	} while (rejected && numRejects < MAX_REJS);
	previousSeqNum = !I_FRAMES_SEQ_NUM_BIT(frameC);

	if (numRejects < MAX_REJS) {
		return packetLength;
	} else {
		return 0;
	}
}

/**
 * @return Bytes written, -1 if error.
 */
int llwrite(int fd, char *data, int dataLength) {
	static char seqNum = 0;
	char *frame = NULL;
	int frameLength = 0;
	char *stuffedFrame = NULL;
	int stuffedFrameLength = 0;
	char *responseFrame = NULL;
	int responseFrameLength = 0;
	int numTimeOuts = 0;

	makeFrame(data, dataLength, seqNum, &frame, &frameLength);
	stuffFrame(frame, frameLength, &stuffedFrame, &stuffedFrameLength);
	do {
		timedOut = false;
		if (write(fd, stuffedFrame, stuffedFrameLength) == -1) {
			perror("llwrite - write");
			return -1;
		}
		alarm(3);
		signal(SIGALRM, sigAlarmHandler);
		enum FrameTypeRes res;
		bool accepted = false;
		do {
			res = readFrame(fd, &responseFrame,  &responseFrameLength);
			switch (res) {
				case RR:
					accepted = true;
					break;
				case REJ:
					accepted = false;
					break;
				case IGNORE:
					accepted = true;
					break;
				default:
					continue;
			}
		} while (!timedOut && !accepted);

		if (timedOut) {
			++numTimeOuts;
			if (numTimeOuts < MAX_TIME_OUTS) {
				printf("%d/%d: Timed out while sending packet. Retrying.\n", numTimeOuts, MAX_TIME_OUTS);
			} else {
				printf("%d/%d: Timed out while sending packet. Exiting.\n", numTimeOuts, MAX_TIME_OUTS);
				return -1;
			}
		}

	} while(timedOut && numTimeOuts < MAX_TIME_OUTS);

	alarm(0);

	if (numTimeOuts >= MAX_TIME_OUTS) {
		return 0;
	} else {
		return dataLength;
	}
}

/**
 * discMsg must already be allocated with 5 chars.
 */
void makeTransDiscMsg(char *discMsg) {
	discMsg[0] = FLAG;
	discMsg[1] = A_3;
	discMsg[2] = C_DISC;
	discMsg[3] = A_3 ^ C_DISC;
	discMsg[4] = FLAG;
}

/**
 * discMsg must already be allocated with 5 chars.
 */
void makeReceiverDiscMsg(char *discMsg) {
	discMsg[0] = FLAG;
	discMsg[1] = A_1;
	discMsg[2] = C_DISC;
	discMsg[3] = A_1 ^ C_DISC;
	discMsg[4] = FLAG;
}

void makeTransUaMsg(char *uaMsg) {
	uaMsg[0] = FLAG;
	uaMsg[1] = A_1;
	uaMsg[2] = C_UA;
	uaMsg[3] = A_1 ^ C_UA;
	uaMsg[4] = FLAG;
}

/**
 * Returns 1 if success, -1 if error.
 */
int llcloseTransmitter(int fd) {
	int numTimeOuts = 0;
	int discMsgSize = 5;
	char discMsg[5];
	makeTransDiscMsg(discMsg);
	do {
		timedOut = false;
		if (write(fd, discMsg, discMsgSize) == -1) {
			perror("llcloseTransmitter - write");
			return -1;
		}
		alarm(3);
		signal(SIGALRM, sigAlarmHandler);

		enum FrameTypeRes res;
		char *frame = NULL;
		int frameLength = 0;
		do {
			res = readFrame(fd, &frame, &frameLength);
		} while (res != DISC && !timedOut);

		alarm(0);

		if (timedOut) {
			numTimeOuts++;
			if (numTimeOuts < MAX_TIME_OUTS) {
				printf("%d/%d: Timed out on disconnection. Retrying.\n", numTimeOuts, MAX_TIME_OUTS);
			} else {
				printf("%d/%d: Timed out on disconnection. Exiting.\n", numTimeOuts, MAX_TIME_OUTS);
				return -1;
			}
		}
	} while (timedOut && numTimeOuts < MAX_TIME_OUTS);

	int uaMsgSize = 5;
	char uaMsg[5];
	makeTransUaMsg(uaMsg);
	if (write(fd, uaMsg, uaMsgSize) == -1) {
		perror("llcloseTransmitter - write");
		return -1;
	}

	return 1;
}

/**
 * Returns 1 if success, -1 if error.
 */
int llcloseReceiver(int fd) {
	enum FrameTypeRes res;
	char *frame = NULL;
	int frameLength = 0;
	do {
		res = readFrame(fd, &frame, &frameLength);
	} while (res != DISC);

	int discMsgSize = 5;
	char discMsg[5];
	makeReceiverDiscMsg(discMsg);

	int numTimeOuts = 0;
	do {
		timedOut = false;
		if (write(fd, discMsg, discMsgSize) == -1) {
			perror("llcloseReceiver - write");
			return -1;
		}
		alarm(3);
		signal(SIGALRM, sigAlarmHandler);

		enum FrameTypeRes res;
		char *frame = NULL;
		int frameLength = 0;
		do {
			res = readFrame(fd, &frame, &frameLength);
		} while (res != UA && !timedOut);

		alarm(0);

		if (timedOut) {
			numTimeOuts++;
			if (numTimeOuts < MAX_TIME_OUTS) {
				printf("%d/%d: Timed out on disconnection acknowledgement. Retrying.\n", numTimeOuts, MAX_TIME_OUTS);
			} else {
				printf("%d/%d: Timed out on disconnection acknowledgement. Exiting.\n", numTimeOuts, MAX_TIME_OUTS);
				return -1;
			}
		}
	} while (timedOut && numTimeOuts < MAX_TIME_OUTS);

	return 1;
}

/**
 * Returns 1 if success, -1 if error.
 */
int llclose(int fd) {
	switch (global_type) {
		case TRANSMITTER:
			return llcloseTransmitter(fd);
		case RECEIVER:
			return llcloseReceiver(fd);
		default:
			#ifdef DEBUG
			printf("llclose(): Invalid CommsType.\n");
			#endif
			return -1;
	}
}
