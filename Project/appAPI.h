#include "llAPI.h"
#include <stdio.h>
#include <math.h>
#include <sys/time.h>

#define C_APP 0
#define TLV_T 0
#define TLV_L 1
#define TLV_V 2

#define DATA_N 1
#define DATA_L2 2
#define DATA_L1 3
#define DATA_P1 4

#define T_FILE_SIZE 0
#define T_FILE_NAME 1

#define DATA_PACKET 1
#define START_PACKET 2
#define END_PACKET 3

struct ExecTimes {
	struct timeval *startTime;
	struct timeval *startDataTime;
	struct timeval *endDataTime;
	struct timeval *endTime;
};

void readFileSize(char *fileSizeChars, int *fileLength, int arrayLength) {
	*fileLength = 0;
	int hexOrder = 0;
	for (int i = arrayLength - 1; i >= 0; i--) {
		*fileLength += (fileSizeChars[i] & 0xF) * pow(16, hexOrder++);
		*fileLength += ((fileSizeChars[i] & 0xF0) >> 4) * pow(16, hexOrder);
		hexOrder++;
	}
}

/**
 * Reads data in packet to fileBuffer sequentially, reallocating it.
 */
int processDataPacket(char *packet, char **fileBuffer, int *fileBufferLength,
		int *seqNumMismatches) {
	static int prevSeqNum = -1;

	unsigned char sequenceNumber = packet[DATA_N];
	if (prevSeqNum == -1) {
		prevSeqNum = sequenceNumber;
	} else if ((prevSeqNum + 1) % 255 != sequenceNumber) {
		printf("Warning: Sequence number mismatch in data packet.\n");
		(*seqNumMismatches)++;
#ifdef DEBUG
		printf("prevSeqNum = %d | sequenceNumber = %d\n", prevSeqNum, sequenceNumber);
#endif
	}

	int dataSize = 256 * (unsigned char) packet[DATA_L2]
			+ (unsigned char) packet[DATA_L1];
	*fileBuffer = realloc(*fileBuffer, *fileBufferLength + dataSize);
	for (int i = 0; i < dataSize; i++) {
		(*fileBuffer)[*fileBufferLength + i] = packet[DATA_P1 + i];
	}
	*fileBufferLength += dataSize;

	prevSeqNum = sequenceNumber;
	return 0;
}

/**
 * Reads file length and filename from packet, if they exist.
 */
int processStartPacket(char *packet, int packetLength, int *fileLength,
		char **filename) {
	bool setName = false, setSize = false;
	int bytesRead = 1;

	while (bytesRead < packetLength) {
		int vLength = packet[bytesRead + TLV_L];
		switch (packet[bytesRead + TLV_T]) {
		case T_FILE_SIZE:
			if (setSize) {
				break;
			}
			char *fileSizeChars = malloc(vLength + 1);
			memcpy(fileSizeChars, packet + bytesRead + TLV_V, vLength);
			fileSizeChars[vLength] = '\0';
			readFileSize(fileSizeChars, fileLength, vLength);
			free(fileSizeChars);
			setSize = true;
			break;
		case T_FILE_NAME:
			if (setName) {
				break;
			}
			*filename = malloc(vLength);
			if (filename == NULL) {
				perror("processStartPacket - malloc");
				return -1;
			}
			memcpy(*filename, packet + bytesRead + TLV_V, vLength);
			setName = true;
			break;
		}
		bytesRead += 2 + vLength;
	}

	if (!setSize) {
		printf(
				"processControlPacket(): Start packet did not contain file size.\n");
		return -1;
	}

	return 0;
}

int processEndPacket(char *endPacket, char *startPacket, int packetLength) {
	for (int i = 1; i < packetLength; i++) {
		if (endPacket[i] != startPacket[i]) {
			printf(
					"processEndPacket(): End packet does not match start packet.\n");
			return -1;
		}
	}
	return 0;
}

int writeLocalFile(char *filename, char *fileBuffer, int fileBufferLength) {
	FILE *fp = fopen(filename, "wb");

	if (fp == NULL) {
		perror("writeLocalFile - fopen");
		return -1;
	}

	if (fwrite(fileBuffer, 1, fileBufferLength, fp) == -1) {
		perror("writeLocalFile - write");
		fclose(fp);
		return -1;
	}

	if (fclose(fp) == EOF) {
		perror("writeLocalFile - fclose");
		return -1;
	}
	return 0;
}

