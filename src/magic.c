#ifndef _MAGIC_C_
#define _MAGIC_C_
#include "magic.h"


service *magic_get_service(magic_t cookie,service **services,char *buffer,size_t length){

	const char *description=NULL;
	if((description=magic_buffer(cookie,buffer,length))==NULL){
		return NULL;
	}
	printf("magic name: [%s]\n",description);

	for(int si=0;services[si]!=NULL;++si){
		for(int ni=0;services[si]->names[ni]!=NULL;++ni){
			if(strcmp(description,services[si]->names[ni])==0){
				return services[si];
			}
		}
	}
	return NULL;

}


#endif
