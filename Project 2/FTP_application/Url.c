#include "Url.h"

#include <stdlib.h>

void logError(char *msg) {
	printf("ERROR: %s\n", msg);
}

int getSeparatorInds(const char *link, int *colonInd, int *atInd, int *firstSlashInd) {
	*colonInd = -1;
	*atInd = -1;
	*firstSlashInd = -1;

	char *colonAddr = strchr(link, ':');
	char *atAddr = strchr(link, '@');
	char *firstSlashAddr = strchr(link, '/');

	if (firstSlashAddr == NULL) {
		logError("No '/' found between host and path.");
		return -1;
	}

	if (atAddr != NULL && colonAddr == NULL) {
		logError("There must be a ':' if there is a '@'.");
		return -1;
	}

	*colonInd = (colonAddr != NULL) ? (colonAddr - link) : -1;
	*atInd = (atAddr != NULL) ? (atAddr - link) : -1;
	*firstSlashInd = (firstSlashAddr != NULL) ? (firstSlashAddr - link) : -1;

	return 0;
}

int parseUsername(struct Url *url, const char *link) {
	int colonInd = -1, atInd = -1, firstSlashInd = -1;
	getSeparatorInds(link, &colonInd, &atInd, &firstSlashInd);
	if (atInd == -1) {
		url->username = "anonymous";
	} else {
		const int usernameLength = colonInd;
		url->username = malloc(usernameLength + 1);
		strncpy(url->username, link, usernameLength);
		url->username[usernameLength] = 0;
	}

	return 0;
}

int parsePassword(struct Url *url, const char *link) {
	int colonInd = -1, atInd = -1, firstSlashInd = -1;
	getSeparatorInds(link, &colonInd, &atInd, &firstSlashInd);
	if (atInd == -1) {
		url->password = "anonymous";
	} else {
		int passwordLength = atInd - colonInd - 1;
		url->password = malloc(passwordLength + 1);
		strncpy(url->password, link + colonInd + 1, passwordLength);
		url->password[passwordLength] = 0;
	}

	return 0;
}

int parseLogin(struct Url *url, const char *link) {
	parseUsername(url, link);
	parsePassword(url, link);

	return 0;
}

int parseHost(struct Url *url, const char *link) {
	int colonInd = -1, atInd = -1, firstSlashInd = -1;
	getSeparatorInds(link, &colonInd, &atInd, &firstSlashInd);
	int hostLength = -1;
	if (atInd == -1) {
		hostLength = firstSlashInd;
	} else {
		hostLength = firstSlashInd - atInd - 1;
	}
	url->host = malloc(hostLength + 1);
	strncpy(url->host, link + atInd + 1, hostLength);
	url->host[hostLength] = 0;

	return 0;
}

int parsePath(struct Url *url, const char *link) {
	int colonInd = -1, atInd = -1, firstSlashInd = -1;
	getSeparatorInds(link, &colonInd, &atInd, &firstSlashInd);
  const int pathLength = strlen(link) - firstSlashInd - 1;
  url->path = malloc(pathLength + 1);
  strncpy(url->path, link + firstSlashInd + 1, pathLength);
  url->path[pathLength] = 0;

  return 0;
}

int parseUrl(struct Url *url, char *str) {
	if (strncmp(str, "ftp://", strlen("ftp://")) != 0) {
		logError("Link must start with 'ftp://'");
		return -1;
	}
	str += strlen("ftp://");

	if (parseLogin(url, str) == -1) {
		return -1;
	}
	printf("username is - %s\n", url->username);
	printf("password is - %s\n", url->password);

	if (parseHost(url, str) == -1) {
		return -1;
	}
	if (parsePath(url, str) == -1) {
		return -1;
	}
	printf("Host is - %s\n", url->host);
	printf("Path is - %s\n", url->path);

	//-------HOST-------
	// 	char host1[128];
	// 	strcpy(host1, newargv1);
	// 	char *host = strchr(host1,'@');
	// 	host++;
	// 	deleteAfter(host, '/');
	//
	// //-------PATH-------
	// 	char *path = strchr(newargv1,'/');
	// 	path++;
	//
	//
	// 	printf("host is - %s \n", host);
	// 	printf("url-path is - %s \n \n", path);

	return 0;

}

void freeUrl(struct Url *url) {
	free(url->username);
	free(url->password);
	free(url->host);
	free(url->path);
}
