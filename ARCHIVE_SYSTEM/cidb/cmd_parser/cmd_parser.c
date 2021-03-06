#include "common_types.h"
#include "double_list.h"
#include "cmd_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#define MAX_ARGS			(32*1024)

#define CONN_BUFFLEN        (257*1024)

typedef struct _COMMAND_ENTRY {
	char cmd[64];
	COMMAND_HANDLER cmd_handler;
} COMMAND_ENTRY;

static int g_cmd_num;
static int g_threads_num;
static BOOL g_notify_stop;
static int g_timeout_interval;
static pthread_t *g_thread_ids;
static pthread_mutex_t g_connection_lock;
static pthread_mutex_t g_cond_mutex;
static pthread_cond_t g_waken_cond;
static DOUBLE_LIST g_connection_list;
static DOUBLE_LIST g_connection_list1;
static COMMAND_ENTRY g_cmd_entry[128];


static void *thread_work_func(void *param);

static int cmd_parser_generate_args(char* cmd_line, int cmd_len, char** argv);

static int cmd_parser_ping(int argc, char **argv, int sockd);

void cmd_parser_init(int threads_num, int timeout)
{
	g_cmd_num = 0;
	g_threads_num = threads_num;
	g_timeout_interval = timeout;
	
	pthread_mutex_init(&g_connection_lock, NULL);
	pthread_mutex_init(&g_cond_mutex, NULL);
	pthread_cond_init(&g_waken_cond, NULL);

	double_list_init(&g_connection_list);
	double_list_init(&g_connection_list1);
}


void cmd_parser_free()
{

	double_list_free(&g_connection_list);
	double_list_free(&g_connection_list1);

	pthread_mutex_destroy(&g_connection_lock);
	pthread_mutex_destroy(&g_cond_mutex);
	pthread_cond_destroy(&g_waken_cond);
}

CONNECTION* cmd_parser_get_connection()
{
	CONNECTION *pconnection;

	pthread_mutex_lock(&g_connection_lock);
	if (double_list_get_nodes_num(&g_connection_list) + 1 +
		double_list_get_nodes_num(&g_connection_list1) >= g_threads_num) {
		pthread_mutex_unlock(&g_connection_lock);
		return NULL;
	}
	
	pthread_mutex_unlock(&g_connection_lock);

	pconnection = (CONNECTION*)malloc(sizeof(CONNECTION));

	pconnection->node.pdata = pconnection;

	return pconnection;
}

void cmd_parser_put_connection(CONNECTION *pconnection)
{	
	pthread_mutex_lock(&g_connection_lock);
	double_list_append_as_tail(&g_connection_list1, &pconnection->node);
	pthread_mutex_unlock(&g_connection_lock);
	pthread_cond_signal(&g_waken_cond);
}


int cmd_parser_run()
{
	int i;


	cmd_parser_register_command("PING", cmd_parser_ping);

	g_thread_ids = (pthread_t*)malloc(g_threads_num*sizeof(pthread_t));

	g_notify_stop = FALSE;

	for (i=0; i<g_threads_num; i++) {
		if (0 != pthread_create(&g_thread_ids[i], NULL,
			thread_work_func, NULL)) {
			printf("[cmd_parser]: fail to create pool thread\n");
			goto FAILURE_EXIT;
		}
	}


	return 0;

FAILURE_EXIT:

	for (i-=1; i>=0; i--) {
		pthread_cancel(g_thread_ids[i]);
	}

	return -1;
}



int cmd_parser_stop()
{
	int i;
	DOUBLE_LIST_NODE *pnode;
	CONNECTION *pconnection;

	g_notify_stop = TRUE;
	pthread_cond_broadcast(&g_waken_cond);
	pthread_mutex_lock(&g_connection_lock);
	for (pnode=double_list_get_head(&g_connection_list); NULL!=pnode;
		pnode=double_list_get_after(&g_connection_list, pnode)) {
		pconnection = (CONNECTION*)pnode->pdata;
		if (TRUE == pconnection->is_selecting) {
			pthread_cancel(pconnection->thr_id);
		} else {
			close(pconnection->sockd);
			pconnection->sockd = -1;
		}	
	}
	pthread_mutex_unlock(&g_connection_lock);

	for (i=0; i<g_threads_num; i++) {
		pthread_join(g_thread_ids[i], NULL);
	}

	while (pnode=double_list_get_from_head(&g_connection_list)) {
		pconnection = (CONNECTION*)pnode->pdata;
		if (-1 != pconnection->sockd) {
			close(pconnection->sockd);
		}
		free(pconnection);
	}

	while (pnode=double_list_get_from_head(&g_connection_list1)) {
		pconnection = (CONNECTION*)pnode->pdata;
		close(pconnection->sockd);
		free(pconnection);
	}
	return 0;
}


void cmd_parser_register_command(const char *command, COMMAND_HANDLER handler)
{
	strcpy(g_cmd_entry[g_cmd_num].cmd, command);
	g_cmd_entry[g_cmd_num].cmd_handler = handler;
	g_cmd_num ++;
}


