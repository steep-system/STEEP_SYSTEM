#include "util.h"
#include "str_hash.h"
#include "double_list.h"
#include "list_file.h"
#include "config_file.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#define LOCKER_VERSION		"3.0"

enum {
	TYPE_NONE,
	TYPE_CREAT
};

typedef struct _ACL_ITEM {
	DOUBLE_LIST_NODE node;
	char ip_addr[16];
} ACL_ITEM;

struct _CONNECTION_NODE;

typedef struct _THR_PARAM {
	int type;
	pthread_t thr_id;
	struct _CONNECTION_NODE *pconnection;
} THR_PARAM;

typedef struct _CONNECTION_NODE {
	DOUBLE_LIST_NODE node;
	DOUBLE_LIST_NODE node_hash;
	int sockd;
	THR_PARAM *pthread;
	time_t lock_time;
	char resource[256];
	BOOL is_locking;
	int offset;
	char buffer[1024];
	char line[1024];
} CONNECTION_NODE;


static BOOL g_notify_stop;
static int g_max_interval;
static int g_threads_num;
static char g_list_path[256];
static pthread_mutex_t g_hash_lock;
static pthread_mutex_t g_connection_lock;
static pthread_mutex_t g_cond_mutex;
static pthread_cond_t g_waken_cond;
static DOUBLE_LIST g_acl_list;
static DOUBLE_LIST g_connection_list;
static DOUBLE_LIST g_connection_list1;
static STR_HASH_TABLE *g_hash_table;


static void *accept_work_func(void *param);

static void *thread_work_func(void *param);

static void term_handler(int signo);

static BOOL read_mark(CONNECTION_NODE *pconnection);

static void unlock_resource(char *resource);

