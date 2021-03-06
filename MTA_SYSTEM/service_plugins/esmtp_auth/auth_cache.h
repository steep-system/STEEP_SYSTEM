#ifndef _H_AUTH_CACHE_
#define _H_AUTH_CACHE_
#include "common_types.h"

enum {
	AUTH_CACHE_TOTAL_SIZE,
	AUTH_CACHE_CURRENT_SIZE,
};

void auth_cache_init(int size);

int auth_cache_run();

int auth_cache_stop();

void auth_cache_free();

BOOL auth_cache_login(const char *username, const char *password);

void auth_cache_add(const char *username, const char *password);

int auth_cache_get_param(int param);

#endif /* _H_AUTH_CACHE_ */
