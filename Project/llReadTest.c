#include "llAPI.h"

int main() {
	char port[] = "/dev/ttyS0";
	int fd = llopen(port, RECEIVER);
	if (fd == -1) {
		printf("llopen failed.\n");
	}

	char *msg = NULL;
	int msgSize = llread(fd, &msg);
	printf("Data size: %d\n", msgSize);
	printf("HEX:\n");
	for (int i = 0; i < msgSize; i++) {
		printf("%x ", msg[i]);
	}
	printf("\n");
	printf("Chars:\n");
	for (int i = 0; i < msgSize; i++) {
		printf("%c ", msg[i]);
	}
	int res = llclose(fd);
	printf("llclose res = %d\n", res);
}