void printReceiverReport(int receivedBytes, int originalFileSize, int seqNumMismatches, struct ExecTimes *times) {
	time_t totalSeconds = times->endTime->tv_sec - times->startTime->tv_sec;
	suseconds_t totalMicroseconds;
	if (times->endTime->tv_usec > times->startTime->tv_usec) {
		totalMicroseconds = times->endTime->tv_usec - times->startTime->tv_usec;
	} else {
		totalMicroseconds = times->startTime->tv_usec - times->endTime->tv_usec;
	}

	time_t dataSeconds = times->endDataTime->tv_sec
			- times->startDataTime->tv_sec;
	suseconds_t dataMicroseconds;
	if (times->endDataTime->tv_usec > times->startDataTime->tv_usec) {
		dataMicroseconds = times->endDataTime->tv_usec
				- times->startDataTime->tv_usec;
	} else {
		dataMicroseconds = times->startDataTime->tv_usec
				- times->endDataTime->tv_usec;
	}

	printf("Received bytes: %d out of %d.\n", receivedBytes, originalFileSize);
	printf("Sequence number mismatches: %d.\n", seqNumMismatches);
	printf("Data transfer time: %.3fs.\n",
			dataSeconds + (double) dataMicroseconds / pow(10, 6));
	printf("Total time: %.3fs.\n",
			totalSeconds + (double) totalMicroseconds / pow(10, 6));
}

int appRead(char port[]) {
	struct timeval startTime;
	if (gettimeofday(&startTime, NULL) == -1) {
		perror("appWrite - startup gettimeofday");
	}
	char *filename = NULL;
	char *fileBuffer = NULL, *packet = NULL, *startPacket = NULL;
	int fileBufferLength = 0;
	int fileLength = 0;
	int packetLength = 0;
	bool finished = false;
	int seqNumMismatches = 0;
	int fd = llopen(port, RECEIVER);

	struct timeval startDataTime;
	if (gettimeofday(&startDataTime, NULL) == -1) {
		perror("appWrite - startup gettimeofday");
	}

	while (!finished) {
		if ((packetLength = llread(fd, &packet)) == -1) {
			printf("appRead(): llread() failed\n");
			return -1;
		}
		if (packet == NULL) {
			continue;
		}
		switch (packet[C_APP]) {
		case DATA_PACKET:
			processDataPacket(packet, &fileBuffer, &fileBufferLength,
					&seqNumMismatches);
			printf("Data received: %.2f%%\n",
					(double) fileBufferLength / fileLength * 100);
			break;
		case START_PACKET:
			if (processStartPacket(packet, packetLength, &fileLength, &filename)
					== -1) {
				free(packet);
				return -1;
			}
			startPacket = malloc(packetLength);
			memcpy(startPacket, packet, packetLength);
			break;
		case END_PACKET:
			if (processEndPacket(packet, startPacket, packetLength) == -1) {
				printf("appRead(): processEndPacket failed.\n");
				free(packet);
				free(fileBuffer);
				return -1;
			}
			finished = true;
			break;
		}
		free(packet);
	}

	free(startPacket);

	struct timeval endDataTime;
	if (gettimeofday(&endDataTime, NULL) == -1) {
		perror("appWrite - startup gettimeofday");
	}

	if (llclose(fd) == -1) {
		printf("appRead(): llclose() failed\n");
		free(filename);
		free(fileBuffer);
		return -1;
	}

	if (writeLocalFile(filename, fileBuffer, fileBufferLength) == -1) {
		printf("appRead(): writeLocalFile() failed.\n");
		free(filename);
		free(fileBuffer);
		return -1;
	}

	free(filename);
	free(fileBuffer);

	struct timeval endTime;
	if (gettimeofday(&endTime, NULL) == -1) {
		perror("appWrite - startup gettimeofday");
	}

	struct ExecTimes times = { &startTime, &startDataTime, &endDataTime,
			&endTime };

	printf("\n\n");
	printReceiverReport(fileBufferLength, fileLength, seqNumMismatches, &times);

	return 0;
}

void printTransmitterReport(int bytesSent, int fileSize,
		struct ExecTimes *times) {
	time_t totalSeconds = times->endTime->tv_sec - times->startTime->tv_sec;
	suseconds_t totalMicroseconds;
	if (times->endTime->tv_usec > times->startTime->tv_usec) {
		totalMicroseconds = times->endTime->tv_usec - times->startTime->tv_usec;
	} else {
		totalMicroseconds = times->startTime->tv_usec - times->endTime->tv_usec;
	}

	time_t dataSeconds = times->endDataTime->tv_sec
			- times->startDataTime->tv_sec;
	suseconds_t dataMicroseconds;
	if (times->endDataTime->tv_usec > times->startDataTime->tv_usec) {
		dataMicroseconds = times->endDataTime->tv_usec
				- times->startDataTime->tv_usec;
	} else {
		dataMicroseconds = times->startDataTime->tv_usec
				- times->endDataTime->tv_usec;
	}

	printf("Transmitted bytes: %d out of %d.\n", bytesSent, fileSize);
	printf("Data transfer time: %.3fs.\n",
			dataSeconds + (double) dataMicroseconds / pow(10, 6));
	printf("Total time: %.3fs.\n",
			totalSeconds + (double) totalMicroseconds / pow(10, 6));
}

