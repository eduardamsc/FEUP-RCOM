#include "FtpData.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

void readFtp(int cmdSocketFd, char *buf, const int bufLength, char *readData, int *readDataLength) {
  int bytesRead = -1;
  memset(buf, 0, bufLength);
  while (buf[3] != ' ') {
    memset(buf, 0, bufLength);
    bytesRead = recv(cmdSocketFd, buf, bufLength, MSG_DONTWAIT);
  }
  readData = malloc(bytesRead);
  memcpy(readData, buf, bytesRead);
  *readDataLength = bytesRead;
}

int initFtpData(struct FtpData *ftpData, const char *hostName) {
  struct hostent *h;

  if ((h = gethostbyname(hostName)) == NULL) {
    herror("gethostbyname");
    exit(1);
  }

	ftpData->ipAddress = inet_ntoa(*((struct in_addr *) h->h_addr));
  ftpData->dataPort = -1;

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
  readFtp(ftpData->cmdSocketFd, buf, 1024, response, &responseLength);
  printf("%s\n", buf);
  // clear "password required" message
  readFtp(ftpData->cmdSocketFd, buf, 1024, response, &responseLength);

  const int passwordLength = strlen("PASS ") + strlen(url->password) + strlen("\n");
  char *passwordMsg = malloc(passwordLength + 1);
  passwordMsg[0] = 0;
  strcat(passwordMsg, "PASS ");
  strcat(passwordMsg, url->password);
  strcat(passwordMsg, "\n");

  send(ftpData->cmdSocketFd, passwordMsg, strlen(passwordMsg), 0);
  response = NULL;
  responseLength = -1;
  readFtp(ftpData->cmdSocketFd, buf, 1024, response, &responseLength);
  printf("%s\n", buf);
}

void setPassive(const struct FtpData *ftpData, int *dataPort) {
  char buf[1024];

  send(ftpData->cmdSocketFd, "PASV\n", strlen("PASV\n"), 0);
  char *response = NULL;
  int responseLength = -1;
  readFtp(ftpData->cmdSocketFd, buf, 1024, response, &responseLength);
  printf("%s\n", buf);

  #define DiscardUptoOpenParen "%*[^(]"
  #define DiscardUptoCloseParen "%*[^)]"
  int ipPart1, ipPart2, ipPart3, ipPart4, dataPortPart1, dataPortPart2;
  sscanf(buf, DiscardUptoOpenParen "(%d,%d,%d,%d,%d,%d)",
        &ipPart1, &ipPart2, &ipPart3, &ipPart4, &dataPortPart1, &dataPortPart2);
  *dataPort = 256 * dataPortPart1 + dataPortPart2;
  #undef DiscardUptoOpenParen
  #undef DiscardUptoCloseParen
}

int setupConnection(struct FtpData *ftpData, const struct Url *url) {
  ftpData->cmdSocketFd = openCmdSocket(ftpData);
  sendLogin(ftpData, url);

  int dataPort = -1;
  setPassive(ftpData, &dataPort);

  return 0;
}

void closeConnection(struct FtpData *ftpData) {
  close(ftpData->cmdSocketFd);
}
