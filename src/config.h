#ifndef _CONFIG_H_
#define _CONFIG_H_
#include <libconfig.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


char *strdup(const char *s);


typedef struct{
	char **names;
	char *host;
	char *port;
}service;


int config_open(const char *,config_t *);
int config_close(config_t *);
int config_get_host(config_t *,char **);
int config_get_port(config_t *,char **);
int config_get_services(config_t *,service ***);
int free_services(service **);


#endif
