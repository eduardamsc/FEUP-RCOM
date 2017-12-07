struct Url {
	char *username;
	char *password;
	char *host;
	char *path;
}

parseUrl(struct Url &url, char[] str) {
	char *ret;

	ret = strchr(str, '/');
	printf("%s",ret);

}
