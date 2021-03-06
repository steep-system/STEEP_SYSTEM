#include "endian_macro.h"
#include "exmdb_client.h"
#include "double_list.h"
#include "hook_common.h"
#include "ext_buffer.h"
#include "list_file.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <net/if.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>

#define SOCKET_TIMEOUT								60

#define RESPONSE_CODE_SUCCESS						0x00
#define RESPONSE_CODE_ACCESS_DENY					0x01
#define RESPONSE_CODE_MAX_REACHED					0x02
#define RESPONSE_CODE_LACK_MEMORY					0x03
#define RESPONSE_CODE_MISCONFIG_PREFIX				0x04
#define RESPONSE_CODE_MISCONFIG_MODE				0x05
#define RESPONSE_CODE_CONNECT_UNCOMPLETE			0x06
#define RESPONSE_CODE_PULL_ERROR					0x07
#define RESPONSE_CODE_DISPATCH_ERROR				0x08
#define RESPONSE_CODE_PUSH_ERROR					0x09

#define CALL_ID_CONNECT								0x00
#define CALL_ID_DELIVERY_MESSAGE					0x6d
#define CALL_ID_CHECK_CONTACT_ADDRESS				0x79


typedef struct _EXMDB_ITEM {
	char prefix[256];
	char type[16];
	char ip_addr[16];
	int port;
} EXMDB_ITEM;

typedef struct _REMOTE_SVR {
	DOUBLE_LIST_NODE node;
	DOUBLE_LIST conn_list;
	char ip_addr[16];
	char prefix[256];
	int prefix_len;
	BOOL b_private;
	int port;
} REMOTE_SVR;

typedef struct _REMOTE_CONN {
    DOUBLE_LIST_NODE node;
	time_t last_time;
	REMOTE_SVR *psvr;
	int sockd;
} REMOTE_CONN;

typedef struct _CONNECT_REQUEST {
	char *prefix;
	char *remote_id;
	BOOL b_private;
} CONNECT_REQUEST;

typedef struct _DELIVERY_MESSAGE_REQUEST {
	const char *dir;
	const char *from_address;
	const char *account;
	uint32_t cpid;
	const MESSAGE_CONTENT *pmsg;
	const char *pdigest;
} DELIVERY_MESSAGE_REQUEST;

typedef struct _CHECK_CONTACT_ADDRESS_REQUEST {
	const char *dir;
	const char *paddress;
} CHECK_CONTACT_ADDRESS_REQUEST;

static int g_conn_num;
static BOOL g_notify_stop;
static pthread_t g_scan_id;
static char g_list_path[256];
static DOUBLE_LIST g_lost_list;
static DOUBLE_LIST g_server_list;
static pthread_mutex_t g_server_lock;


static int exmdb_client_push_connect_request(
	EXT_PUSH *pext, const CONNECT_REQUEST *r)
{
	int status;
	
	status = ext_buffer_push_string(pext, r->prefix);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_string(pext, r->remote_id);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	return ext_buffer_push_bool(pext, r->b_private);
}

static int exmdb_client_push_delivery_message_request(
	EXT_PUSH *pext, const DELIVERY_MESSAGE_REQUEST *r)
{
	int status;
	
	status = ext_buffer_push_string(pext, r->dir);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_string(pext, r->from_address);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_string(pext, r->account);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_uint32(pext, r->cpid);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	status = ext_buffer_push_message_content(pext, r->pmsg);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	return ext_buffer_push_string(pext, r->pdigest);
}

static int exmdb_client_push_check_contact_address_request(
	EXT_PUSH *pext, const CHECK_CONTACT_ADDRESS_REQUEST *r)
{
	int status;
	
	status = ext_buffer_push_string(pext, r->dir);
	if (EXT_ERR_SUCCESS != status) {
		return status;
	}
	return ext_buffer_push_string(pext, r->paddress);
}