int main(int argc, char **argv)
{
	int i, num;
	int optval;
	int listen_port;
	int table_size;
	int sockd, status;
	BOOL b_listen;
	pthread_t thr_id;
	pthread_attr_t thr_attr;
	char temp_buff[32];
	char listen_ip[16];
	char *str_value, *pitem;
	ACL_ITEM *pacl;
	struct in_addr addr;
	struct sockaddr_in my_name;
	LIST_FILE *plist;
	CONFIG_FILE *pconfig;
	THR_PARAM *threads;
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;

	
	if (2 != argc) {
		printf("%s <cfg file>\n", argv[0]);
		return -1;
	}
	if (2 == argc && 0 == strcmp(argv[1], "--help")) {
		printf("%s <cfg file>\n", argv[0]);
		return 0;
	}
	if (2 == argc && 0 == strcmp(argv[1], "--version")) {
		printf("version: %s\n", LOCKER_VERSION);
		return 0;
	}
	signal(SIGPIPE, SIG_IGN);
	
	pconfig = config_file_init(argv[1]);
	if (NULL == pconfig) {
		printf("[system]: fail to open config file %s\n", argv[1]);
		return -2;
	}

	str_value = config_file_get_value(pconfig, "DATA_FILE_PATH");
	if (NULL == str_value) {
		strcpy(g_list_path, "../data/locker_acl.txt");
	} else {
		snprintf(g_list_path, 255, "%s/locker_acl.txt", str_value);
	}
	printf("[system]: acl file path is %s\n", g_list_path);

	str_value = config_file_get_value(pconfig, "LOCKER_LISTEN_ANY");
	if (NULL == str_value) {
		b_listen = FALSE;
	} else {
		if (0 == strcasecmp(str_value, "TRUE")) {
			b_listen = TRUE;
		} else {
			b_listen = FALSE;
		}
	}

	if (FALSE == b_listen) {
		str_value = config_file_get_value(pconfig, "LOCKER_LISTEN_IP");
		if (NULL == str_value) {
			strcpy(listen_ip, "127.0.0.1");
			config_file_set_value(pconfig, "LOCKER_LISTEN_IP", "127.0.0.1");
		} else {
			strncpy(listen_ip, str_value, 16);
		}
		g_list_path[0] = '\0';
		printf("[system]: listen ip is %s\n", listen_ip);
	} else {
		listen_ip[0] = '\0';
		printf("[system]: listen ip is ANY\n");
	}

	str_value = config_file_get_value(pconfig, "LOCKER_LISTEN_PORT");
	if (NULL == str_value) {
		listen_port = 7777;
		config_file_set_value(pconfig, "LOCKER_LISTEN_PORT", "7777");
	} else {
		listen_port = atoi(str_value);
		if (listen_port <= 0) {
			listen_port = 7777;
			config_file_set_value(pconfig, "LOCKER_LISTEN_PORT", "7777");
		}
	}
	printf("[system]: listen port is %d\n", listen_port);

	str_value = config_file_get_value(pconfig, "LOCKER_TABLE_SIZE");
	if (NULL == str_value) {
		table_size = 3000;
		config_file_set_value(pconfig, "LOCKER_TABLE_SIZE", "3000");
	} else {
		table_size = atoi(str_value);
		if (table_size <= 0) {
			table_size = 3000;
			config_file_set_value(pconfig, "LOCKER_TABLE_SIZE", "3000");
		}
	}
	printf("[system]: hash table size is %d\n", table_size);

	str_value = config_file_get_value(pconfig, "LOCKER_MAXIMUM_INTERVAL");
	if (NULL == str_value) {
		g_max_interval = 180;
		config_file_set_value(pconfig, "LOCKER_MAXIMUM_INTERVAL", "3minutes");
	} else {
		g_max_interval = atoitvl(str_value);
		if (g_max_interval <= 0) {
			g_max_interval = 180;
			config_file_set_value(pconfig, "LOCKER_MAXIMUM_INTERVAL",
				"3minutes");
		}
	}
	itvltoa(g_max_interval, temp_buff);
	printf("[system]: maximum interval is %s\n", temp_buff);

	str_value = config_file_get_value(pconfig, "LOCKER_THREADS_NUM");
	if (NULL == str_value) {
		g_threads_num = 200;
		config_file_set_value(pconfig, "LOCKER_THREADS_NUM", "200");
	} else {
		g_threads_num = atoi(str_value);
		if (g_threads_num < 20) {
			g_threads_num = 20;
			config_file_set_value(pconfig, "LOCKER_THREADS_NUM", "20");
		}
		if (g_threads_num > 1000) {
			g_threads_num = 1000;
			config_file_set_value(pconfig, "LOCKER_THREADS_NUM", "1000");
		}
	}

	printf("[system]: threads number is %d\n", g_threads_num);

	g_threads_num ++;

	config_file_save(pconfig);
	config_file_free(pconfig);

	g_hash_table = str_hash_init(table_size, sizeof(DOUBLE_LIST), NULL);
	if (NULL == g_hash_table) {
		printf("[system]: fail to init hash table\n");
		return -3;
	}
	
	/* create a socket */
	sockd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockd == -1) {
        printf("[system]: fail to create socket for listening\n");
		str_hash_free(g_hash_table);
		return -4;
	}
	optval = -1;
	/* eliminates "Address already in use" error from bind */
	setsockopt(sockd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
		sizeof(int));
	
	/* socket binding */
	memset(&my_name, 0, sizeof(my_name));
	my_name.sin_family = AF_INET;
	if ('\0' != listen_ip[0]) {
		my_name.sin_addr.s_addr = inet_addr(listen_ip);
	} else {
		my_name.sin_addr.s_addr = INADDR_ANY;
	}   
	my_name.sin_port = htons(listen_port);
	
	status = bind(sockd, (struct sockaddr*)&my_name, sizeof(my_name));
	if (-1 == status) {
		printf("[system]: fail to bind socket\n");
        close(sockd);
		str_hash_free(g_hash_table);
		return -5;
    }
	
	status = listen(sockd, 5);

	if (-1 == status) {
		printf("[system]: fail to listen socket\n");
		close(sockd);
		str_hash_free(g_hash_table);
		return -6;
	}

	pthread_mutex_init(&g_hash_lock, NULL);
	pthread_mutex_init(&g_connection_lock, NULL);
	pthread_mutex_init(&g_cond_mutex, NULL);
	pthread_cond_init(&g_waken_cond, NULL);
	
	double_list_init(&g_acl_list);
	double_list_init(&g_connection_list);
	double_list_init(&g_connection_list1);

	threads = (THR_PARAM*)malloc(g_threads_num*sizeof(THR_PARAM));

	pthread_attr_init(&thr_attr);

	pthread_attr_setstacksize(&thr_attr, 1024*1024);

	for (i=0; i<g_threads_num; i++) {
		threads[i].type = TYPE_CREAT;
		if (0 != pthread_create(&threads[i].thr_id, &thr_attr,
			thread_work_func, &threads[i])) {
			printf("[system]: fail to create pool thread\n");
			break;
		}
	}

	pthread_attr_destroy(&thr_attr);

	if (i != g_threads_num) {
		for (i-=1; i>=0; i--) {
			pthread_cancel(threads[i].thr_id);
		}

		close(sockd);
		str_hash_free(g_hash_table);

		double_list_free(&g_acl_list);
		double_list_free(&g_connection_list);
		double_list_free(&g_connection_list1);

		pthread_mutex_destroy(&g_connection_lock);
		pthread_mutex_destroy(&g_hash_lock);
		pthread_mutex_destroy(&g_cond_mutex);
		pthread_cond_destroy(&g_waken_cond);
		return -7;

	}
	
	if ('\0' != g_list_path[0]) {
		plist = list_file_init(g_list_path, "%s:16");
		if (NULL == plist) {
			for (i=g_threads_num-1; i>=0; i--) {
				pthread_cancel(threads[i].thr_id);
			}

			close(sockd);
			str_hash_free(g_hash_table);

			double_list_free(&g_acl_list);
			double_list_free(&g_connection_list);
			double_list_free(&g_connection_list1);

			pthread_mutex_destroy(&g_connection_lock);
			pthread_mutex_destroy(&g_hash_lock);
			pthread_mutex_destroy(&g_cond_mutex);
			pthread_cond_destroy(&g_waken_cond);
			printf("[system]: fail to load acl from %s\n", g_list_path);
			return -8;
		}
		num = list_file_get_item_num(plist);
		pitem = list_file_get_list(plist);
		for (i=0; i<num; i++) {
			pacl = (ACL_ITEM*)malloc(sizeof(ACL_ITEM));
			if (NULL == pacl) {
				continue;
			}
			pacl->node.pdata = pacl;
			strcpy(pacl->ip_addr, pitem + 16*i);
			double_list_append_as_tail(&g_acl_list, &pacl->node);
		}
		list_file_free(plist);

	}


	if (0 != pthread_create(&thr_id, NULL, accept_work_func, (void*)(long)sockd)) {
		printf("[system]: fail to create accept thread\n");

		for (i=g_threads_num-1; i>=0; i--) {
			pthread_cancel(threads[i].thr_id);
		}

		close(sockd);
		str_hash_free(g_hash_table);

		while (pnode=double_list_get_from_head(&g_acl_list)) {
			free(pnode->pdata);
		}
		double_list_free(&g_acl_list);

		double_list_free(&g_connection_list);
		double_list_free(&g_connection_list1);

		pthread_mutex_destroy(&g_connection_lock);
		pthread_mutex_destroy(&g_hash_lock);
		pthread_mutex_destroy(&g_cond_mutex);
		pthread_cond_destroy(&g_waken_cond);
		return -9;
	}
	
	g_notify_stop = FALSE;
	signal(SIGTERM, term_handler);
	printf("[system]: LOCKER is now rinning\n");
	while (FALSE == g_notify_stop) {
		sleep(1);
	}

	close(sockd);

	for (i=0; i<g_threads_num; i++) {
		if (TYPE_NONE != threads[i].type) {
			pthread_cancel(threads[i].thr_id);
		}
	}

	free(threads);

	str_hash_free(g_hash_table);

	while (pnode=double_list_get_from_head(&g_acl_list)) {
		free(pnode->pdata);
	}

	double_list_free(&g_acl_list);

	while (pnode=double_list_get_from_head(&g_connection_list)) {
		pconnection = (CONNECTION_NODE*)pnode->pdata;
		close(pconnection->sockd);
		free(pconnection);
	}

	double_list_free(&g_connection_list);

	while (pnode=double_list_get_from_head(&g_connection_list1)) {
		pconnection = (CONNECTION_NODE*)pnode->pdata;
		close(pconnection->sockd);
		free(pconnection);
	}

	double_list_free(&g_connection_list1);

	pthread_mutex_destroy(&g_connection_lock);
	pthread_mutex_destroy(&g_hash_lock);
	pthread_mutex_destroy(&g_cond_mutex);
	pthread_cond_destroy(&g_waken_cond);

	return 0;
}



