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

#define T_FILE_SIZE '0'
#define T_FILE_NAME '1'

#define DATA_PACKET '1'
#define START_PACKET '2'
#define END_PACKET '3'

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
        memcpy(filename, packet + bytesRead + TLV_V, vLength);
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
  for (int i = 0; i < packetLength; i++) {
    if (endPacket[i] != startPacket[i]) {
      printf("processEndPacket(): End packet does not match start packet.\n");
      return -1;
    }
  }
  return 0;
}

int writeLocalFile(char *filename, char *fileBuffer, int fileBufferLength) {
  int fd = open(filename, O_CREAT, S_IRWXU | S_IRWXG | S_IROTH);
  if (fd == -1) {
    printf("writeLocalFile(): File creation failed.\n");
    return -1;
  }
  if (write(fd, fileBuffer, fileBufferLength) == -1) {
    printf("writeLocalFile(): File write failed.\n");
    close(fd);
    return -1;
  }
  if (close(fd) == -1) {
    printf("writeLocalFile(): File closing failed.\n");
    return -1;
  }
  return 0;
}

int appRead(char port[]) {
  char *filename;
  char *fileBuffer = NULL, *packet = NULL, *startPacket = NULL;
  int fileBufferLength = 0;
  int fileLength = 0;
  int packetLength = 0;
  bool finished = false;
  int fd = llopen_read(port);

  while (!finished) {
    if ((packetLength = llread(fd, packet)) == -1) {
      printf("appRead(): llread() failed\n");
      return -1;
    }
    switch (packet[C_APP]) {
      case DATA_PACKET:
        processDataPacket(packet, &fileBuffer, &fileBufferLength);
        break;
      case START_PACKET:
        if (processStartPacket(packet, packetLength, &fileLength, &filename) == -1) {
          printf("appRead(): processStartPacket failed.\n");
          return -1;
        }
        startPacket = malloc(packetLength);
        memcpy(startPacket, packet, packetLength);
        break;
      case END_PACKET:
        if (processEndPacket(packet, startPacket, packetLength) == -1) {
          printf("appRead(): processEndPacket failed.\n");
          return -1;
        }
        finished = true;
        break;
    }
  }

  if (llclose_Receiver(fd) == -1) {
      printf("appRead(): llclose_Receiver() failed\n");;
      return -1;
  }

  if (writeLocalFile(filename, fileBuffer, fileBufferLength) == -1) {
    printf("appRead(): writeLocalFile() failed.\n");
    return -1;
  }

  return 0;
}

int appWrite(char port[], char filename[]) {
  int portFd = llopen(port);

  int sz;
  int fd = open(filename,O_RDONLY);

  char buffer[20];
  char n = 0;

  FILE *fp = fdopen(fd, "r");
  if (fp == NULL) {
    printf("fdopen failed.\n");
    return -1;
  }
  fseek(fp, 0L, SEEK_END);
  sz = ftell(fp);

  char startPacket[8 + strlen(filename)];
  bzero(startPacket, 8 + strlen(filename));
  startPacket[0]=2;
  startPacket[1]=0;
  startPacket[2]=4;
  startPacket[3]=(sz&0xFF000000)>>24;
  startPacket[4]=(sz&0x00FF0000)>>16;
  startPacket[5]=(sz&0x0000FF00)>>8;
  startPacket[6]=(sz&0x000000FF);
  startPacket[7]=1;
  startPacket[8]=strlen(filename);
  strcpy(startPacket+9,filename);

  llwrite(portFd,startPacket,9+strlen(filename));

  while(read(fd,buffer,20)){
    char dataPacket[24];
    bzero(dataPacket, 24);

    dataPacket[0] = 1;
    dataPacket[1] = n%255;
    dataPacket[2] = 0;
    dataPacket[3] = 20;

    strcpy(dataPacket+4, buffer);

    llwrite(portFd,dataPacket,24);

    n++;
  }

  char endPacket[8 + strlen(filename)];
  bzero(endPacket, 8 + strlen(filename));
  endPacket[0]=3;
  endPacket[1]=0;
  endPacket[2]=4;
  endPacket[3]=(sz&0xFF000000)>>24;
  endPacket[4]=(sz&0x00FF0000)>>16;
  endPacket[5]=(sz&0x0000FF00)>>8;
  endPacket[6]=(sz&0x000000FF);
  endPacket[7]=1;
  endPacket[8]=strlen(filename);
  strcpy(endPacket+9,filename);

  llwrite(portFd,endPacket,9+strlen(filename));

  llclose_Transmitter(portFd);
  return 0;
}
