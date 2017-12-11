#include "FtpData.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define DiscardUptoOpenParen "%*[^(]"
#define DiscardUptoCloseParen "%*[^)]"

void logFtpError(char *msg) {
	printf("ERROR: %s\n", msg);
}



int msgCode(const char *msg) {
	int code = -1;
	char trash[1024];
	sscanf(msg, "%d%s", &code, trash);
	return code;
}

int getFileSize(const char *msg) {
	int fileSize = -1;
	sscanf(msg, DiscardUptoOpenParen "(%d bytes).", &fileSize);
	return fileSize;
}

void readFtp(int socketFd, char *buf, const int bufLength, char **readData, int *readDataLength) {
  int bytesRead = -1;
  memset(buf, 0, bufLength);
  while (buf[3] != ' ') {
    memset(buf, 0, bufLength);
    bytesRead = recv(socketFd, buf, bufLength, MSG_DONTWAIT);
  }
  *readData = malloc(bytesRead);
  memcpy(*readData, buf, bytesRead);
  *readDataLength = bytesRead;

	//#ifdef DEBUG_PRINTS
	printf("%s", *readData);
	//#endif
}

int initFtpData(struct FtpData *ftpData, const char *hostName) {
  struct hostent *h;

  if ((h = gethostbyname(hostName)) == NULL) {
    herror("gethostbyname");
    exit(1);
  }

	ftpData->ipAddress = inet_ntoa(*((struct in_addr *) h->h_addr));
  ftpData->dataPort = -1;
	ftpData->cmdSocketFd = -1;
	ftpData->dataSocketFd = -1;

  return 0;
}