static void *accept_work_func(void *param)
{
	int sockd, sockd2;
	ACL_ITEM *pacl;
	socklen_t addrlen;
	char client_hostip[16];
	DOUBLE_LIST_NODE *pnode;
	struct sockaddr_in peer_name;
	CONNECTION_NODE *pconnection;	


	sockd = (int)(long)param;
	while (FALSE == g_notify_stop) {
		/* wait for an incoming connection */
        addrlen = sizeof(peer_name);
        sockd2 = accept(sockd, (struct sockaddr*)&peer_name, &addrlen);
		if (-1 == sockd2) {
			continue;
		}
		strcpy(client_hostip, inet_ntoa(peer_name.sin_addr));
		if ('\0' != g_list_path[0]) {
			for (pnode=double_list_get_head(&g_acl_list); NULL!=pnode;
				pnode=double_list_get_after(&g_acl_list, pnode)) {
				pacl = (ACL_ITEM*)pnode->pdata;
				if (0 == strcmp(client_hostip, pacl->ip_addr)) {
					break;
				}
			}
			
			if (NULL == pnode) {
				write(sockd2, "Access Deny\r\n", 13);
				close(sockd2);
				continue;
			}
		}

		pconnection = (CONNECTION_NODE*)malloc(sizeof(CONNECTION_NODE));
		if (NULL == pconnection) {
			write(sockd2, "Internal Error!\r\n", 17);
			close(sockd2);
			continue;
		}
		pthread_mutex_lock(&g_connection_lock);
		if (double_list_get_nodes_num(&g_connection_list) + 1 +
			double_list_get_nodes_num(&g_connection_list1) >= g_threads_num) {
			pthread_mutex_unlock(&g_connection_lock);
			free(pconnection);
			write(sockd2, "Maximum Connection Reached!\r\n", 29);
			close(sockd2);
			continue;

		}
		
		pconnection->node.pdata = pconnection;
		pconnection->node_hash.pdata = pconnection;
		pconnection->sockd = sockd2;
		pconnection->offset = 0;
		pconnection->is_locking = FALSE;
		double_list_append_as_tail(&g_connection_list1, &pconnection->node);
		pthread_mutex_unlock(&g_connection_lock);
		write(sockd2, "OK\r\n", 4);
		pthread_cond_signal(&g_waken_cond);

	}
	
	pthread_exit(0);

}


