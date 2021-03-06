#ifndef _H_EXMDB_PARSER_
#define _H_EXMDB_PARSER_
#include "common_types.h"
#include "double_list.h"
#include <pthread.h>


enum {
	ALIVE_ROUTER_CONNECTIONS
};

typedef struct _EXMDB_CONNECTION {
	DOUBLE_LIST_NODE node;
	BOOL b_stop;
	pthread_t thr_id;
	char remote_id[128];
	int sockd;
} EXMDB_CONNECTION;

typedef struct _ROUTER_CONNECTION {
	DOUBLE_LIST_NODE node;
	BOOL b_stop;
	pthread_t thr_id;
	char remote_id[128];
	int sockd;
	time_t last_time;
	pthread_mutex_t lock;
	pthread_mutex_t cond_mutex;
	pthread_cond_t waken_cond;
	DOUBLE_LIST datagram_list;
} ROUTER_CONNECTION;


int exmdb_parser_get_param(int param);

void exmdb_parser_init(int max_threads,
	int max_routers, const char *list_path);

int exmdb_parser_run();

int exmdb_parser_stop();

void exmdb_parser_free();

EXMDB_CONNECTION* exmdb_parser_get_connection();

void exmdb_parser_put_connection(EXMDB_CONNECTION *pconnection);

ROUTER_CONNECTION* exmdb_parser_get_router(const char *remote_id);

void exmdb_parser_put_router(ROUTER_CONNECTION *pconnection);

BOOL exmdb_parser_remove_router(ROUTER_CONNECTION *pconnection);

#endif	/* _H_EXMDB_PARSER_ */
