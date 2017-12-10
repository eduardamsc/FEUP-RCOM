#ifndef FTP_DATA_H
#define FTP_DATA_H

#include "Url.h"

#define FTP_CMD_PORT 21

struct FtpData {
  char *ipAddress;
  int dataPort;
  int cmdSocketFd;
};

/**
 * Gets IP address from host name.
 */
int initFtpData(struct FtpData *ftpData, const char *hostName);

/**
 *  Logs in, sets passive mode, reads server data port
 */
int setupConnection(struct FtpData *ftpData, const struct Url *url);

/**
 *
 */
int downloadFile(struct FtpData *ftpData);

/**
 * Closes sockets.
 */
void closeConnection(struct FtpData *ftpData);

#endif // FTP_DATA_H
