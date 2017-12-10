#include "Url.h"

#include <stdlib.h>

void logError(char *msg) {
	printf("ERROR: %s\n", msg);
}

void initLinkInds(struct LinkIndexes *linkInds) {
	linkInds->colonInd = -1;
	linkInds->atInd = -1;
	linkInds->firstSlashInd = -1;
}

int getSeparatorInds(struct LinkIndexes *linkInds, const char *link) {
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

	linkInds->colonInd = (colonAddr != NULL) ? (colonAddr - link) : -1;
	linkInds->atInd = (atAddr != NULL) ? (atAddr - link) : -1;
	linkInds->firstSlashInd = (firstSlashAddr != NULL) ? (firstSlashAddr - link) : -1;

	return 0;
}

int parseUsername(struct Url *url, const char *link, const struct LinkIndexes *linkInds) {
	if (linkInds->atInd == -1) {
		url->username = "anonymous";
	} else {
		const int usernameLength = linkInds->colonInd;
		url->username = malloc(usernameLength + 1);
		strncpy(url->username, link, usernameLength);
		url->username[usernameLength] = 0;
	}

	return 0;
}

int parsePassword(struct Url *url, const char *link, const struct LinkIndexes *linkInds) {
	if (linkInds->atInd == -1) {
		url->password = "anonymous";
	} else {
		int passwordLength = linkInds->atInd - linkInds->colonInd - 1;
		url->password = malloc(passwordLength + 1);
		strncpy(url->password, link + linkInds->colonInd + 1, passwordLength);
		url->password[passwordLength] = 0;
	}

	return 0;
}

int parseLogin(struct Url *url, const char *link, const struct LinkIndexes *linkInds) {
	parseUsername(url, link, linkInds);
	parsePassword(url, link, linkInds);

	return 0;
}

int parseHost(struct Url *url, const char *link, const struct LinkIndexes *linkInds) {
	int hostLength = -1;
	if (linkInds->atInd == -1) {
		hostLength = linkInds->firstSlashInd;
	} else {
		hostLength = linkInds->firstSlashInd - linkInds->atInd - 1;
	}
	url->host = malloc(hostLength + 1);
	strncpy(url->host, link + linkInds->atInd + 1, hostLength);
	url->host[hostLength] = 0;

	return 0;
}

int parsePath(struct Url *url, const char *link, const struct LinkIndexes *linkInds) {
  const int pathLength = strlen(link) - linkInds->firstSlashInd - 1;
  url->path = malloc(pathLength + 1);
  strncpy(url->path, link + linkInds->firstSlashInd + 1, pathLength);
  url->path[pathLength] = 0;

  return 0;
}

int parseUrl(struct Url *url, char *str) {
	if (strncmp(str, "ftp://", strlen("ftp://")) != 0) {
		logError("Link must start with 'ftp://'");
		exit(1);
	}
	str += strlen("ftp://");

  struct LinkIndexes linkInds;
  initLinkInds(&linkInds);
  getSeparatorInds(&linkInds, str);

	if (parseLogin(url, str, &linkInds) == -1) {
		return -1;
	}

  #ifdef DEBUG_PRINTS
	printf("username is - %s\n", url->username);
	printf("password is - %s\n", url->password);
  #endif

	if (parseHost(url, str, &linkInds) == -1) {
		return -1;
	}
	if (parsePath(url, str, &linkInds) == -1) {
		return -1;
	}

  #ifdef DEBUG_PRINTS
	printf("Host is - %s\n", url->host);
	printf("Path is - %s\n", url->path);
  #endif

	return 0;

}

void freeUrl(struct Url *url) {
	free(url->username);
	free(url->password);
	free(url->host);
	free(url->path);
}
