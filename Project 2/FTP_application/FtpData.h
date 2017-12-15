#ifndef FTP_DATA_H
#define FTP_DATA_H

#include "Url.h"

#define FTP_CMD_PORT 21

struct FtpData {
  char *ipAddress;
  int dataPort;
  int cmdSocketFd;
  int dataSocketFd;
};

/**
 * Gets IP address from host name.
 */
int initFtpData(struct FtpData *ftpData, const char *hostName);

/**
 *  Logs in, sets passive mode, opens server data socket.
 */
int setupConnection(struct FtpData *ftpData, const struct Url *url);

/**
 * Downloads file.
 */
int downloadFile(struct FtpData *ftpData, const char *filePath);

/**
 * Closes sockets.
 */
void closeConnection(struct FtpData *ftpData);

#endif // FTP_DATA_H
