#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#define BUFSIZE 2097152

#define SOCKET_ERROR -1

/*
struct sockaddr_in
{
	short sin_family; // 주소 체계: AF_INET 
	u_short sin_port; // 16 비트 포트 번호, network byte order 
	struct in_addr sin_addr; // 32 비트 IP 주소 
	char sin_zero[8]; // 전체 크기를 16 비트로 맞추기 위한 dummy 
};*/
struct sockaddr_in clientaddr[100];

int sock_id[100];
int sock_num = 0;

void * function(void *arg){ // arg is from thread of main
	int sckfd = *((int *)arg);
	int index;

	for(int i=0; i<sock_num; i++){
		if(sckfd == sock_id[i]){
			index = i;
			break;
		}
	}

	/* get ip address from domain name
	#include <netdb.h>
	struct hostent
	{
		char * h_name; 
		char ** h_aliases;
		int h_addrtype;
		int h_length;
		char ** h_addr_list;
	}
	struct hostent * gethostbyaddr(const char * addr, socklen_t len, int family);*/
	struct hostent * host = gethostbyaddr((const char *)&clientaddr[index].sin_addr.s_addr, sizeof(clientaddr[index].sin_addr.s_addr), AF_INET);
	if(host == NULL){
		perror(">>>>> gethostbyaddr error <<<<<");
		exit(1);
	}

	char * host_addr = inet_ntoa(*(struct in_addr *)&clientaddr[index].sin_addr);
	if(host_addr == NULL){
		perror(">>>>> inet_ntoa error <<<<<");
		exit(1);
	}
	char buf[BUFSIZE]; // consider maxlen of http get request
	struct sockaddr_in serveraddr;
	char http_host[100];
	while(1){
		memset(buf, 0, BUFSIZE);
		ssize_t f = read(sckfd, buf, BUFSIZE);
		if(f == 0) // read until meet EOF / if meet EOF, read returns 0
			break;
		printf("%s", buf);
		int len = strlen(buf);

		for(int i=0; i<len; i++){
			if(buf[i] !='H')
				continue;
			else{
				if(strncmp(&buf[i], "Host: ", 6) != 0)
					continue;
				else{
					i = i+6;
					int j = i;
					while(!((buf[i] == 0x0d) && (buf[i+1] == 0x0a))){
						http_host[i-j] = buf[i];
						i++;
					}
					http_host[i]=0;
				}
			}
		}

		printf("<%s>", http_host);

	
		int sock = socket(AF_INET, SOCK_STREAM, 0);
		if(sock == SOCKET_ERROR){
			perror(">>>>> socket opening error <<<<<");
			exit(1);
		}

		printf("%d", sock);

		struct hostent * server = gethostbyname(http_host);
		if(server == NULL){
			perror(">>>>> there are no such host <<<<<<");
			exit(1);
		}

		memset((char *)&serveraddr, 0, sizeof(serveraddr));
		serveraddr.sin_family = AF_INET;
		serveraddr.sin_port = htons(80);
		memcpy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);

		serveraddr.sin_addr.s_addr = inet_addr(inet_ntoa(*(struct in_addr*)server->h_addr_list[0]));

		// int connect(int sockfd, const struct sockaddr *serv_addr, socklen_t addrlen);
		int retval = connect(sock, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
		if(retval == SOCKET_ERROR){
			perror(">>>>> socket connecting error <<<<<");
			exit(1);
		}

		int fd = write(sock, buf, len);
		if(fd < 0){
			perror(">>>>> socket writing error <<<<<");
			exit(1);
		}
    
        fd = read(sock, buf, BUFSIZE); //get http reply

        fd = write(sckfd, buf, len); //relay http request
        if (fd < 0){
			perror(">>>>> socket writing error <<<<<");
			exit(1);
		}
        
        printf("[+] Relay Success!!!\n");
        close(sock);
	}
}


	

int main(int argc, char **argv){
	pthread_t thread;
	int retval;
	struct sockaddr_in serveraddr; //sockaddr_in used if AF_INET
	struct sockaddr_in temp_addr;

	if(argc != 2){
		perror(">>>>> usage error <<<<<");
		exit(1);
	}

	int port = atoi(argv[1]);


	/* ----- opening socket -----
	#include <sys/types.h>
	#include <sys/socket.h>
	int socket(int family, int type, int protocol);
	sckfd =socket(AF_INET,SOCK_STREAM,0);*/
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd == SOCKET_ERROR){
		perror(">>>>> socket opening error <<<<<");
		exit(1);
	}


	/* ----- set socket option as SO_REUSEADDR -----
	   BOOL optval = TRUE;
	   retval = setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));*/
	int optval = 1;
	retval  = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
	if(retval == SOCKET_ERROR){
		perror(">>>>> socket option setting error <<<<<");
		exit(1);
	}


	/* ----- bind socket -----
	   ZeroMemory(&serveraddr, sizeof(serveraddr));
	   serveraddr.sin_family = AF_INET;
	   serveraddr.sin_port = htons(9000);
	   serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	   retval = bind(listen_sock, (SOCKADDR*)&serveraddr,
	   sizeof(serveraddr));*/
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	retval = bind(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
	if(retval == SOCKET_ERROR){
		perror(">>>>> socket binding error <<<<<");
		exit(1);
	}


	/* ----- listen -----
	   retval = listen(listen_sock, SOMAXCONN);*/
	retval = listen(fd, 5);
	if(retval == SOCKET_ERROR){
		perror(">>>>> socket listening error <<<<<");
		exit(1);
	}


	int len = sizeof(struct sockaddr_in);
	while(1){
		// int accept(int s, struct sockaddr *addr, socklen_t *addrlen);
		// addrlen = size of which 'addr' is pointing
		int sckfd = accept(fd, (struct sockaddr *)&temp_addr, &len);
		if(sckfd == SOCKET_ERROR){
			perror(">>>>> socket accepting error <<<<<");
			exit(1);
		}

		clientaddr[sock_num] = temp_addr;
		sock_id[sock_num] = sckfd;
		sock_num++;

		// int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
		int thr_id = pthread_create(&thread, NULL, function, (void *)&sckfd);
		if(thr_id != 0){ // return 0 if thread is successfully creating
			perror(">>>>> thread creating error <<<<<");
			exit(1);
		}
		// int pthread_detach(pthread_t th);
		pthread_detach(thread);
	}
}
