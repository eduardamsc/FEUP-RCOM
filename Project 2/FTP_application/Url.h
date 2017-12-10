#ifndef URL_H
#define URL_H

#include <stdio.h>
#include <string.h>

void logError(char *msg);

struct Url {
	char *username;
	char *password;
	char *host;
	char *path;
};

struct LinkIndexes {
	int colonInd;
	int atInd;
	int firstSlashInd;
};

void initLinkInds(struct LinkIndexes *linkInds);

int getSeparatorInds(struct LinkIndexes *linkInds, const char *link);


int parseUsername(struct Url *url, const char *link, const struct LinkIndexes *linkInds);

int parsePassword(struct Url *url, const char *link, const struct LinkIndexes *linkInds);

int parseLogin(struct Url *url, const char *link, const struct LinkIndexes *linkInds);

int parseHost(struct Url *url, const char *link, const struct LinkIndexes *linkInds);

int parsePath(struct Url *url, const char *link, const struct LinkIndexes *linkInds);

int parseUrl(struct Url *url, char *str);

void freeUrl(struct Url *url);

#endif
