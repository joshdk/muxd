#ifndef _CONFIG_C_
#define _CONFIG_C_
#include "config.h"


int config_open(const char *path,config_t *config){
	config_init(config);

	if(config_read_file(config,path)==CONFIG_FALSE){
		fprintf(stderr,"[-] config_read_file (%s) on line (%d)\n",config_error_text(config),config_error_line(config));
		config_destroy(config);
		config=NULL;
		return 1;
	}
	return 0;
}




int config_close(config_t *config){
	config_destroy(config);
	return 0;
}




int config_get_port(config_t *config,char **port){
	config_setting_t *cs_port=NULL;
	if((cs_port=config_lookup(config,"port"))==NULL){
		return 1;
	}
	switch(config_setting_type(cs_port)){
		case CONFIG_TYPE_INT:
			*port=calloc(16,sizeof(*port));
			sprintf(*port,"%lu",config_setting_get_int(cs_port));
			return 0;
		case CONFIG_TYPE_STRING:
			*port=strdup(config_setting_get_string(cs_port));
			return 0;
	}
	return 1;
}




int config_get_host(config_t *config,char **host){
	config_setting_t *cs_host=NULL;
	if((cs_host=config_lookup(config,"host"))==NULL){
		return 1;
	}
	if((*host=strdup(config_setting_get_string(cs_host)))==NULL){
		free(*host);
		*host=NULL;
		return 1;
	}
	return 0;
}




int config_get_services(config_t *config,service ***final_services){

	config_setting_t *cs_services=NULL;
	if((cs_services=config_lookup(config,"services"))==NULL){
		return 1;
	}

	int service_count=config_setting_length(cs_services);
	service **services=NULL;
	services=malloc((service_count+1)*sizeof(service *));
	int services_len=0;

	for(int n=0;n<service_count;n++){
		//printf("service: %d\n",n);
		
		config_setting_t *cs_service=NULL;
		if((cs_service=config_setting_get_elem(cs_services,n))==NULL){
			//printf("failed to retrieve service\n");
			continue;
		}

		int enabled=0;
		if(config_setting_lookup_bool(cs_service,"enabled",&enabled)){
			//printf("  enabled: %s\n",enabled?"true":"false");
			if(enabled==0){
				continue;
			}
		}

		config_setting_t *cs_host=NULL;
		if((cs_host=config_setting_get_member(cs_service,"host"))==NULL){
			continue;
		}

		char *host=NULL;
		host=strdup(config_setting_get_string(cs_host));

		if(host==NULL){
			printf("failed to retrieve host\n");
			continue;
		}

		config_setting_t *cs_port=NULL;
		if((cs_port=config_setting_get_member(cs_service,"port"))==NULL){
			free(host);
			continue;
		}

		char *port=NULL;
		switch(config_setting_type(cs_port)){
			case CONFIG_TYPE_INT:
				port=calloc(16,sizeof(*port));
				sprintf(port,"%lu",config_setting_get_int(cs_port));
				break;
			case CONFIG_TYPE_STRING:
				port=strdup(config_setting_get_string(cs_port));
				break;
		}

		if(port==NULL){
			printf("failed to retrieve port\n");
			free(host);
			continue;
		}

		config_setting_t *cs_names=NULL;
		if((cs_names=config_setting_get_member(cs_service,"names"))==NULL){
			free(host);
			free(port);
			continue;
		}

		int name_count=config_setting_length(cs_names);
		char **names=malloc((name_count+1)*sizeof(*names));
		int names_len=0;

		for(int ni=0;ni<name_count;++ni){
			const char *name=config_setting_get_string_elem(cs_names,ni);
			if(name!=NULL && strlen(name)>0){
				names[names_len]=strdup(name);
				//printf("    \"%s\"\n",names[names_len]);
				names_len++;
			}
		}
		names[names_len]=NULL;

		services[services_len]=malloc(sizeof(service));
		services[services_len]->host=host;
		services[services_len]->port=port;
		services[services_len]->names=names;
		services_len++;
	}

	services[services_len]=NULL;
	*final_services=services;
	return 0;
}




int free_services(service **services){

	for(service **s=services;*s!=NULL;++s){
		free((*s)->host);
		(*s)->host=NULL;
		free((*s)->port);
		(*s)->port=NULL;
		for(char **name=(*s)->names;*name!=NULL;++name){
			free(*name);
			*name=NULL;
		}
		free((*s)->names);
		(*s)->names=NULL;
		free(*s);
		*s=NULL;
	}
	free(services);
	services=NULL;

	return 0;
}


#endif
