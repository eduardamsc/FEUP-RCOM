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

void logError(char *msg) {
	printf("ERROR: %s\n", msg);
}

// void deleteAfter (char* myStr, char splitvalue){
//     char *del = myStr;
//     while (del < &myStr[strlen(myStr)] && *del != splitvalue) {
//       del++;
// 		}
//     if (*del== splitvalue) {
//       *del= '\0';
// 		}
// }

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

	*colonInd = colonAddr - link;
	*atInd = atAddr - link;
	*firstSlashInd = atAddr - link;

	return 0;
}

int parseUsername(struct Url *url, const char *link) {
	int colonInd, atInd, firstSlashInd;
	// stuff TODO
	url->username = malloc(usernameLength + 1);
	strncpy(url->username, link, usernameLength);
	url->username[usernameLength] = 0;
}

void parsePassword(struct Url *url, const char *link) {
	char *atAddr = strchr(link, '@');
	char *colonAddr = strchr(link, ':');
	int passwordLength = linkInds->atInd - linkInds->colonInd;
	char *password = malloc(passwordLength + 1);
	strncpy(url->username, link + linkInds->colonInd + 1, passwordLength);
	password[passwordLength] = 0;
}

int parseLogin(struct Url *url, char *link) {
	char *colonAddr = strchr(str, ':');
	if (colonAddr == NULL) {
		logError("Link must contain a colon when a @ exists.");
		return -1;
	}

	int colonInd = colonAddr - link;
	url->username = parseUsername(link, colonInd);
	url->password = parsePassword(link, colonInd);

	return 0;
}

void setAnonLogin(struct Url *url) {
	url->username = "anonymous";
	url->password = "anonymous";
}

int parseUrl(struct Url *url, char *str) {
	if (strncmp(str, "ftp://", strlen("ftp://")) != 0) {
		logError("Link must start with 'ftp://'");
		return -1;
	}
	str += strlen("ftp://");

	// struct LinkIndexes linkInds;
	// setLinkInds(str, &linkInds);

	if (strpbrk(str, "@") != NULL) {
		if (parseLogin(url, str) == -1) {
			return -1;
		}
	} else {
		setAnonLogin(url);
	}
	printf("username is - %s\n", url->username);
	printf("password is - %s\n", url->password);


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

}

void freeUrl(struct Url *url) {
	free(url->username);
	free(url->password);
	free(url->host);
	free(url->path);
}
