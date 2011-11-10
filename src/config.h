#ifndef _CONFIG_H_
#define _CONFIG_H_
#include <stdlib.h>

struct service{
	char **names;
	char *host;
	char *port;
};



struct services{
	struct service s[1];
};



//char local_port[]="1234";
//char local_host[]="localhost";

struct services s[]={
	{["abc"],"bash.org","80"},
};


#endif
