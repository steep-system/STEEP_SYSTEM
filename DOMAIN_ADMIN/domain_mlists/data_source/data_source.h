#ifndef _H_DATA_SOURCE_
#define _H_DATA_SOURCE_
#include "common_types.h"
#include "double_list.h"
#include <time.h>

enum {
	ADD_RESULT_OK = 0,
	ADD_RESULT_NODOMAIN,
	ADD_RESULT_DOMAINNOTMAIN,
	ADD_RESULT_NOGROUP,
	ADD_RESULT_USERNAME,
	ADD_RESULT_FULL,
	ADD_RESULT_EXIST,
	ADD_RESULT_NOCLASS,
	ADD_RESULT_CLASSERR
};


#define DOMAIN_TYPE_NORMAL					0

#define DOMAIN_TYPE_ALIAS					1

#define ADDRESS_TYPE_MLIST					2

#define MLIST_TYPE_NORMAL					0

#define MLIST_TYPE_GROUP					1

#define MLIST_TYPE_DOMAIN					2

#define MLIST_TYPE_CLASS					3

#define MLIST_PRIVILEGE_ALL					0

#define MLIST_PRIVILEGE_INTERNAL			1

#define MLIST_PRIVILEGE_DOMAIN				2

#define MLIST_PRIVILEGE_SPECIFIED			3

#define MLIST_PRIVILEGE_OUTGOING			4

#define MAXIMUM_MLIST_NUM					1000

#define MAXIMUM_MLIST_ITEMS					1000

typedef struct _MLIST_ITEM {
	char listname[128];
	int list_type;
	int list_privilege;
} MLIST_ITEM;

typedef struct _GROUP_ITEM {
	char groupname[128];
	char title[128];
} GROUP_ITEM;

typedef struct _CLASS_ITEM {
	int class_id;
	char classname[32];
} CLASS_ITEM;

typedef struct _DATA_COLLECT {
	DOUBLE_LIST list;
	DOUBLE_LIST_NODE *pnode;
} DATA_COLLECT;


DATA_COLLECT* data_source_collect_init();

void data_source_collect_free(DATA_COLLECT *pcollect);

int data_source_collect_total(DATA_COLLECT *pcollect);

void data_source_collect_begin(DATA_COLLECT *pcollect);

int data_source_collect_done(DATA_COLLECT *pcollect);

void* data_source_collect_get_value(DATA_COLLECT *pcollect);

int data_source_collect_forward(DATA_COLLECT *pcollect);


void data_source_init(const char *host, int port, const char *user,
	const char *password, const char *db_name);

int data_source_run();

int data_source_stop();

void data_source_free();

BOOL data_source_add_mlist(const char *listname, int list_type,
	int list_privilege, int *presult);

BOOL data_source_add_clist(int class_id, const char *listname,
	int list_privilege, int *presult);

BOOL data_source_edit_mlist(const char *listname, int list_privilege);

BOOL data_source_remove_mlist(const char *listname);

BOOL data_source_get_mlists(const char *domainname, DATA_COLLECT *pcollect);

BOOL data_source_expand_mlist(const char *listname, DATA_COLLECT *pcollect);

BOOL data_source_expand_specified(const char *listname, DATA_COLLECT *pcollect);

BOOL data_source_mlist_del(const char *listname, const char *address);

BOOL data_source_mlist_insert(const char *listname, const char *address);

BOOL data_source_specified_del(const char *listname, const char *address);

BOOL data_source_specified_insert(const char *listname, const char *address);

BOOL data_source_get_groups(const char *domainname, DATA_COLLECT *pcollect);

BOOL data_source_get_classes(const char *domainname, DATA_COLLECT *pcollect);

BOOL data_source_info_mlist(const char *listname, int *plist_privilege);

#endif