static int exmdb_client_push_request(uint8_t call_id,
	void *prequest, BINARY *pbin_out)
{
	int status;
	EXT_PUSH ext_push;
	
	if (FALSE == ext_buffer_push_init(
		&ext_push, NULL, 0, EXT_FLAG_WCOUNT)) {
		return EXT_ERR_ALLOC;
	}
	status = ext_buffer_push_advance(&ext_push, sizeof(uint32_t));
	if (EXT_ERR_SUCCESS != status) {
		ext_buffer_push_free(&ext_push);
		return status;
	}
	status = ext_buffer_push_uint8(&ext_push, call_id);
	if (EXT_ERR_SUCCESS != status) {
		ext_buffer_push_free(&ext_push);
		return status;
	}
	switch (call_id) {
	case CALL_ID_CONNECT:
		status = exmdb_client_push_connect_request(
								&ext_push, prequest);
		if (EXT_ERR_SUCCESS != status) {
			ext_buffer_push_free(&ext_push);
			return status;
		}
		break;
	case CALL_ID_DELIVERY_MESSAGE:
		status = exmdb_client_push_delivery_message_request(
										&ext_push, prequest);
		if (EXT_ERR_SUCCESS != status) {
			ext_buffer_push_free(&ext_push);
			return status;
		}
		break;
	case CALL_ID_CHECK_CONTACT_ADDRESS:
		status = exmdb_client_push_check_contact_address_request(
											&ext_push, prequest);
		if (EXT_ERR_SUCCESS != status) {
			ext_buffer_push_free(&ext_push);
			return status;
		}
		break;
	default:
		ext_buffer_push_free(&ext_push);
		return EXT_ERR_BAD_SWITCH;
	}
	pbin_out->cb = ext_push.offset;
	ext_push.offset = 0;
	ext_buffer_push_uint32(&ext_push,
		pbin_out->cb - sizeof(uint32_t));
	/* memory referneced by ext_push.data will be freed outside */
	pbin_out->pb = ext_push.data;
	return EXT_ERR_SUCCESS;
}

static BOOL exmdb_client_read_socket(int sockd, BINARY *pbin)
{
	fd_set myset;
	int read_len;
	uint32_t offset;
	struct timeval tv;
	uint8_t resp_buff[5];
	
	pbin->cb = 0;
	while (TRUE) {
		tv.tv_usec = 0;
		tv.tv_sec = SOCKET_TIMEOUT;
		FD_ZERO(&myset);
		FD_SET(sockd, &myset);
		if (select(sockd + 1, &myset, NULL, NULL, &tv) <= 0) {
			return FALSE;
		}
		if (0 == pbin->cb) {
			read_len = read(sockd, resp_buff, 5);
			if (1 == read_len) {
				pbin->cb = 1;
				*(uint8_t*)pbin->pb = resp_buff[0];
				return TRUE;
			} else if (5 == read_len) {
				pbin->cb = *(uint32_t*)(resp_buff + 1) + 5;
				if (pbin->cb >= 1024) {
					return FALSE;
				}
				memcpy(pbin->pb, resp_buff, 5);
				offset = 5;
				if (offset == pbin->cb) {
					return TRUE;
				}
				continue;
			} else {
				return FALSE;
			}
		}
		read_len = read(sockd,
			pbin->pb + offset,
			pbin->cb - offset);
		if (read_len <= 0) {
			return FALSE;
		}
		offset += read_len;
		if (offset == pbin->cb) {
			return TRUE;
		}
	}
}

static BOOL exmdb_client_write_socket(
	int sockd, const BINARY *pbin)
{
	fd_set myset;
	int written_len;
	uint32_t offset;
	struct timeval tv;
	
	offset = 0;
	while (TRUE) {
		tv.tv_usec = 0;
		tv.tv_sec = SOCKET_TIMEOUT;
		FD_ZERO(&myset);
		FD_SET(sockd, &myset);
		if (select(sockd + 1, NULL, &myset, NULL, &tv) <= 0) {
			return FALSE;
		}
		written_len = write(sockd,
				pbin->pb + offset,
				pbin->cb - offset);
		if (written_len <= 0) {
			return FALSE;
		}
		offset += written_len;
		if (offset == pbin->cb) {
			return TRUE;
		}
	}
}