int openCmdSocket(const struct FtpData *ftpData) {
  int cmdSocketFd;
  struct sockaddr_in server_addr;

  bzero((char *) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ftpData->ipAddress);	//32 bit Internet address network byte ordered
	server_addr.sin_port = htons(FTP_CMD_PORT);

	if ((cmdSocketFd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
  	perror("socket()");
    exit(1);
  }

  if(connect(cmdSocketFd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
    perror("connect()");
    exit(1);
  }

  return cmdSocketFd;
}

int openDataSocket(struct FtpData *ftpData) {
  int dataSocketFd;
  struct sockaddr_in server_addr;

  bzero((char *) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ftpData->ipAddress);	//32 bit Internet address network byte ordered
	server_addr.sin_port = htons(ftpData->dataPort);

  if ((dataSocketFd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
  	perror("socket()");
		closeConnection(ftpData);
    exit(1);
  }

  if(connect(dataSocketFd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0){
    perror("connect()");
		closeConnection(ftpData);
    exit(1);
  }

  return dataSocketFd;
}

// TODO: Wrong login.
void sendLogin(const struct FtpData *ftpData, const struct Url *url) {
  char buf[1024];

  const int userMsgLength = strlen("USER ") + strlen(url->username) + strlen("\n");
  char *userMsg = malloc(userMsgLength + 1);
  userMsg[0] = 0;
  strcat(userMsg, "USER ");
  strcat(userMsg, url->username);
  strcat(userMsg, "\n");

  send(ftpData->cmdSocketFd, userMsg, strlen(userMsg), 0);
  char *response = NULL;
  int responseLength = -1;
  readFtp(ftpData->cmdSocketFd, buf, 1024, &response, &responseLength);
  //printf("%s\n", buf);
  // clear "password required" message
	if (msgCode(response) == 220) {
  	readFtp(ftpData->cmdSocketFd, buf, 1024, &response, &responseLength);
	}

  const int passwordLength = strlen("PASS ") + strlen(url->password) + strlen("\n");
  char *passwordMsg = malloc(passwordLength + 1);
  passwordMsg[0] = 0;
  strcat(passwordMsg, "PASS ");
  strcat(passwordMsg, url->password);
  strcat(passwordMsg, "\n");

  send(ftpData->cmdSocketFd, passwordMsg, strlen(passwordMsg), 0);
  response = NULL;
  responseLength = -1;
  readFtp(ftpData->cmdSocketFd, buf, 1024, &response, &responseLength);
  //printf("%s\n", buf);
}

void setPassive(const struct FtpData *ftpData, int *dataPort) {
  char buf[1024];

  send(ftpData->cmdSocketFd, "PASV\n", strlen("PASV\n"), 0);
  char *response = NULL;
  int responseLength = -1;
  readFtp(ftpData->cmdSocketFd, buf, 1024, &response, &responseLength);
  //printf("%s\n", buf);

  int ipPart1, ipPart2, ipPart3, ipPart4, dataPortPart1, dataPortPart2;
  sscanf(buf, DiscardUptoOpenParen "(%d,%d,%d,%d,%d,%d)",
        &ipPart1, &ipPart2, &ipPart3, &ipPart4, &dataPortPart1, &dataPortPart2);
  *dataPort = 256 * dataPortPart1 + dataPortPart2;
}

int setupConnection(struct FtpData *ftpData, const struct Url *url) {
  ftpData->cmdSocketFd = openCmdSocket(ftpData);
  sendLogin(ftpData, url);

  setPassive(ftpData, &ftpData->dataPort);
  ftpData->dataSocketFd = openDataSocket(ftpData);

  return 0;
}

void sendRetr(const struct FtpData *ftpData, const char *filePath, int *fileSize) {
  char buf[1024];

  const int retrMsgLength = strlen("RETR ") + strlen(filePath) + strlen("\n");
  char *retrMsg = malloc(retrMsgLength + 1);
  retrMsg[0] = 0;
  strcat(retrMsg, "RETR ");
  strcat(retrMsg, filePath);
  strcat(retrMsg, "\n");

  send(ftpData->cmdSocketFd, retrMsg, strlen(retrMsg), 0);
  char *response = NULL;
  int responseLength = -1;
  readFtp(ftpData->cmdSocketFd, buf, 1024, &response, &responseLength);
  //printf("%s\n", buf);
	*fileSize = getFileSize(response);
}

char * getFilenameFromPath(const char *filePath) {
  const char *lastSlashAddr = strrchr(filePath, '/');
  const int lastSlashInd = lastSlashAddr - filePath;
  const int filenameLength = strlen(filePath) - lastSlashInd - 1;
  char *filename = malloc(filenameLength + 1);
  filename[0] = 0;
  strcat(filename, lastSlashAddr + 1);

  return filename;
}

void receiveFile(struct FtpData *ftpData, const char *filePath, const int fileSize) {
  char *filename = getFilenameFromPath(filePath);
  FILE *fp;
  if ((fp = fopen(filename, "wb")) == NULL) {
    logFtpError("Cannot open file '%s' for writing.");
		closeConnection(ftpData);
    exit(1);
  }

  char buf[1024];
  int bytesRead = -1;
	int totalBytesRead = 0;
	printf("\e[?25l"); // hide cursor
  while ((bytesRead = recv(ftpData->dataSocketFd, buf, 1024, MSG_DONTWAIT)) != 0) {
    if (bytesRead == -1) {
      // herror("recv");
      continue;
    }
    if (fwrite(buf, bytesRead, 1, fp) == 0) {
      logFtpError("Local file writing failure.");
    }
		totalBytesRead += bytesRead;
		float transferProgress = (float) totalBytesRead / fileSize * 100.0;
		printf("\rDownload progress: %.1f%% (%d / %d bytes)", transferProgress, totalBytesRead, fileSize);
	}
	printf("\e[?25h"); // display cursor

  fclose(fp);
  free(filename);
}

int downloadFile(struct FtpData *ftpData, const char *filePath) {
	int fileSize = -1;
  sendRetr(ftpData, filePath, &fileSize);
  receiveFile(ftpData, filePath, fileSize);

  return 0;
}

void closeConnection(struct FtpData *ftpData) {
	if (ftpData->cmdSocketFd != -1) {
	  close(ftpData->cmdSocketFd);
	}
	if (ftpData->dataSocketFd != -1) {
		close(ftpData->dataSocketFd);
	}
}
