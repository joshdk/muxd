#define _POSIX_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "config.h"

gid_t gid=500;
uid_t uid=500;


char *strdup(const char *s);


// get sockaddr, IPv4 or IPv6:
//void *get_in_addr(struct sockaddr *sa)
//{
    //if (sa->sa_family == AF_INET) {
        //return &(((struct sockaddr_in*)sa)->sin_addr);
    //}

    //return &(((struct sockaddr_in6*)sa)->sin6_addr);
//}






int main(int argc,char **argv){

	if(argc<5){
		//fail
		return 1;
	}

	char *laddr=strdup(argv[1]);
	char *lport=strdup(argv[2]);
	char *raddr=strdup(argv[3]);
	char *rport=strdup(argv[4]);

	printf("local:  [%s:%s]\n",laddr,lport);
	printf("remote: [%s:%s]\n",raddr,rport);



	int fdlsocket=-1;
	int fdrsocket=-1;




	struct addrinfo hints, *servinfo;


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags=2;//AI_CANONNAME;

	//local host lookup
	if((getaddrinfo(laddr,lport,&hints,&servinfo))!=0){
		perror("[-] getaddrinfo");
		return 1;
	}
	puts("[+] getaddrinfo");

	int lsocket=-1;
	struct sockaddr_in *sa=NULL;
	int bind_success=0;
	for(struct addrinfo *p=servinfo;p!=NULL;p=p->ai_next){

		//create local socket
		if((lsocket=socket(p->ai_family,p->ai_socktype,p->ai_protocol))<0){
			perror("[-] socket");
			continue;
		}else{
			puts("[+] socket");
		}

		//set socket option
		int flags=1;
		if(setsockopt(lsocket,SOL_SOCKET,SO_REUSEADDR,&flags,sizeof(flags))!=0){
			perror("[-] setsockopt");
			return 1;
		}else{
			puts("[+] setsockopt");
		}

		//bind to local address
		if(bind(lsocket,p->ai_addr,p->ai_addrlen)<0){
			perror("[-] bind");
			continue;
		}else{
			puts("[+] bind");
			bind_success=1;
			break;
		}

	}


	if(bind_success==0){
		fprintf(stderr, "selectserver: failed to bind\n");
		return 1;
	}

	//drop privs
	if(setgid(gid)!=0){
		perror("[-] setgid");
	}
	puts("[+] setgid");

	//drop privs
	if(setuid(uid)!=0){
		perror("[-] setuid");
	}
	puts("[+] setuid");



	//listen on port
	if(listen(lsocket,3)!=0){
		perror("[-] listen");
		return 1;
	}else{
		puts("[+] listen");
	}




	socklen_t addrlen;
	addrlen = sizeof(struct sockaddr_in);
	//accept client connections
	while((fdlsocket=accept(lsocket, (struct sockaddr *)sa,&addrlen))>=0){
		puts("[+] accept");
		pid_t child=0;
		if((child=fork())==0){
			puts("[+] child fork");
			break;
		}else{
			printf("[+] parent fork (%d)\n",child);
		}
	}
	//puts("[-] accept");
	//return 1;

	char *buffer=NULL;
	int lbuffer=0;
	int sbuffer=1024;
	buffer=malloc(sizeof(*buffer)*sbuffer);
	if((lbuffer=recv(fdlsocket,buffer,1024,0))>0){
		printf("[+] recv (%d bytes)\n",lbuffer);
	}else{
		perror("[-] recv");
		return 1;
	}


	

	//remote host lookup
	if((getaddrinfo(raddr,rport,&hints,&servinfo))!=0){
		puts("[-] getaddrinfo");
		return 1;
	}
	puts("[+] getaddrinfo");

	for(struct addrinfo *p=servinfo;p!=NULL;p=p->ai_next){

		//create remote socket
		if((fdrsocket=socket(p->ai_family,p->ai_socktype,p->ai_protocol))<0){
			perror("[-] socket");
			continue;
		}else{
			puts("[+] socket");
		}

		//connect to remore host
		if(connect(fdrsocket,p->ai_addr,p->ai_addrlen)==-1) {
			//close(sockfd);
			puts("[-] connect");
			continue;
		}else{
			puts("[+] connect");
		}
		break;

	}


	
	int bs=-1;
	if((bs=send(fdrsocket,buffer,lbuffer,0))>0){
		printf("[+] send (%d bytes)\n",bs);
	}else{
		perror("[-] send");
		return 1;
	}



	if(fork()==0){
		puts("[+] fork child");
		puts("[!] server start");
		int sbuffer=16;
		char *buffer=calloc(sbuffer,1);
		int count=0;
		while((count=recv(fdlsocket,buffer,sbuffer,0))>0){
			if(send(fdrsocket,buffer,count,0)!=-1){
			}
			//memset(buffer,'\0',sbuffer);
		}

		free(buffer);
		buffer=NULL;
		puts("[!] server done");
		puts("[!] remote shutdown");
		if(shutdown(fdrsocket,2)!=0){
			perror("[-] shutdown");
		}else{
			perror("[+] shutdown");
		}
	}else{
		puts("[+] fork parent");
		puts("[!] client start");
		int sbuffer=16;
		char *buffer=calloc(sbuffer,1);
		int count=0;
		while((count=recv(fdrsocket,buffer,sbuffer,0))>0){
			if(send(fdlsocket,buffer,count,0)!=-1){
			}
			//memset(buffer,'\0',sbuffer);
		}

		free(buffer);
		buffer=NULL;
		puts("[!] client done");
		puts("[!] local shutdown");
		if(shutdown(fdlsocket,2)!=0){
			perror("[-] shutdown");
		}else{
			perror("[+] shutdown");
		}
	}





	return 0;
}
