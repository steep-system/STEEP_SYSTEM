#ifndef _H_EXMDB_LOCAL_
#define _H_EXMDB_LOCAL_
#include <stdarg.h>
#include "hook_common.h"

enum {
	DELIVERY_NO_USER,
	DELIVERY_MAILBOX_FULL,
	DELIVERY_OPERATION_ERROR,
	DELIVERY_OPERATION_FAILURE,
	DELIVERY_OPERATION_OK,
	DELIVERY_OPERATION_DELIVERED
};

#define BOUND_NOTLOCAL					7

#define SPAM_STATISTIC_OK               0
#define SPAM_STATISTIC_NOUSER           2

typedef void (*SPAM_STATISTIC)(int);

extern BOOL (*exmdb_local_check_domain)(const char *domainname);

extern BOOL (*exmdb_local_get_lang)(const char *username, char *lang);

extern BOOL (*exmdb_local_get_timezone)(
	const char *username, char *timezone);

extern BOOL (*exmdb_local_check_same_org2)(
	const char *domainname1, const char *domainname2);

extern BOOL (*exmdb_local_lang_to_charset)(
	const char *lang, char *charset);

extern SPAM_STATISTIC exmdb_local_spam_statistic;

void exmdb_local_init(const char *config_path,
	const char *org_name, const char *default_charset,
	const char *default_timezone, const char *propname_path);

int exmdb_local_run();

int exmdb_local_stop();

void exmdb_local_free();

BOOL exmdb_local_hook(MESSAGE_CONTEXT *pcontext);

int exmdb_local_deliverquota(MESSAGE_CONTEXT *pcontext, const char *address);

void exmdb_local_log_info(MESSAGE_CONTEXT *pcontext, const char *rcpt_to,
	int level, char *format, ...);

void exmdb_local_console_talk(int argc, char **argv, char *result, int length);

#endif

