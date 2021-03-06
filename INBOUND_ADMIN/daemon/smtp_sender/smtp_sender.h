#ifndef _H_SMTP_SENDER_
#define _H_SMTP_SENDER_
#include "common_types.h"

void smtp_sender_init(const char *backend_path);

int smtp_sender_run();

void smtp_sender_send(BOOL b_local, const char *sender, const char *address,
	const char *pbuff, int size);

int smtp_sender_stop();

void smtp_sender_free();

#endif
