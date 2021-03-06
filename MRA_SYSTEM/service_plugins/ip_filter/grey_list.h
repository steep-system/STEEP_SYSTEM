#ifndef _H_GREY_LIST_
#define _H_GREY_LIST_

#include "common_types.h"

enum{
	GREY_LIST_ALLOW = 0,
	GREY_LIST_DENY,
	GREY_LIST_NOT_FOUND
};

enum{
	GREY_REFRESH_OK = 0,
	GREY_REFRESH_FILE_ERROR,
	GREY_REFRESH_HASH_FAIL
};

void grey_list_init(const char *path, int growing_num);

int grey_list_run();

int grey_list_stop();

void grey_list_free();

int grey_list_refresh();

int grey_list_query(const char *ip, BOOL b_count);

BOOL grey_list_add_ip(const char *ip, int times, int interval);

BOOL grey_list_remove_ip(const char *ip);

BOOL grey_list_dump(const char *path);

BOOL grey_list_echo(const char *ip, int *ptimes, int *pinterval);

#endif /* _H_GREY_LIST_ */
