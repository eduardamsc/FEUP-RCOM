#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h> 
#include <netdb.h> 
#include <sys/types.h>
#include <netinet/in.h> 
#include<arpa/inet.h>

void deleteAfter (char* myStr, char splitvalue){

    char *del = myStr;

    while (del < &myStr[strlen(myStr)] && *del != splitvalue)
        del++;

    if (*del== splitvalue)
        *del= '\0';

    return;
}


int main(int argc, char *argv[]) {

		if (argc != 2) {  
            fprintf(stderr,"usage: getip address\n");
            exit(1);
        }


//-------NEW_ARGV_1-------
	char *newargv1;
	newargv1 = argv[1] + 6;

	printf("\n");

	if (strpbrk(newargv1, "@") != NULL){


//-------USERNAME-------
		char username[128];
		strcpy(username, newargv1);
		deleteAfter(username, ':');

//-------PASSWORD-------
		char password1[128];
		strcpy(password1, newargv1);
		char *password = strchr(password1,':');
		password++;
		deleteAfter(password, '@');

		printf("username is - |%s| \n", username);

		printf("password is - |%s| \n", password);

	}


//-------HOST-------
	char host1[128];
	strcpy(host1, newargv1);
	char *host = strchr(host1,'@');
	host++;
	deleteAfter(host, '/');

//-------PATH-------
	char *path = strchr(newargv1,'/');
	path++;


	printf("host is - |%s| \n", host);

	printf("url-path is - |%s| \n \n", path);





	struct hostent *h;
        

        if ((h=gethostbyname(host)) == NULL) {  
            herror("gethostbyname");
            exit(1);
        }

		char *ipaddress = inet_ntoa(*((struct in_addr *)h->h_addr));

        printf("Host name  : %s\n", h->h_name);
        printf("IP Address : %s\n",inet_ntoa(*((struct in_addr *)h->h_addr)));


	int	sockfd;
	struct	sockaddr_in server_addr;
	char	buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";  
	int	bytes;
	
	//server address handling
	bzero((char*)&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ipaddress);	//32 bit Internet address network byte ordered
	server_addr.sin_port = htons(21);		//server TCP port must be network byte ordered 
    
	//open an TCP socket
	if ((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0) {
    		perror("socket()");
        	exit(0);
    	}
	//connect to the server
    	if(connect(sockfd, 
	           (struct sockaddr *)&server_addr, 
		   sizeof(server_addr)) < 0){
        	perror("connect()");
		exit(0);
	}
    	//send a string to the server
	char loginMsg[60] = "LOGIN";
	strcat(loginMsg, username);
	bytes = send(sockfd, loginMsg, strlen(buf));
	printf("Bytes escritos %d\n", bytes);

	char passMsg[60] = "PASSWORD";
	strcat(passMsg, password);
	bytes = send(sockfd, passMsg, strlen(buf));
	printf("Bytes escritos %d\n", bytes);

	

	close(sockfd);



return 0;

}