static int exmdb_client_connect_exmdb(REMOTE_SVR *pserver)
{
	int sockd;
	int process_id;
	BINARY tmp_bin;
	struct timeval tv;
	char remote_id[128];
	char tmp_buff[1024];
	const char *str_host;
	uint8_t response_code;
	CONNECT_REQUEST request;
	struct sockaddr_in servaddr;
	
    sockd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(pserver->port);
    inet_pton(AF_INET, pserver->ip_addr, &servaddr.sin_addr);
    if (0 != connect(sockd,
		(struct sockaddr*)&servaddr,
		sizeof(servaddr))) {
        close(sockd);
        return -1;
    }
	str_host = get_host_ID();
	process_id = getpid();
	sprintf(remote_id, "%s:%d", str_host, process_id);
	request.prefix = pserver->prefix;
	request.remote_id = remote_id;
	request.b_private = pserver->b_private;
	if (EXT_ERR_SUCCESS != exmdb_client_push_request(
		CALL_ID_CONNECT, &request, &tmp_bin)) {
		close(sockd);
		return -1;
	}
	if (FALSE == exmdb_client_write_socket(sockd, &tmp_bin)) {
		free(tmp_bin.pb);
		close(sockd);
		return -1;
	}
	free(tmp_bin.pb);
	tmp_bin.pb = tmp_buff;
	if (FALSE == exmdb_client_read_socket(sockd, &tmp_bin)) {
		close(sockd);
		return -1;
	}
	response_code = tmp_bin.pb[0];
	if (RESPONSE_CODE_SUCCESS == response_code) {
		if (5 != tmp_bin.cb || 0 != *(uint32_t*)(tmp_bin.pb + 1)) {
			printf("[exmdb_local]: response format error "
				"when connect to %s:%d for prefix \"%s\"\n",
				pserver->ip_addr, pserver->port, pserver->prefix);
			close(sockd);
			return -1;
		}
		return sockd;
	}
	switch (response_code) {
	case RESPONSE_CODE_ACCESS_DENY:
		printf("[exmdb_local]: fail to connect to "
			"%s:%d for prefix \"%s\", access deny!\n",
			pserver->ip_addr, pserver->port, pserver->prefix);
		break;
	case RESPONSE_CODE_MAX_REACHED:
		printf("[exmdb_local]: fail to connect to %s:%d for "
			"prefix \"%s\",maximum connections reached in server!\n",
			pserver->ip_addr, pserver->port, pserver->prefix);
		break;
	case RESPONSE_CODE_LACK_MEMORY:
		printf("[exmdb_local]: fail to connect to %s:%d "
			"for prefix \"%s\", server out of memory!\n",
			pserver->ip_addr, pserver->port, pserver->prefix);
		break;
	case RESPONSE_CODE_MISCONFIG_PREFIX:
		printf("[exmdb_local]: fail to connect to %s:%d for "
			"prefix \"%s\", server does not serve the prefix, "
			"configuation file of client or server may be incorrect!",
			pserver->ip_addr, pserver->port, pserver->prefix);
		break;
	case RESPONSE_CODE_MISCONFIG_MODE:
		printf("[exmdb_local]: fail to connect to %s:%d for "
			"prefix \"%s\", work mode with the prefix in server is"
			"different from the mode in client, configuation file "
			"of client or server may be incorrect!",
			pserver->ip_addr, pserver->port, pserver->prefix);
		break;
	default:
		printf("[exmdb_local]: fail to connect to "
			"%s:%d for prefix \"%s\", error code %d!\n",
			pserver->ip_addr, pserver->port,
			pserver->prefix, (int)response_code);
		break;
	}
	close(sockd);
	return -1;
}

