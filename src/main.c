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
#include <syslog.h>
#include "config.h"
#include "magic.h"

gid_t gid=500;
uid_t uid=500;


char *strdup(const char *s);


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


in_port_t get_in_port(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return (((struct sockaddr_in*)sa)->sin_port);
    }

    return (((struct sockaddr_in6*)sa)->sin6_port);
}




int main(int argc,char **argv){

	openlog("muxd",LOG_NDELAY|LOG_PID,LOG_DAEMON);
	syslog(LOG_NOTICE,"starting muxd");


	if(argc<3){
		syslog(LOG_ERR,"insufficient parameters");
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}


	syslog(LOG_INFO,"opening config file %s",argv[1]);

	config_t conf;
	if(config_open(argv[1],&conf)){
		syslog(LOG_ERR,"failed to open config file %s",argv[1]);
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}

	char *laddr=NULL;
	if(config_get_host(&conf,&laddr)){
		syslog(LOG_ERR,"failed to read host from %s",argv[1]);
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}
	
	char *lport=NULL;
	if(config_get_port(&conf,&lport)){
		syslog(LOG_ERR,"failed to read port from %s",argv[1]);
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}


	printf("local:  [%s:%s]\n",laddr,lport);


	service **services=NULL;
	if(config_get_services(&conf,&services)){
		syslog(LOG_ERR,"failed to read services from %s",argv[1]);
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}

	config_close(&conf);


	syslog(LOG_INFO,"opening magic file %s",argv[2]);

	magic_t cookie=NULL;
	if((cookie=magic_open(MAGIC_NONE))==NULL){
		syslog(LOG_ERR,"failed to perform magic");
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}


	if(magic_load(cookie,argv[2])==-1){
		syslog(LOG_ERR,"failed to open magic file %s",argv[2]);
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}



	int fdlsocket=-1;
	int fdrsocket=-1;




	struct addrinfo hints, *servinfo;


	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags=2;//AI_CANONNAME;

	//local host lookup
	
	syslog(LOG_INFO,"attempting to lookup local address [%s]:%s",laddr,lport);

	if((getaddrinfo(laddr,lport,&hints,&servinfo))!=0){
		syslog(LOG_ERR,"failed to lookup local address [%s]:%s",laddr,lport);
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}

	int lsocket=-1;
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
			//perror("[-] bind");
			continue;
		}else{
			syslog(LOG_INFO,"successfully bound to local address [%s]:%s",laddr,lport);
			bind_success=1;
			break;
		}

	}

	freeaddrinfo(servinfo);

	if(bind_success==0){
		syslog(LOG_ERR,"failed to bind to local address [%s]:%s",laddr,lport);
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}

	free(laddr);
	free(lport);

	//drop privs
	syslog(LOG_INFO,"attempting to drop privliages to group:%d",gid);
	if(setgid(gid)!=0){
		syslog(LOG_ERR,"failed to drop privliages to group:%d",gid);
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}
	//puts("[+] setgid");

	//drop privs
	syslog(LOG_INFO,"attempting to drop privliages to user:%d",uid);
	if(setuid(uid)!=0){
		syslog(LOG_ERR,"failed to drop privliages to user:%d",uid);
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}
	//puts("[+] setuid");



	//listen on port
	syslog(LOG_INFO,"attempting to listen on socket");
	if(listen(lsocket,3)!=0){
		syslog(LOG_ERR,"failed to listen on socket");
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}




	struct sockaddr_storage sas;
	socklen_t addrlen=sizeof(sas);
	char *bhost=calloc(INET6_ADDRSTRLEN+1,sizeof(char));
	char *bport=calloc(16+1,sizeof(char));

	//accept client connections
	while((fdlsocket=accept(lsocket,(struct sockaddr *)&sas,&addrlen))>=0){
		puts("[+] accept");

		const struct sockaddr *client=(struct sockaddr *)&sas;
		if(getnameinfo(client,addrlen,bhost,INET6_ADDRSTRLEN+1,bport,16+1,NI_NUMERICHOST|NI_NUMERICSERV)==0){
			printf("name success\n");
			printf("host: [%s]\n",bhost);
			printf("port: [%s]\n",bport);
			syslog(LOG_INFO,"accepted client [%s]:%s",bhost,bport);
		}else{
			printf("name failure\n");
			syslog(LOG_INFO,"accepted client");
		}

		//break;
		pid_t child=0;
		if((child=fork())==0){
			puts("[+] child fork");
			break;
		}else{
			printf("[+] parent fork (%d)\n",child);
			syslog(LOG_INFO,"forked worker process %d",child);
		}
	}

	free(bhost);
	free(bport);



	//puts("[-] accept");
	//return 1;

	int lbuffer=0;
	int sbuffer=1024;
	char *buffer=NULL;
	buffer=malloc(sizeof(*buffer)*sbuffer);
	if((lbuffer=recv(fdlsocket,buffer,sbuffer,0))<=0){
		syslog(LOG_ERR,"failed to recv");
		syslog(LOG_CRIT,"stopping muxd worker");
		shutdown(fdlsocket,SHUT_RDWR);
		magic_close(cookie);
		free_services(services);
		free(buffer);
		return 1;
	}
	//printf("buffer: [%s]\n",buffer);


	service *service=NULL;
	if((service=magic_get_service(cookie,services,buffer,lbuffer))==NULL){
		syslog(LOG_ERR,"failed to determine protocol");
		syslog(LOG_CRIT,"stopping muxd worker");
		shutdown(fdlsocket,SHUT_RDWR);
		free_services(services);
		magic_close(cookie);
		free(buffer);
		return 1;
	}
	//printf("[+] magic_get_service\n");
	magic_close(cookie);


	char *raddr=NULL;
	char *rport=NULL;
	raddr=strdup(service->host);
	rport=strdup(service->port);
	free_services(services);
	printf("remote: [%s:%s]\n",raddr,rport);

	//free(buffer);
	//shutdown(fdlsocket,SHUT_RDWR);
	//shutdown(fdrsocket,SHUT_RDWR);
	//return 0;


	syslog(LOG_INFO,"attempting to lookup remote address [%s]:%s",raddr,rport);

	//remote host lookup
	if((getaddrinfo(raddr,rport,&hints,&servinfo))!=0){
		syslog(LOG_ERR,"failed to lookup remote address [%s]:%s",raddr,rport);
		syslog(LOG_CRIT,"stopping muxd worker");
		shutdown(fdlsocket,SHUT_RDWR);
		free(raddr);
		free(rport);
		return 1;
	}
	//puts("[+] getaddrinfo");

	int connect_success=0;
	for(struct addrinfo *p=servinfo;p!=NULL;p=p->ai_next){

		//create remote socket
		if((fdrsocket=socket(p->ai_family,p->ai_socktype,p->ai_protocol))<0){
			//perror("[-] socket");
			continue;
		}else{
			//puts("[+] socket");
		}

		//connect to remore host
		if(connect(fdrsocket,p->ai_addr,p->ai_addrlen)==-1) {
			//close(sockfd);
			//puts("[-] connect");
			continue;
		}else{
			//puts("[+] connect");
			syslog(LOG_INFO,"successfully connected to remote address [%s]:%s",raddr,rport);
			connect_success=1;
			break;
		}

	}

	freeaddrinfo(servinfo);


	if(connect_success==0){
		syslog(LOG_ERR,"failed to connect to remote address [%s]:%s",raddr,rport);
		syslog(LOG_CRIT,"stopping muxd worker");
		shutdown(fdlsocket,SHUT_RDWR);
		free(raddr);
		free(rport);
		return 1;
	}

	free(raddr);
	free(rport);
	
	int bs=-1;
	if((bs=send(fdrsocket,buffer,lbuffer,0))>0){
		printf("[+] send (%d bytes)\n",bs);
	}else{
		perror("[-] send");
		return 1;
	}
	free(buffer);



	//shutdown(fdlsocket,SHUT_RDWR);
	//shutdown(fdrsocket,SHUT_RDWR);
	//free_services(services);
	//return 0;



	pid_t worker=fork();
	if(worker>0){
		syslog(LOG_DEBUG,"(parent) local shuttle started");
		//shutdown(fdlsocket,SHUT_WR);//we won't write to local
		//shutdown(fdrsocket,SHUT_RD);//we won't read from remote
		//puts("[+] fork child");
		//puts("[!] server start");
		int size=16;
		ssize_t count=0;
		char *shuttle=malloc(size*sizeof(char));
		while(1){
			count=recv(fdlsocket,shuttle,size,0);
			printf("local: recv %d bytes\n",count);
			if(count<1){
				syslog(LOG_ERR,"local: failed to recv");
				printf("local: failed to recv\n");
				break;
			}
			if(send(fdrsocket,shuttle,count,0)==-1){
				syslog(LOG_ERR,"local: failed to send");
				break;
			}
		}

		free(shuttle);
		//shuttle=NULL;
		//puts("[!] server done");
		//puts("[!] remote shutdown");
		syslog(LOG_DEBUG,"local shuttle finished");
		shutdown(fdlsocket,SHUT_RDWR);
		shutdown(fdrsocket,SHUT_RDWR);
		//if(shutdown(fdrsocket,2)!=0){
			//perror("[-] shutdown");
		//}else{
			//perror("[+] shutdown");
		//}
		printf("waiting on child: %d\n",worker);
		waitpid(worker,NULL,0);
		printf("done waiting on child: %d\n",worker);

	}else{
		syslog(LOG_DEBUG,"(child) remote shuttle started");
		//shutdown(fdlsocket,SHUT_RD);//we won't read from local
		//shutdown(fdrsocket,SHUT_WR);//we won't write to remote
		//puts("[+] fork parent");
		//puts("[!] client start");
		int size=16;
		ssize_t count=0;
		char *shuttle=malloc(size*sizeof(char));
		while(1){
			count=recv(fdrsocket,shuttle,size,0);
			printf("remote: recv %d bytes\n",count);
			if(count<1){
				syslog(LOG_ERR,"remote: failed to recv");
				printf("remote: failed to recv\n");
				break;
			}
			if(send(fdlsocket,shuttle,count,0)==-1){
				syslog(LOG_ERR,"remote: failed to send");
				break;
			}
		}

		free(shuttle);
		//shuttle=NULL;
		//puts("[!] client done");
		//puts("[!] local shutdown");
		syslog(LOG_DEBUG,"remote shuttle finished");
		shutdown(fdlsocket,SHUT_RDWR);
		shutdown(fdrsocket,SHUT_RDWR);
		//if(shutdown(fdrsocket,2)!=0){
			//perror("[-] shutdown");
		//}else{
			//perror("[+] shutdown");
		//}
		printf("child exiting\n");
		exit(0);
	}

	syslog(LOG_INFO,"worker finished normally");
	syslog(LOG_INFO,"stopping muxd worker");
	exit(0);
	return 0;
}
