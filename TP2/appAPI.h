#include "llAPI.h"

int appRead(int port) {
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