static void *thread_work_func(void *param)
{
	time_t cur_time;
	time_t last_time;
	DOUBLE_LIST *plist;
	DOUBLE_LIST temp_list;
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;
	THR_PARAM *thr_param;


	thr_param = (THR_PARAM*)param;

NEXT_LOOP:
	if (TYPE_CREAT == thr_param->type) {
		pthread_mutex_lock(&g_cond_mutex);
		pthread_cond_wait(&g_waken_cond, &g_cond_mutex);
		pthread_mutex_unlock(&g_cond_mutex);

		pthread_mutex_lock(&g_connection_lock);
		pnode = double_list_get_from_head(&g_connection_list1);
		if (NULL != pnode) {
			double_list_append_as_tail(&g_connection_list, pnode);
		}
		pthread_mutex_unlock(&g_connection_lock);
		if (NULL == pnode) {
			goto NEXT_LOOP;
		}
		pconnection = (CONNECTION_NODE*)pnode->pdata;
		pconnection->pthread = thr_param;
		
	} else {
		pconnection = thr_param->pconnection;
		thr_param->type = TYPE_CREAT;
	}

	while (TRUE) {
		if (FALSE == read_mark(pconnection)) {
			if (TRUE == pconnection->is_locking) {
				unlock_resource(pconnection->resource);
			}
			close(pconnection->sockd);
			pthread_mutex_lock(&g_connection_lock);
			double_list_remove(&g_connection_list, &pconnection->node);
			pthread_mutex_unlock(&g_connection_lock);
			free(pconnection);
			goto NEXT_LOOP;
		}

		if (TRUE == pconnection->is_locking) {
			if (0 == strcasecmp(pconnection->line, "UNLOCK")) {
				unlock_resource(pconnection->resource);
				write(pconnection->sockd, "TRUE\r\n", 6);
				pconnection->is_locking = FALSE;
			} else if (0 == strcasecmp(pconnection->line, "QUIT")) {
				unlock_resource(pconnection->resource);
				write(pconnection->sockd, "BYE\r\n", 5);
				close(pconnection->sockd);
				pthread_mutex_lock(&g_connection_lock);
				double_list_remove(&g_connection_list, &pconnection->node);
				pthread_mutex_unlock(&g_connection_lock);
				free(pconnection);
				goto NEXT_LOOP;
			} else {
				time(&cur_time);
				if (cur_time - pconnection->lock_time >= g_max_interval) {
					unlock_resource(pconnection->resource);
					pconnection->is_locking = FALSE;
				}
				write(pconnection->sockd, "FALSE\r\n", 7);
			}
		} else {
			if (0 == strncasecmp(pconnection->line, "LOCK ", 5) &&
				strlen(pconnection->line) > 5) {
				strncpy(pconnection->resource, pconnection->line + 5, 256);
				pconnection->resource[255] = '\0';
				pthread_mutex_lock(&g_hash_lock);
				plist = str_hash_query(g_hash_table, pconnection->resource);
				if (NULL == plist) {
					if (1 != str_hash_add(g_hash_table,
						pconnection->resource, &temp_list)) {
						pthread_mutex_unlock(&g_hash_lock);
						time(&last_time);
						while (TRUE) {
							sleep(1);
							pthread_mutex_lock(&g_hash_lock);
							if (1 == str_hash_add(g_hash_table,
								pconnection->resource, &temp_list)) {
								break;
							}
							pthread_mutex_unlock(&g_hash_lock);
							time(&cur_time);
							if (cur_time - last_time >= g_max_interval) {
								close(pconnection->sockd);
								pthread_mutex_lock(&g_connection_lock);
								double_list_remove(&g_connection_list,
									&pconnection->node);
								pthread_mutex_unlock(&g_connection_lock);
								free(pconnection);
								goto NEXT_LOOP;
							}
						}
					}

					plist = str_hash_query(g_hash_table, pconnection->resource);
					double_list_init(plist);
					pthread_mutex_unlock(&g_hash_lock);
					write(pconnection->sockd, "TRUE\r\n", 6);
					time(&pconnection->lock_time);
					pconnection->is_locking = TRUE;
				} else {
					double_list_append_as_tail(plist, &pconnection->node_hash);
					pconnection->pthread->type = TYPE_NONE;
					pthread_mutex_unlock(&g_hash_lock);
					pthread_detach(pthread_self());
					pthread_exit(0);
				}
			} else if (0 == strcasecmp(pconnection->line, "PING")) {
				write(pconnection->sockd, "TRUE\r\n", 6);
			} else if (0 == strcasecmp(pconnection->line, "QUIT")) {
				write(pconnection->sockd, "BYE\r\n", 5);
				close(pconnection->sockd);
				pthread_mutex_lock(&g_connection_lock);
				double_list_remove(&g_connection_list, &pconnection->node);
				pthread_mutex_unlock(&g_connection_lock);
				free(pconnection);
				goto NEXT_LOOP;
			} else {
				write(pconnection->sockd, "FALSE\r\n", 7);
			}
		}

	}

	pthread_exit(0);
}