int appWrite(char port[], char filename[]) {
	struct timeval startTime;
	if (gettimeofday(&startTime, NULL) == -1) {
		perror("appWrite - startup gettimeofday");
	}
	int portFd = llopen(port, TRANSMITTER);
	if (portFd == -1) {
		printf("appWrite(): Failed to open connection.\n");
		return -1;
	}

	int fileSize = -1;
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		perror("appWrite - fopen");
		return -1;
	}

	struct stat statBuf;
	stat(filename, &statBuf);
	fileSize = statBuf.st_size;

	char startPacket[9 + strlen(filename)];
	bzero(startPacket, 9 + strlen(filename));
	startPacket[0] = START_PACKET;
	startPacket[1] = 0;
	startPacket[2] = 4;
	startPacket[3] = (fileSize & 0xFF000000) >> 24;
	startPacket[4] = (fileSize & 0x00FF0000) >> 16;
	startPacket[5] = (fileSize & 0x0000FF00) >> 8;
	startPacket[6] = (fileSize & 0x000000FF);
	startPacket[7] = 1;
	startPacket[8] = strlen(filename);
	memcpy(startPacket + 9, filename, strlen(filename));

	if (llwrite(portFd, startPacket, 9 + strlen(filename)) == -1) {
#ifdef DEBUG
		printf("appWrite(): Failed to send start packet.\n");
#endif
		fclose(fp);
		return -1;
	}

	struct timeval startDataTime;
	if (gettimeofday(&startDataTime, NULL) == -1) {
		perror("appWrite - startup gettimeofday");
	}

	char buffer[1024];
	char n = 0;
	int bytesRead = -2;
	int totalBytesWritten = 0;
	while (bytesRead = fread(buffer, 1, 1024, fp)) {
		char dataPacket[bytesRead + 4];
		bzero(dataPacket, bytesRead + 4);

		dataPacket[0] = DATA_PACKET;
		dataPacket[1] = n % 255;
		dataPacket[2] = bytesRead / 256;
		dataPacket[3] = bytesRead % 256;

		memcpy(dataPacket + 4, buffer, bytesRead);

		int res = -2;
		if ((res = llwrite(portFd, dataPacket, bytesRead + 4)) == -1) {
#ifdef DEBUG
			printf("appWrite(): Failed to send data packet.\n");
#endif
			fclose(fp);
			return -1;
		}
		if (res > 0) {
			totalBytesWritten += bytesRead;
		}
		printf("Data sent: %.2f%%\n",
				(double) totalBytesWritten / fileSize * 100);

		n++;
	}
	if (fclose(fp) == EOF) {
		perror("appWrite - fclose");
		return -1;
	}

	struct timeval endDataTime;
	if (gettimeofday(&endDataTime, NULL) == -1) {
		perror("appWrite - startup gettimeofday");
	}

	char endPacket[9 + strlen(filename)];
	bzero(endPacket, 9 + strlen(filename));
	endPacket[0] = END_PACKET;
	endPacket[1] = 0;
	endPacket[2] = 4;
	endPacket[3] = (fileSize & 0xFF000000) >> 24;
	endPacket[4] = (fileSize & 0x00FF0000) >> 16;
	endPacket[5] = (fileSize & 0x0000FF00) >> 8;
	endPacket[6] = (fileSize & 0x000000FF);
	endPacket[7] = 1;
	endPacket[8] = strlen(filename);
	memcpy(endPacket + 9, filename, strlen(filename));

	if (llwrite(portFd, endPacket, 9 + strlen(filename)) == -1) {
		printf("appWrite(): Failed to send end packet\n");
		return -1;
	}

	if (llclose(portFd) == -1) {
		printf("appWrite(): Failed to disconnect.\n");
		return -1;
	}

	struct timeval endTime;
	if (gettimeofday(&endTime, NULL) == -1) {
		perror("appWrite - startup gettimeofday");
	}

	struct ExecTimes times = { &startTime, &startDataTime, &endDataTime,
			&endTime };

	printf("\n\n");
	printTransmitterReport(totalBytesWritten, fileSize, &times);

	return 0;
}
