/*Non-Canonical Input Processing*/

#include "appAPI.h"

#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

volatile int STOP = FALSE;

int main(int argc, char** argv) {
#ifdef DEBUG
	printf("Debug mode: ON.\n");
#endif

	if (argc < 2) {
		printf("Usage:\tSerialPort\n\tex: /dev/ttyS0\n");
		exit(1);
	}

	if (appRead(argv[1]) == -1) {
		printf("appRead() failed.\n");
		return -1;
	}

	return 0;
}