static void *scan_work_func(void *pparam)
{
	fd_set myset;
	time_t now_time;
	struct timeval tv;
	uint8_t resp_buff;
	uint32_t ping_buff;
	REMOTE_CONN *pconn;
	REMOTE_SVR *pserver;
	DOUBLE_LIST temp_list;
	DOUBLE_LIST_NODE *pnode;
	DOUBLE_LIST_NODE *ptail;
	DOUBLE_LIST_NODE *pnode1;
	
	
	ping_buff = 0;
	double_list_init(&temp_list);

	while (FALSE == g_notify_stop) {
		pthread_mutex_lock(&g_server_lock);
		time(&now_time);
		for (pnode=double_list_get_head(&g_server_list); NULL!=pnode;
			pnode=double_list_get_after(&g_server_list, pnode)) {
			pserver = (REMOTE_SVR*)pnode->pdata;
			ptail = double_list_get_tail(&pserver->conn_list);
			while (pnode1=double_list_get_from_head(&pserver->conn_list)) {
				pconn = (REMOTE_CONN*)pnode1->pdata;
				if (now_time - pconn->last_time >= SOCKET_TIMEOUT - 3) {
					double_list_append_as_tail(&temp_list, &pconn->node);
				} else {
					double_list_append_as_tail(&pserver->conn_list,
						&pconn->node);
				}

				if (pnode1 == ptail) {
					break;
				}
			}
		}
		pthread_mutex_unlock(&g_server_lock);

		while (pnode=double_list_get_from_head(&temp_list)) {
			pconn = (REMOTE_CONN*)pnode->pdata;
			if (TRUE == g_notify_stop) {
				close(pconn->sockd);
				free(pconn);
				continue;
			}
			if (sizeof(uint32_t) != write(pconn->sockd,
				&ping_buff, sizeof(uint32_t))) {
				close(pconn->sockd);
				pconn->sockd = -1;
				pthread_mutex_lock(&g_server_lock);
				double_list_append_as_tail(&g_lost_list, &pconn->node);
				pthread_mutex_unlock(&g_server_lock);
				continue;
			}
			tv.tv_usec = 0;
			tv.tv_sec = SOCKET_TIMEOUT;
			FD_ZERO(&myset);
			FD_SET(pconn->sockd, &myset);
			if (select(pconn->sockd + 1, &myset, NULL, NULL, &tv) <= 0 ||
				1 != read(pconn->sockd, &resp_buff, 1) ||
				RESPONSE_CODE_SUCCESS != resp_buff) {
				close(pconn->sockd);
				pconn->sockd = -1;
				pthread_mutex_lock(&g_server_lock);
				double_list_append_as_tail(&g_lost_list, &pconn->node);
				pthread_mutex_unlock(&g_server_lock);
			} else {
				time(&pconn->last_time);
				pthread_mutex_lock(&g_server_lock);
				double_list_append_as_tail(&pconn->psvr->conn_list,
					&pconn->node);
				pthread_mutex_unlock(&g_server_lock);
			}
		}

		pthread_mutex_lock(&g_server_lock);
		while (pnode=double_list_get_from_head(&g_lost_list)) {
			double_list_append_as_tail(&temp_list, pnode);
		}
		pthread_mutex_unlock(&g_server_lock);

		while (pnode=double_list_get_from_head(&temp_list)) {
			pconn = (REMOTE_CONN*)pnode->pdata;
			if (TRUE == g_notify_stop) {
				close(pconn->sockd);
				free(pconn);
				continue;
			}
			pconn->sockd = exmdb_client_connect_exmdb(pconn->psvr);
			if (-1 != pconn->sockd) {
				time(&pconn->last_time);
				pthread_mutex_lock(&g_server_lock);
				double_list_append_as_tail(&pconn->psvr->conn_list,
					&pconn->node);
				pthread_mutex_unlock(&g_server_lock);
			} else {
				pthread_mutex_lock(&g_server_lock);
				double_list_append_as_tail(&g_lost_list, &pconn->node);
				pthread_mutex_unlock(&g_server_lock);
			}
		}
		sleep(1);
	}
}

static REMOTE_CONN* exmdb_client_get_connection(const char *dir)
{
	REMOTE_SVR *pserver;
	DOUBLE_LIST_NODE *pnode;

	for (pnode=double_list_get_head(&g_server_list); NULL!=pnode;
		pnode=double_list_get_after(&g_server_list, pnode)) {
		pserver = (REMOTE_SVR*)pnode->pdata;
		if (0 == strncmp(dir, pserver->prefix, pserver->prefix_len)) {
			break;
		}
	}
	if (NULL == pnode) {
		printf("[exmdb_local]: cannot find remote server for %s\n", dir);
		return NULL;
	}
	pthread_mutex_lock(&g_server_lock);
	pnode = double_list_get_from_head(&pserver->conn_list);
	pthread_mutex_unlock(&g_server_lock);
	if (NULL == pnode) {
		printf("[exmdb_local]: no alive connection for"
			" remote server for %s\n", pserver->prefix);
		return NULL;
	}
	return (REMOTE_CONN*)pnode->pdata;
}

BOOL exmdb_client_get_exmdb_information(
	const char *dir, char *ip_addr, int *pport,
	int *pconn_num, int *palive_conn)
{
	REMOTE_SVR *pserver;
	DOUBLE_LIST_NODE *pnode;

	for (pnode=double_list_get_head(&g_server_list); NULL!=pnode;
		pnode=double_list_get_after(&g_server_list, pnode)) {
		pserver = (REMOTE_SVR*)pnode->pdata;
		if (0 == strncmp(dir, pserver->prefix, pserver->prefix_len)) {
			break;
		}
	}
	if (NULL == pnode) {
		return FALSE;
	}
	strcpy(ip_addr, pserver->ip_addr);
	*pport = pserver->port;
	*palive_conn = double_list_get_nodes_num(&pserver->conn_list);
	*pconn_num = g_conn_num;
	return TRUE;
}

