#include "list_file.h"
#include "ext_buffer.h"
#include "double_list.h"
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>
#include <net/if.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>


#define REBUILD_VERSION					"1.0"

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
#define CALL_ID_UNLOAD_STORE						0x80

typedef struct _EXMDB_ITEM {
	char prefix[256];
	char type[16];
	char ip_addr[16];
	int port;
} EXMDB_ITEM;

typedef struct _EXMDB_NODE {
	DOUBLE_LIST_NODE node;
	EXMDB_ITEM exmdb_info;
} EXMDB_NODE;

typedef struct _CONNECT_REQUEST {
	char *prefix;
	char *remote_id;
	BOOL b_private;
} CONNECT_REQUEST;

typedef struct _UNLOAD_STORE_REQUEST {
	const char *dir;
} UNLOAD_STORE_REQUEST;

static DOUBLE_LIST g_exmdb_list;

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

static int exmdb_client_push_unload_store_request(
	EXT_PUSH *pext, const UNLOAD_STORE_REQUEST *r)
{
	return ext_buffer_push_string(pext, r->dir);
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
	case CALL_ID_UNLOAD_STORE:
		status = exmdb_client_push_unload_store_request(
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
	pbin->pb = NULL;
	while (TRUE) {
		tv.tv_usec = 0;
		tv.tv_sec = SOCKET_TIMEOUT;
		FD_ZERO(&myset);
		FD_SET(sockd, &myset);
		if (select(sockd + 1, &myset, NULL, NULL, &tv) <= 0) {
			if (NULL != pbin->pb) {
				free(pbin->pb);
				pbin->pb = NULL;
			}
			return FALSE;
		}
		if (0 == pbin->cb) {
			read_len = read(sockd, resp_buff, 5);
			if (1 == read_len) {
				pbin->cb = 1;
				pbin->pb = malloc(1);
				if (NULL == pbin->pb) {
					return FALSE;
				}
				*(uint8_t*)pbin->pb = resp_buff[0];
				return TRUE;
			} else if (5 == read_len) {
				pbin->cb = *(uint32_t*)(resp_buff + 1) + 5;
				pbin->pb = malloc(pbin->cb);
				if (NULL == pbin->pb) {
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
			free(pbin->pb);
			pbin->pb = NULL;
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
	int written_len;
	uint32_t offset;
	
	offset = 0;
	while (TRUE) {
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

static int connect_exmdb(const char *dir)
{
	int sockd;
	int prefix_len;
	int process_id;
	BINARY tmp_bin;
	struct timeval tv;
	EXMDB_NODE *pexnode;
	char remote_id[128];
	char tmp_buff[1024];
	const char *str_host;
	uint8_t response_code;
	CONNECT_REQUEST request;
	DOUBLE_LIST_NODE *pnode;
	struct sockaddr_in servaddr;
	
	for (pnode=double_list_get_head(&g_exmdb_list); NULL!=pnode;
		pnode=double_list_get_after(&g_exmdb_list, pnode)) {
		pexnode = (EXMDB_NODE*)pnode->pdata;
		prefix_len = strlen(pexnode->exmdb_info.prefix);
		if (0 == strncmp(pexnode->exmdb_info.prefix, dir, prefix_len)) {
			break;
		}
	}
	if (NULL == pnode) {
		return -1;
	}
    sockd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(pexnode->exmdb_info.port);
    inet_pton(AF_INET, pexnode->exmdb_info.ip_addr, &servaddr.sin_addr);
    if (0 != connect(sockd,
		(struct sockaddr*)&servaddr,
		sizeof(servaddr))) {
        close(sockd);
        return -1;
    }
	process_id = getpid();
	sprintf(remote_id, "freebusy:%d", process_id);
	request.prefix = pexnode->exmdb_info.prefix;
	request.remote_id = remote_id;
	request.b_private = TRUE;
	if (EXT_ERR_SUCCESS != exmdb_client_push_request(
		CALL_ID_CONNECT, &request, &tmp_bin)) {
		close(sockd);
		return -1;
	}
	if (FALSE == exmdb_client_write_socket(sockd, &tmp_bin)) {
		close(sockd);
		return -1;
	}
	tmp_bin.pb = tmp_buff;
	if (FALSE == exmdb_client_read_socket(sockd, &tmp_bin)) {
		close(sockd);
		return -1;
	}
	response_code = tmp_bin.pb[0];
	if (RESPONSE_CODE_SUCCESS == response_code) {
		if (5 != tmp_bin.cb || 0 != *(uint32_t*)(tmp_bin.pb + 1)) {
			fprintf(stderr, "response format error when connect to "
				"%s:%d for prefix \"%s\"\n", pexnode->exmdb_info.ip_addr,
				pexnode->exmdb_info.port, pexnode->exmdb_info.prefix);
			close(sockd);
			return -1;
		}
		return sockd;
	}
	switch (response_code) {
	case RESPONSE_CODE_ACCESS_DENY:
		fprintf(stderr, "fail to connect to %s:%d for prefix "
			"\"%s\", access deny!\n", pexnode->exmdb_info.ip_addr,
			pexnode->exmdb_info.port, pexnode->exmdb_info.prefix);
		break;
	case RESPONSE_CODE_MAX_REACHED:
		fprintf(stderr, "fail to connect to %s:%d for prefix"
			" \"%s\",maximum connections reached in server!\n",
			pexnode->exmdb_info.ip_addr, pexnode->exmdb_info.port,
			pexnode->exmdb_info.prefix);
		break;
	case RESPONSE_CODE_LACK_MEMORY:
		fprintf(stderr, "fail to connect to %s:%d for prefix \"%s\","
			"server out of memory!\n", pexnode->exmdb_info.ip_addr,
			pexnode->exmdb_info.port, pexnode->exmdb_info.prefix);
		break;
	case RESPONSE_CODE_MISCONFIG_PREFIX:
		fprintf(stderr, "fail to connect to %s:%d for prefix \"%s\","
			"server does not serve the prefix, configuation file of "
			"client or server may be incorrect!", pexnode->exmdb_info.ip_addr,
			pexnode->exmdb_info.port, pexnode->exmdb_info.prefix);
		break;
	case RESPONSE_CODE_MISCONFIG_MODE:
		fprintf(stderr, "fail to connect to %s:%d for prefix \"%s\","
			" work mode with the prefix in server is different from "
			"the mode in client, configuation file of client or server"
			" may be incorrect!", pexnode->exmdb_info.ip_addr,
			pexnode->exmdb_info.port, pexnode->exmdb_info.prefix);
		break;
	default:
		fprintf(stderr, "fail to connect to %s:%d for prefix "
			"\"%s\", error code %d!\n", pexnode->exmdb_info.ip_addr,
			pexnode->exmdb_info.port, pexnode->exmdb_info.prefix,
			(int)response_code);
		break;
	}
	close(sockd);
	return -1;
}

static BOOL exmdb_client_unload_store(const char *dir)
{
	int sockd;
	BINARY tmp_bin;
	UNLOAD_STORE_REQUEST request;
	
	request.dir = dir;
	if (EXT_ERR_SUCCESS != exmdb_client_push_request(
		CALL_ID_UNLOAD_STORE, &request, &tmp_bin)) {
		return FALSE;
	}
	sockd = connect_exmdb(dir);
	if (-1 == sockd) {
		return FALSE;
	}
	if (FALSE == exmdb_client_write_socket(sockd, &tmp_bin)) {
		close(sockd);
		return FALSE;
	}
	if (FALSE == exmdb_client_read_socket(sockd, &tmp_bin)) {
		close(sockd);
		return FALSE;
	}
	if (5 != tmp_bin.cb || RESPONSE_CODE_SUCCESS != tmp_bin.pb[0]) {
		close(sockd);
		return FALSE;
	}
	close(sockd);
	return TRUE;
}

int main(int argc, char **argv)
{
	int i, fd;
	int list_num;
	int str_size;
	int str_size1;
	char *err_msg;
	LIST_FILE *plist;
	char *sql_string;
	sqlite3 *psqlite;
	EXMDB_ITEM *pitem;
	char tmp_sql[1024];
	EXMDB_NODE *pexnode;
	const char *presult;
	sqlite3_stmt *pstmt;
	char temp_path[256];
	char temp_path1[256];
	struct stat node_stat;
	
	if (2 == argc && 0 == strcmp(argv[1], "--version")) {
		printf("version: %s\n", REBUILD_VERSION);
		exit(0);
	}

	if (2 == argc && 0 == strcmp(argv[1], "--help")) {
		printf("usage: %s <maildir>\n", argv[0]);
		exit(0);
	}

	if (2 != argc) {
		printf("usage: %s <maildir>\n", argv[0]);
		exit(-1);
	}
	umask(0);
	snprintf(temp_path, 256, "%s/exmdb/exchange.sqlite3", argv[1]);
	if (0 != stat(temp_path, &node_stat)) {
		printf("can not find sotre database,"
			" %s does not exit\n", temp_path);
		exit(-1);
	}
	
	if (0 != stat("../doc/sqlite3_common.txt", &node_stat)) {
		printf("can not find store template"
			" file \"sqlite3_common.txt\"\n");	
		exit(-2);
	}
	if (0 == S_ISREG(node_stat.st_mode)) {
		printf("\"sqlite3_common.txt\" is not a regular file\n");
		exit(-3);
	}
	str_size = node_stat.st_size;
	
	if (0 != stat("../doc/sqlite3_private.txt", &node_stat)) {
		printf("can not find store template "
			"file \"sqlite3_private.txt\"\n");	
		exit(-4);
	}
	if (0 == S_ISREG(node_stat.st_mode)) {
		printf("\"sqlite3_private.txt\" is not a regular file\n");
		exit(-5);
	}
	str_size1 = node_stat.st_size;
	
	sql_string = malloc(str_size + str_size1 + 1);
	if (NULL == sql_string) {
		printf("fail to allocate memory\n");
		exit(-6);
	}
	fd = open("../doc/sqlite3_common.txt", O_RDONLY);
	if (-1 == fd) {
		printf("fail to open \"sqlite3_common.txt\" for reading\n");
		free(sql_string);
		exit(-7);
	}
	if (str_size != read(fd, sql_string, str_size)) {
		printf("fail to read content from store"
			" template file \"sqlite3_common.txt\"\n");
		close(fd);
		free(sql_string);
		exit(-7);
	}
	close(fd);
	fd = open("../doc/sqlite3_private.txt", O_RDONLY);
	if (-1 == fd) {
		printf("fail to open \"sqlite3_private.txt\" for reading\n");
		free(sql_string);
		exit(-7);
	}
	if (str_size1 != read(fd, sql_string + str_size, str_size1)) {
		printf("fail to read content from store"
			" template file \"sqlite3_private.txt\"\n");
		close(fd);
		free(sql_string);
		exit(-7);
	}
	close(fd);
	sql_string[str_size + str_size1] = '\0';
	if (SQLITE_OK != sqlite3_initialize()) {
		printf("fail to initialize sqlite engine\n");
		free(sql_string);
		exit(-8);
	}
	snprintf(temp_path1, 256, "%s/exmdb/new.sqlite3", argv[1]);
	remove(temp_path1);
	if (SQLITE_OK != sqlite3_open_v2(temp_path1, &psqlite,
		SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE, NULL)) {
		printf("fail to create store database\n");
		free(sql_string);
		sqlite3_shutdown();
		exit(-9);
	}
	chmod(temp_path1, 0666);
	sqlite3_exec(psqlite, "BEGIN TRANSACTION", NULL, NULL, NULL);
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table creation sql, error: %s\n", err_msg);
		free(sql_string);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	free(sql_string);
	/* commit the transaction */
	sqlite3_exec(psqlite, "COMMIT TRANSACTION", NULL, NULL, NULL);
	snprintf(tmp_sql, 1024, "ATTACH DATABASE "
		"'%s/exmdb/exchange.sqlite3' AS source_db", argv[1]);
	if (SQLITE_OK != sqlite3_exec(psqlite,
		tmp_sql, NULL, NULL, &err_msg)) {
		printf("fail to excute attach database sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	
	sqlite3_exec(psqlite, "BEGIN TRANSACTION", NULL, NULL, NULL);
	sql_string = "INSERT INTO configurations "
		"SELECT * FROM source_db.configurations";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO allocated_eids "
		"SELECT * FROM source_db.allocated_eids";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO named_properties "
		"SELECT * FROM source_db.named_properties";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO store_properties "
		"SELECT * FROM source_db.store_properties";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO permissions "
		"SELECT * FROM source_db.permissions";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO rules "
		"SELECT * FROM source_db.rules";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO folders "
		"SELECT * FROM source_db.folders";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO folder_properties "
		"SELECT * FROM source_db.folder_properties";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO receive_table "
		"SELECT * FROM source_db.receive_table";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO messages "
		"SELECT * FROM source_db.messages";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO message_properties "
		"SELECT * FROM source_db.message_properties";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO message_changes "
		"SELECT * FROM source_db.message_changes";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO recipients "
		"SELECT * FROM source_db.recipients";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO recipients_properties "
		"SELECT * FROM source_db.recipients_properties";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO attachments "
		"SELECT * FROM source_db.attachments";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO attachment_properties "
		"SELECT * FROM source_db.attachment_properties";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO search_scopes "
		"SELECT * FROM source_db.search_scopes";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "INSERT INTO search_result "
		"SELECT * FROM source_db.search_result";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute table copy sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	/* commit the transaction */
	sqlite3_exec(psqlite, "COMMIT TRANSACTION", NULL, NULL, NULL);
	sqlite3_exec(psqlite, "DETACH DATABASE source_db", NULL, NULL, NULL);
	sql_string = "REINDEX";
	if (SQLITE_OK != sqlite3_exec(psqlite,
		sql_string, NULL, NULL, &err_msg)) {
		printf("fail to excute reindex sql, error: %s\n", err_msg);
		sqlite3_close(psqlite);
		sqlite3_shutdown();
		exit(-9);
	}
	sql_string = "PRAGMA integrity_check";
	if (SQLITE_OK == sqlite3_prepare_v2(
		psqlite, sql_string, -1, &pstmt, NULL )) {
		if (SQLITE_ROW == sqlite3_step(pstmt)) {
			presult = sqlite3_column_text(pstmt, 0);
			if (NULL == presult || 0 != strcmp(presult, "ok")) {
				printf("new database is still "
					"malformed, can not be fixed!\n");
				exit(-10);
			}
		}
		sqlite3_finalize(pstmt);
	}
	sqlite3_close(psqlite);
	sqlite3_shutdown();
	
	double_list_init(&g_exmdb_list);
	plist = list_file_init("../data/exmdb_list.txt", "%s:256%s:16%s:16%d");
	if (NULL == plist) {
		printf("fail to open exmdb list file\n");
		exit(-11);
	}
	list_num = list_file_get_item_num(plist);
	pitem = (EXMDB_ITEM*)list_file_get_list(plist);
	for (i=0; i<list_num; i++) {
		if (0 != strcasecmp(pitem[i].type, "private")) {
			continue;
		}
		pexnode = malloc(sizeof(EXMDB_NODE));
		if (NULL == pexnode) {
			continue;
		}
		pexnode->node.pdata = pexnode;
		memcpy(&pexnode->exmdb_info, pitem + i, sizeof(EXMDB_ITEM));
		double_list_append_as_tail(&g_exmdb_list, &pexnode->node);
	}
	list_file_free(plist);
	if (FALSE == exmdb_client_unload_store(argv[1])) {
		printf("fail to unload store\n");
		exit(-12);
	}
	remove(temp_path);
	link(temp_path1, temp_path);
	remove(temp_path1);
	exit(0);
}
