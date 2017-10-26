#include "llAPI.h"
#include <stdio.h>

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

int readFileSize(char *fileSizeChars, int *fileLength, int arrayLength) {
  if (scanf(fileSizeChars, "%d", fileLength) == EOF) {
    printf("readFileSize(): scanf failed.\n");
    return -1;
  }
  return 0;
}

/**
 * Reads data in packet to fileBuffer sequentially, reallocating it.
 */
int processDataPacket(char *packet, char **fileBuffer, int *fileBufferLength) {
  static int prevSeqNum = -1;

  int sequenceNumber = packet[DATA_N];
  if (prevSeqNum == -1) {
    prevSeqNum = sequenceNumber;
  }
  if ((prevSeqNum + 1) % 255 != sequenceNumber) {
    printf("Warning: Sequence number mismatch in data packet.\n");
  }

  int dataSize = 256 * packet[DATA_L2] + packet[DATA_L1];
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
int processStartPacket(char *packet, int packetLength, int *fileLength, char **filename) {
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
        setSize = true;
        break;
      case T_FILE_NAME:
        if (setName) {
          break;
        }
        *filename = malloc(vLength);
        memcpy(*filename, packet + bytesRead + TLV_V, vLength);
        setName = true;
        break;
    }
    bytesRead += 2 + vLength;
  }

  if (!setSize) {
    printf("processControlPacket(): Start packet did not contain file size.\n");
    return -1;
  }

  return 0;
}

int processEndPacket(char *endPacket, char *startPacket, int packetLength) {
  for (int i = 1; i < packetLength; i++) {
    if (endPacket[i] != startPacket[i]) {
      printf("processEndPacket(): End packet does not match start packet.\n");
      return -1;
    }
  }
  return 0;
}

int writeLocalFile(char *filename, char *fileBuffer, int fileBufferLength) {
  int fd = open(filename, O_CREAT | O_WRONLY, 0777);
  if (fd == -1) {
    perror("writeLocalFile - open");
    //printf("writeLocalFile(): File creation failed.\n");
    return -1;
  }
  int res = 0;
  while ((res = write(fd, fileBuffer, fileBufferLength)) > 0) {
    if (res == -1) {
      perror("writeLocalFile - write");
    }
    // printf("writeLocalFile(): File write failed.\n");
    close(fd);
    return -1;
  }
  if (close(fd) == -1) {
    perror("writeLocalFile - close");
    // printf("writeLocalFile(): File closing failed.\n");
    return -1;
  }
  return 0;
}

int appRead(char port[]) {
  char *filename = NULL;
  char *fileBuffer = NULL, *packet = NULL, *startPacket = NULL;
  int fileBufferLength = 0;
  int fileLength = 0;
  int packetLength = 0;
  bool finished = false;
  int fd = llopen_read(port);

  while (!finished) {
    if ((packetLength = llread(fd, &packet)) == -1) {
      printf("appRead(): llread() failed\n");
      return -1;
    }
    switch (packet[C_APP]) {
      case DATA_PACKET:
        printf("Before processDataPacket\n");
        processDataPacket(packet, &fileBuffer, &fileBufferLength);
        printf("After processDataPacket\n");
        break;
      case START_PACKET:
        printf("Before processStartPacket\n");
        if (processStartPacket(packet, packetLength, &fileLength, &filename) == -1) {
          printf("appRead(): processStartPacket failed.\n");
          return -1;
        }
        printf("After processStartPacket\n");
        startPacket = malloc(packetLength);
        memcpy(startPacket, packet, packetLength);
        break;
      case END_PACKET:
        printf("Before processEndPacket\n");
        if (processEndPacket(packet, startPacket, packetLength) == -1) {
          printf("appRead(): processEndPacket failed.\n");
          return -1;
        }
        printf("After processEndPacket\n");
        finished = true;
        break;
    }
  }

  if (llclose_Receiver(fd) == -1) {
      printf("appRead(): llclose_Receiver() failed\n");;
      return -1;
  }

  if (writeLocalFile(filename, fileBuffer, fileLength) == -1) {
    printf("appRead(): writeLocalFile() failed.\n");
    return -1;
  }

  return 0;
}

int appWrite(char port[], char filename[]) {
  int portFd = llopen(port);

  int fileSize = -1;
  int fd = open(filename,O_RDONLY);

  char buffer[20];
  char n = 0;

  struct stat statBuf;
  fstat(fd, &statBuf);
  fileSize = statBuf.st_size;

  char startPacket[9 + strlen(filename)];
  bzero(startPacket, 9 + strlen(filename));
  startPacket[0] = START_PACKET;
  startPacket[1] = 0;
  startPacket[2] = 4;
  startPacket[3] = (fileSize & 0xFF000000)>>24;
  startPacket[4] = (fileSize & 0x00FF0000)>>16;
  startPacket[5] = (fileSize & 0x0000FF00)>>8;
  startPacket[6] = (fileSize & 0x000000FF);
  startPacket[7] = 1;
  startPacket[8] = strlen(filename);
  strncpy(startPacket + 9,filename, strlen(filename));

  llwrite(portFd,startPacket,9+strlen(filename));

  int readRes = -2;
  while (readRes = read(fd,buffer,20)){
    char dataPacket[24];
    bzero(dataPacket, 24);

    dataPacket[0] = DATA_PACKET;
    dataPacket[1] = n%255;
    dataPacket[2] = 0;
    dataPacket[3] = 20;

    strcpy(dataPacket+4, buffer);

    llwrite(portFd,dataPacket,24);

    n++;
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
  strncpy(endPacket + 9, filename, strlen(filename));

  llwrite(portFd,endPacket,9+strlen(filename));

  llclose_Transmitter(portFd);
  return 0;
}
