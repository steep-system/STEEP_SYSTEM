#ifndef _H_TEMP_LIST_
#define _H_TEMP_LIST_
#include "common_types.h"
#include <time.h>


void temp_list_init(BOOL case_sensive, int size);

int temp_list_run();

int temp_list_stop();

void temp_list_free();

BOOL temp_list_add_string(const char *str, int interval);

BOOL temp_list_query(const char *str);

BOOL temp_list_remove_string(const char *str);

BOOL temp_list_dump(const char *path);

BOOL temp_list_echo(const char *str, time_t *puntil);


#endif /* _H_TEMP_LIST_ */