static void exmdb_client_put_connection(REMOTE_CONN *pconn, BOOL b_lost)
{
	if (FALSE == b_lost) {
		pthread_mutex_lock(&g_server_lock);
		double_list_append_as_tail(&pconn->psvr->conn_list, &pconn->node);
		pthread_mutex_unlock(&g_server_lock);
	} else {
		close(pconn->sockd);
		pconn->sockd = -1;
		pthread_mutex_lock(&g_server_lock);
		double_list_append_as_tail(&g_lost_list, &pconn->node);
		pthread_mutex_unlock(&g_server_lock);
	}
}

void exmdb_client_init(int conn_num, const char *list_path)
{
	g_notify_stop = TRUE;
	g_conn_num = conn_num;
	strcpy(g_list_path, list_path);
	double_list_init(&g_server_list);
	double_list_init(&g_lost_list);
	pthread_mutex_init(&g_server_lock, NULL);
}

int exmdb_client_run()
{
	int i, j;
	int list_num;
	BOOL b_private;
	LIST_FILE *plist;
	EXMDB_ITEM *pitem;
	REMOTE_CONN *pconn;
	REMOTE_SVR *pserver;
	
	plist = list_file_init(g_list_path, "%s:256%s:16%s:16%d");
	if (NULL == plist) {
		printf("[exmdb_local]: fail to open exmdb list file\n");
		return 1;
	}
	g_notify_stop = FALSE;
	list_num = list_file_get_item_num(plist);
	pitem = (EXMDB_ITEM*)list_file_get_list(plist);
	for (i=0; i<list_num; i++) {
		if (0 == strcasecmp(pitem[i].type, "private")) {
			b_private = TRUE;
		} else if (0 == strcasecmp(pitem[i].type, "public")) {
			b_private = FALSE;
		} else {
			printf("[exmdb_local]: unknown type \"%s\", "
				"can only be \"private\" or \"public\"!");
			list_file_free(plist);
			g_notify_stop = TRUE;
			return 2;
		}
		pserver = malloc(sizeof(REMOTE_SVR));
		if (NULL == pserver) {
			printf("[exmdb_local]: fail to allocate memory for exmdb\n");
			list_file_free(plist);
			g_notify_stop = TRUE;
			return 3;
		}
		pserver->node.pdata = pserver;
		strcpy(pserver->prefix, pitem[i].prefix);
		pserver->prefix_len = strlen(pserver->prefix);
		pserver->b_private = b_private;
		strcpy(pserver->ip_addr, pitem[i].ip_addr);
		pserver->port = pitem[i].port;
		double_list_init(&pserver->conn_list);
		double_list_append_as_tail(&g_server_list, &pserver->node);
		for (j=0; j<g_conn_num; j++) {
		   pconn = malloc(sizeof(REMOTE_CONN));
			if (NULL == pconn) {
				printf("[exmdb_local]: fail to "
					"allocate memory for exmdb\n");
				list_file_free(plist);
				g_notify_stop = TRUE;
				return 4;
			}
			pconn->node.pdata = pconn;
			pconn->sockd = -1;
			pconn->psvr = pserver;
			double_list_append_as_tail(&g_lost_list, &pconn->node);
		}
	}
	list_file_free(plist);
	if (0 != pthread_create(&g_scan_id, NULL, scan_work_func, NULL)) {
		printf("[exmdb_local]: fail to create proxy scan thread\n");
		g_notify_stop = TRUE;
		return 5;
	}
	return 0;
}

int exmdb_client_stop()
{
	REMOTE_CONN *pconn;
	REMOTE_SVR *pserver;
	DOUBLE_LIST_NODE *pnode;
	
	if (FALSE == g_notify_stop) {
		g_notify_stop = TRUE;
		pthread_join(g_scan_id, NULL);
	}
	g_notify_stop = TRUE;
	while (pnode=double_list_get_from_head(&g_lost_list)) {
		free(pnode->pdata);
	}
	while (pnode=double_list_get_from_head(&g_server_list)) {
		pserver = (REMOTE_SVR*)pnode->pdata;
		while (pnode=double_list_get_from_head(&pserver->conn_list)) {
			pconn = (REMOTE_CONN*)pnode->pdata;
			close(pconn->sockd);
			free(pconn);
		}
		free(pserver);
	}
	return 0;
}

