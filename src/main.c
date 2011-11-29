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


char *strdup(const char *s);
int daemon(int nochdir, int noclose);


int main(int argc,char **argv){

	openlog("muxd",LOG_NDELAY|LOG_PID,LOG_DAEMON);
	syslog(LOG_NOTICE,"#################");
	syslog(LOG_NOTICE,"# starting muxd #");
	syslog(LOG_NOTICE,"#################");

	syslog(LOG_INFO,"daemonizing");

	if(daemon(0,0)!=0){
		syslog(LOG_ERR,"failed to daemonize");
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}

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

	syslog(LOG_INFO,"reading config file");

	char *laddr=NULL;
	if(config_get_host(&conf,&laddr)){
		syslog(LOG_ERR,"failed to read host");
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}
	
	char *lport=NULL;
	if(config_get_port(&conf,&lport)){
		syslog(LOG_ERR,"failed to read port");
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}

	service **services=NULL;
	if(config_get_services(&conf,&services)){
		syslog(LOG_ERR,"failed to read services");
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

	syslog(LOG_INFO,"reading magic file");

	if(magic_load(cookie,argv[2])==-1){
		syslog(LOG_ERR,"failed to read magic");
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}




	struct addrinfo hints, *servinfo;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags=AI_PASSIVE;
	
	syslog(LOG_INFO,"looking up local address [%s]:%s",laddr,lport);

	//local host lookup
	if((getaddrinfo(laddr,lport,&hints,&servinfo))!=0){
		syslog(LOG_ERR,"failed to lookup local address [%s]:%s",laddr,lport);
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}

	int lsocket=-1;
	int fdlsocket=-1;
	int bind_success=0;
	for(struct addrinfo *p=servinfo;p!=NULL;p=p->ai_next){
		//create local socket
		if((lsocket=socket(p->ai_family,p->ai_socktype,p->ai_protocol))<0){
			continue;
		}

		//set socket option
		int flags=1;
		if(setsockopt(lsocket,SOL_SOCKET,SO_REUSEADDR,&flags,sizeof(flags))!=0){
			return 1;
		}

		//bind to local address
		if(bind(lsocket,p->ai_addr,p->ai_addrlen)<0){
			continue;
		}else{
			syslog(LOG_INFO,"bound to local address [%s]:%s",laddr,lport);
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




	gid_t gid=500;
	uid_t uid=500;
	syslog(LOG_INFO,"droping privileges to %d:%d",uid,gid);

	//drop user privileges
	if(setgid(gid)!=0){
		syslog(LOG_ERR,"failed to drop privileges to group:%d",gid);
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}

	//drop group privileges
	if(setuid(uid)!=0){
		syslog(LOG_ERR,"failed to drop privileges to user:%d",uid);
		syslog(LOG_CRIT,"stopping muxd");
		return 1;
	}




	syslog(LOG_INFO,"listening on socket");

	//listen on port
	int backlog=16;
	if(listen(lsocket,backlog)!=0){
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

		const struct sockaddr *client=(struct sockaddr *)&sas;
		if(getnameinfo(client,addrlen,bhost,INET6_ADDRSTRLEN+1,bport,16+1,NI_NUMERICHOST|NI_NUMERICSERV)==0){
			syslog(LOG_INFO,"accepted client [%s]:%s",bhost,bport);
		}else{
			syslog(LOG_INFO,"accepted client [???]:???");
		}

		pid_t child=0;
		if((child=fork())==0){
			break;
		}else{
			syslog(LOG_INFO,"created worker %d",child);
		}

	}

	free(bhost);
	free(bport);



	//read handshake from client
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

	magic_close(cookie);




	char *raddr=NULL;
	char *rport=NULL;
	raddr=strdup(service->host);
	rport=strdup(service->port);
	free_services(services);

	syslog(LOG_INFO,"looking up remote address [%s]:%s",raddr,rport);

	//remote host lookup
	if((getaddrinfo(raddr,rport,&hints,&servinfo))!=0){
		syslog(LOG_ERR,"failed to lookup remote address [%s]:%s",raddr,rport);
		syslog(LOG_CRIT,"stopping muxd worker");
		shutdown(fdlsocket,SHUT_RDWR);
		free(raddr);
		free(rport);
		return 1;
	}

	int fdrsocket=-1;
	int connect_success=0;
	for(struct addrinfo *p=servinfo;p!=NULL;p=p->ai_next){
		//create remote socket
		if((fdrsocket=socket(p->ai_family,p->ai_socktype,p->ai_protocol))<0){
			continue;
		}

		//connect to remore host
		if(connect(fdrsocket,p->ai_addr,p->ai_addrlen)==-1) {
			continue;
		}else{
			syslog(LOG_INFO,"connected to remote address [%s]:%s",raddr,rport);
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




	//send handshake to remote host
	int bs=-1;
	if((bs=send(fdrsocket,buffer,lbuffer,0))<=0){
		syslog(LOG_ERR,"failed to send");
		syslog(LOG_CRIT,"stopping muxd worker");
		shutdown(fdlsocket,SHUT_RDWR);
		shutdown(fdrsocket,SHUT_RDWR);
		free(buffer);
		return 1;
	}
	free(buffer);




	pid_t worker=fork();
	if(worker>0){
		syslog(LOG_DEBUG,"local shuttle started");
		int size=4096;
		char *shuttle=malloc(size*sizeof(char));
		while(1){
			ssize_t brecv=recv(fdlsocket,shuttle,size,0);
			if(brecv<0){
				//error
				syslog(LOG_ERR,"local failed to recv");
				break;
			}else if(brecv==0){
				//ordered shutdown
				syslog(LOG_INFO,"local orderly shutdown");
				break;
			}//success

			ssize_t bsent=send(fdrsocket,shuttle,brecv,0);
			if(bsent<0){
				syslog(LOG_ERR,"local failed to send");
				break;
			}
		}

		free(shuttle);
		syslog(LOG_DEBUG,"local shuttle finished");
		shutdown(fdlsocket,SHUT_RDWR);
		shutdown(fdrsocket,SHUT_RDWR);
		waitpid(worker,NULL,0);

	}else{
		syslog(LOG_DEBUG,"remote shuttle started");
		int size=4096;
		char *shuttle=malloc(size*sizeof(char));
		while(1){
			ssize_t brecv=recv(fdrsocket,shuttle,size,0);
			if(brecv<0){
				//error
				syslog(LOG_ERR,"remote failed to recv");
				break;
			}else if(brecv==0){
				//ordered shutdown
				syslog(LOG_INFO,"remote orderly shutdown");
				break;
			}//success

			ssize_t bsent=send(fdlsocket,shuttle,brecv,0);
			if(bsent<0){
				syslog(LOG_ERR,"remote failed to send");
				break;
			}
		}

		free(shuttle);
		syslog(LOG_DEBUG,"remote shuttle finished");
		shutdown(fdlsocket,SHUT_RDWR);
		shutdown(fdrsocket,SHUT_RDWR);
		return 0;
	}




	//all done with this worker
	syslog(LOG_INFO,"worker finished normally");
	syslog(LOG_INFO,"stopping muxd worker");
	return 0;
}
