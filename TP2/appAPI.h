#include "llAPI.h"

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

int readFileSize(char *fileSizeChars, int &fileLength, int arrayLength) {
  scanf(fileSizeChars, "%d", fileLength);
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
  while (int i = 0; i < dataSize; i++) {
    (*fileBuffer)[fileBufferLength + i] = packet[DATA_P1 + i];
  }
  *fileBufferLength += dataSize;

  prevSeqNum = sequenceNumber;
  return 0;
}

/**
 * Reads file length and filename from packet, if they exist.
 */
int processStartPacket(char *packet, int packetLength, int &fileLength, char **filename) {
  bool setName = false, setSize = false;
  int bytesRead = 1;

  while (bytesRead < packetLength) {
    int vLength = packet[bytesRead + TLV_L];
    switch (packet[bytesRead + TLV_T]) {
      case T_FILE_SIZE:
        if (setSize) {
          break;
        }
        char *fileSizeChars = malloc(v1Length + 1);
        memcpy(fileSizeChars, packet+V1_APP, v1Length);
        fileSizeChars[v1Length] = '\0';
        readFileSize(fileSizeChars, fileLength, v1Length);
        setSize = true;
        break;
      case T_FILE_NAME:
        if (setName) {
          break;
        }
        memcpy(filename, packet+V1_APP, v1Length);
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

int processEndPacket(char *startPacket, char *startPacket, int packetLength) {
  for (int i = 0; i < packetLength; i++) {
    if (startPacket[i] != startPacket[i]) {
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
}

int appRead(int port) {
  char filename[];
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
    switch (packet[C_APP])) {
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

int appWrite(int port, char filename[]) {
  llopen(port);

  int sz;
  int fd = open(filename,O_RDONLY);

  char buffer[20];
  char n = 0;

  fseek(fd, 0L, SEEK_END);
  sz = ftell(fd);

  char StartPacket[];
  StartPacket[0]=2;
  StartPacket[1]=0;
  StartPacket[2]=4;
  StartPacket[3]=(sz&OxFF000000)>>24;
  StartPacket[4]=(sz&Ox00FF0000)>>16;
  StartPacket[5]=(sz&Ox0000FF00)>>8;
  StartPacket[6]=(sz&Ox000000FF);
  StartPacket[7]=1;
  StartPacket[8]=strlen(filename);
  strcpy(StartPacket+9,filename);

  llwrite(fd,StartPacket,9+strlen(filename));

  while(read(fd,buffer,20)){
    char DataPacket[24];

    DataPacket[0] = 1;
    DataPacket[1] = n%255;
    DataPacket[2] = 0;
    DataPacket[3] = 20;

    strcpy(DataPacket+4, buffer);

    llwrite(fd,DataPacket,28);

    n++;
  }

  char EndPacket[];
  EndPacket[0]=3;
  EndPacket[1]=0;
  EndPacket[2]=4;
  EndPacket[3]=(sz&OxFF000000)>>24;
  EndPacket[4]=(sz&Ox00FF0000)>>16;
  EndPacket[5]=(sz&Ox0000FF00)>>8;
  EndPacket[6]=(sz&Ox000000FF);
  EndPacket[7]=1;
  EndPacket[8]=strlen(filename);
  strcpy(EndPacket+9,filename);

  llwrite(fd,EndPacket,9+strlen(filename));



  llclose_Transmitter(port);
  return 0;
}