static void *thread_work_func(void *param)
{
	int i, j;
	int argc;
	int result;
	int offset;
	int temp_len;
	int read_len;
	fd_set myset;
	struct timeval tv;
	char *argv[MAX_ARGS];
	char temp_response[128];
	CONNECTION *pconnection;
	DOUBLE_LIST_NODE *pnode;
	char buffer[CONN_BUFFLEN];

NEXT_LOOP:
	if (TRUE == g_notify_stop) {
		pthread_exit(0);
	}

	pthread_mutex_lock(&g_cond_mutex);
	pthread_cond_wait(&g_waken_cond, &g_cond_mutex);
	pthread_mutex_unlock(&g_cond_mutex);

	if (TRUE == g_notify_stop) {
		pthread_exit(0);
	}
	
	pthread_mutex_lock(&g_connection_lock);
	pnode = double_list_get_from_head(&g_connection_list1);
	if (NULL != pnode) {
		double_list_append_as_tail(&g_connection_list, pnode);
	}
	pthread_mutex_unlock(&g_connection_lock);
	if (NULL == pnode) {
		goto NEXT_LOOP;
	}
	pconnection = (CONNECTION*)pnode->pdata;

	offset = 0;

    while (FALSE == g_notify_stop) {
		tv.tv_usec = 0;
		tv.tv_sec = g_timeout_interval;
		FD_ZERO(&myset);
		FD_SET(pconnection->sockd, &myset);
		pconnection->is_selecting = TRUE;
		pconnection->thr_id = pthread_self();
		if (select(pconnection->sockd + 1, &myset, NULL, NULL, &tv) <= 0) {
			pconnection->is_selecting = FALSE;
			pthread_mutex_lock(&g_connection_lock);
			double_list_remove(&g_connection_list, &pconnection->node);
			pthread_mutex_unlock(&g_connection_lock);
			close(pconnection->sockd);
			free(pconnection);
			goto NEXT_LOOP;
		}
		pconnection->is_selecting = FALSE;
		read_len = read(pconnection->sockd, buffer + offset,
					CONN_BUFFLEN - offset);
		if (read_len <= 0) {
			pthread_mutex_lock(&g_connection_lock);
			double_list_remove(&g_connection_list, &pconnection->node);
			pthread_mutex_unlock(&g_connection_lock);
			close(pconnection->sockd);
			free(pconnection);
			goto NEXT_LOOP;
		}
		offset += read_len;
		for (i=0; i<offset-1; i++) {
			if ('\r' == buffer[i] && '\n' == buffer[i + 1]) {
				if (4 == i && 0 == strncasecmp(buffer, "QUIT", 4)) {
					write(pconnection->sockd, "BYE\r\n", 5);
					pthread_mutex_lock(&g_connection_lock);
					double_list_remove(&g_connection_list, &pconnection->node);
					pthread_mutex_unlock(&g_connection_lock);
					close(pconnection->sockd);
					free(pconnection);
					goto NEXT_LOOP;
				}

				argc = cmd_parser_generate_args(buffer, i, argv);
				if(0 == argc) {
					write(pconnection->sockd, "FALSE 1\r\n", 9);
					offset -= i + 2;
					memmove(buffer, buffer + i + 2, offset);
					break;	
				}
				
				/* compare build-in command */
				for (j=0; j<g_cmd_num; j++) {
					if (FALSE == g_notify_stop &&
						0 == strcasecmp(g_cmd_entry[j].cmd, argv[0])) {
						result = g_cmd_entry[j].cmd_handler(argc, argv,
									pconnection->sockd);
						if (0 != result) {
							temp_len = sprintf(temp_response,
										"FALSE %d\r\n", result);
							write(pconnection->sockd, temp_response, temp_len);
						}
						break;
					}
				}
				
				if (FALSE == g_notify_stop && j == g_cmd_num) {
					write(pconnection->sockd, "FALSE 0\r\n", 9);
				}

				offset -= i + 2;
				memmove(buffer, buffer + i + 2, offset);
				break;	
			}
		}

		if (CONN_BUFFLEN == offset) {
			pthread_mutex_lock(&g_connection_lock);
			double_list_remove(&g_connection_list, &pconnection->node);
			pthread_mutex_unlock(&g_connection_lock);
			close(pconnection->sockd);
			free(pconnection);
			goto NEXT_LOOP;
		}
	}
	
	pthread_exit(0);

}

static int cmd_parser_ping(int argc, char **argv, int sockd)
{
	write(sockd, "TRUE\r\n", 6);
	return 0;
}

static int cmd_parser_generate_args(char* cmd_line, int cmd_len, char** argv)
{
	int argc;                    /* number of args */
	char *ptr;                   /* ptr that traverses command line  */
	char *last_space;
	
	cmd_line[cmd_len] = ' ';
	cmd_line[cmd_len + 1] = '\0';
	ptr = cmd_line;
	/* Build the argv list */
	argc = 0;
	last_space = cmd_line;
	while (*ptr != '\0') {
		if ('{' == *ptr) {
			if ('}' != cmd_line[cmd_len - 1]) {
				return 0;
			}
			argv[argc] = ptr;
			cmd_line[cmd_len] = '\0';
			argc ++;
			break;
		}

		if (' ' == *ptr) {
			/* ignore leading spaces */
			if (ptr == last_space) {
				last_space ++;
			} else {
				argv[argc] = last_space;
				*ptr = '\0';
				last_space = ptr + 1;
				argc ++;
			}
		}
		ptr ++;
	}
	
	argv[argc] = NULL;
	return argc;
}	

