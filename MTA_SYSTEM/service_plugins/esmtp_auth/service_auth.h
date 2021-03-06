#ifndef _H_SERVICE_AUTH_
#define _H_SERVICE_AUTH_

#include "common_types.h"

typedef BOOL (*VERIFY_USER_PASS)(const char*, const char*, char*, int);

void service_auth_init(int context_num, VERIFY_USER_PASS verify_user_pass);

int service_auth_run();

int service_auth_stop();

void service_auth_free();

const char* service_auth_ehlo();

int service_auth_process(int context_ID, const char *cmd_line, int line_len,
	char *reply_string, int reply_len);

BOOL service_auth_retrieve(int context_ID, char *usename, int length);

void service_auth_clear(int context_ID);



#endif /* _H_SERVICE_AUTH_ */

