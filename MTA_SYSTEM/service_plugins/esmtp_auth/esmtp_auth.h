#ifndef _H_ESMTP_AUTH_
#define _H_ESMTP_AUTH_
#include "common_types.h"

enum {
	ESMTP_AUTH_RETRYING_TIMES

};

void esmtp_auth_init(int retrying_times);

int esmtp_auth_run();

BOOL esmtp_auth_login(const char *username, const char *password, char *reason,
	int reason_len);

int esmtp_auth_stop();

void esmtp_auth_free();

int esmtp_auth_get_param(int param);

void esmtp_auth_set_param(int param, int value);

#endif
