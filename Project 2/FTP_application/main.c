#include <stdio.h>
#include <stdlib.h>

#include "Url.h"

void printUsage(char progName[]) {
	printf("USAGE: %s ftp://[<user>:<password>@]<host>/<url-path>", progName);
}

int main(int argc, char *argv[]) {
	if (argc != 2) {
		printUsage(argv[0]);
		exit(1);
	}

	struct Url url;
	memset(&url, 0, sizeof(url));
	parseUrl(&url, argv[1]);

	// FtpData ftp;
	// setupConnection(&url, &ftp);
	//
	// downloadFile(&url, &ftp);

	//printResult;

	return 0;
}