void exmdb_client_free()
{
	double_list_free(&g_lost_list);
	double_list_free(&g_server_list);
	pthread_mutex_destroy(&g_server_lock);
}

int exmdb_client_delivery_message(const char *dir,
	const char *from_address, const char *account,
	uint32_t cpid, const MESSAGE_CONTENT *pmsg,
	const char *pdigest)
{
	BINARY tmp_bin;
	uint32_t result;
	REMOTE_CONN *pconn;
	char tmp_buff[1024];
	DELIVERY_MESSAGE_REQUEST request;
	
	request.dir = dir;
	request.from_address = from_address;
	request.account = account;
	request.cpid = cpid;
	request.pmsg = pmsg;
	request.pdigest = pdigest;
	if (EXT_ERR_SUCCESS != exmdb_client_push_request(
		CALL_ID_DELIVERY_MESSAGE, &request, &tmp_bin)) {
		return EXMDB_RUNTIME_ERROR;
	}
	pconn = exmdb_client_get_connection(dir);
	if (NULL == pconn) {
		free(tmp_bin.pb);
		return EXMDB_NO_SERVER;
	}
	if (FALSE == exmdb_client_write_socket(pconn->sockd, &tmp_bin)) {
		free(tmp_bin.pb);
		exmdb_client_put_connection(pconn, TRUE);
		return EXMDB_RDWR_ERROR;
	}
	free(tmp_bin.pb);
	tmp_bin.pb = tmp_buff;
	if (FALSE == exmdb_client_read_socket(pconn->sockd, &tmp_bin)) {
		exmdb_client_put_connection(pconn, TRUE);
		return EXMDB_RDWR_ERROR;
	}
	time(&pconn->last_time);
	exmdb_client_put_connection(pconn, FALSE);
	if (5 + sizeof(uint32_t) != tmp_bin.cb ||
		RESPONSE_CODE_SUCCESS != tmp_buff[0]) {
		return EXMDB_RUNTIME_ERROR;
	}
	result = IVAL(tmp_buff, 5);
	if (0 == result) {
		return EXMDB_RESULT_OK;
	} else if (1 == result) {
		return EXMDB_MAILBOX_FULL;
	} else {
		return EXMDB_RESULT_ERROR;
	}
}

int exmdb_client_check_contact_address(const char *dir,
	const char *paddress, BOOL *pb_found)
{
	BINARY tmp_bin;
	uint32_t result;
	REMOTE_CONN *pconn;
	char tmp_buff[1024];
	CHECK_CONTACT_ADDRESS_REQUEST request;
	
	request.dir = dir;
	request.paddress = paddress;
	if (EXT_ERR_SUCCESS != exmdb_client_push_request(
		CALL_ID_CHECK_CONTACT_ADDRESS, &request, &tmp_bin)) {
		return EXMDB_RUNTIME_ERROR;
	}
	pconn = exmdb_client_get_connection(dir);
	if (NULL == pconn) {
		free(tmp_bin.pb);
		return EXMDB_NO_SERVER;
	}
	if (FALSE == exmdb_client_write_socket(pconn->sockd, &tmp_bin)) {
		free(tmp_bin.pb);
		exmdb_client_put_connection(pconn, TRUE);
		return EXMDB_RDWR_ERROR;
	}
	free(tmp_bin.pb);
	tmp_bin.pb = tmp_buff;
	if (FALSE == exmdb_client_read_socket(pconn->sockd, &tmp_bin)) {
		exmdb_client_put_connection(pconn, TRUE);
		return EXMDB_RDWR_ERROR;
	}
	time(&pconn->last_time);
	exmdb_client_put_connection(pconn, FALSE);
	if (5 + sizeof(uint8_t) != tmp_bin.cb ||
		RESPONSE_CODE_SUCCESS != tmp_buff[0]) {
		return EXMDB_RUNTIME_ERROR;
	}
	if (0 == tmp_bin.pb[5]) {
		*pb_found = FALSE;
	} else {
		*pb_found = TRUE;
	}
	return EXMDB_RESULT_OK;
}