static void unlock_resource(char *resource)
{
	DOUBLE_LIST *plist;
	DOUBLE_LIST_NODE *pnode;
	CONNECTION_NODE *pconnection;


	pthread_mutex_lock(&g_hash_lock);
	plist = str_hash_query(g_hash_table, resource);
	pnode = double_list_get_from_head(plist);
	if (NULL == pnode) {
		double_list_free(plist);
		str_hash_remove(g_hash_table, resource);
		pthread_mutex_unlock(&g_hash_lock);
	} else {
		pthread_mutex_unlock(&g_hash_lock);
		pconnection = (CONNECTION_NODE*)pnode->pdata;
		write(pconnection->sockd, "TRUE\r\n", 6);
		time(&pconnection->lock_time);
		pconnection->is_locking = TRUE;
		pconnection->pthread->pconnection = pconnection;
		while (0 != pthread_create(&pconnection->pthread->thr_id, NULL,
				thread_work_func, pconnection->pthread)) {
			sleep(1);
		}
	}
}

static BOOL read_mark(CONNECTION_NODE *pconnection)
{
	fd_set myset;
	int i, read_len;
	struct timeval tv;

	while (TRUE) {
		tv.tv_usec = 0;
		tv.tv_sec = g_max_interval;
		FD_ZERO(&myset);
		FD_SET(pconnection->sockd, &myset);
		if (select(pconnection->sockd + 1, &myset, NULL, NULL, &tv) <= 0) {
			return FALSE;
		}
		read_len = read(pconnection->sockd, pconnection->buffer +
					pconnection->offset, 1024 - pconnection->offset);
		if (read_len <= 0) {
			return FALSE;
		}
		pconnection->offset += read_len;
		for (i=0; i<pconnection->offset-1; i++) {
			if ('\r' == pconnection->buffer[i] &&
				'\n' == pconnection->buffer[i + 1]) {
				memcpy(pconnection->line, pconnection->buffer, i);
				pconnection->line[i] = '\0';
				pconnection->offset -= i + 2;
				memmove(pconnection->buffer, pconnection->buffer + i + 2,
					pconnection->offset);
				return TRUE;
			}
		}
		if (1024 == pconnection->offset) {
			return FALSE;
		}
		
	}
}

static void term_handler(int signo)
{
	g_notify_stop = TRUE;
}

