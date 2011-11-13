#ifndef _MAGIC_H_
#define _MAGIC_H_
#include <magic.h>
#include "config.h"

magic_t magic_open(int);
service *magic_get_service(magic_t,service **,char *,size_t);


#endif
