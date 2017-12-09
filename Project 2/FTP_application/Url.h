#include <stdio.h>
#include <string.h>

struct Url {
	char *username;
	char *password;
	char *host;
	char *path;
};

// struct LinkIndexes {
// 	int colonInd = -1;
// 	int atInd = -1;
// 	int firstSlashInd = -1;
// };
//
// int setLinkInds(const char *link, struct LinkIndexes *linkInds) {
// 	char *atAddr = strchr(str, '@');
// 	if (atAddr != NULL) {
// 			linkInds->atInd = atAddr - link;
// 	}
//
// 	if (linkInds->atInd != -1) {
// 		char *colonAddr = strchr(str, ':');
// 		if (colonAddr == NULL) {
// 			logError("Link must contain a colon when a '@' exists.");
// 			return -1;
// 		}
// 		linkInds->colonInd = colonAddr - link;
// 	}
//
// 	char *firstSlashAddr = strchr(str, '/');
// 	if (firstSlashAddr != NULL) {
// 		linkInds->firstSlashInd = firstSlashAddr - link
// 	}
//
// 	return 0;
// }

void logError(char *msg);


int getSeparatorInds(const char *link, int *colonInd, int *atInd, int *firstSlashInd);

int parseUsername(struct Url *url, const char *link);

int parsePassword(struct Url *url, const char *link);

int parseLogin(struct Url *url, const char *link);

int parseHost(struct Url *url, const char *link);

int parsePath(struct Url *url, const char *link);

int parseUrl(struct Url *url, char *str);

void freeUrl(struct Url *url);
