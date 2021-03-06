#include "single_list.h"
#include "util.h"
#include "mail.h"
#include "mime_pool.h"
#include "mail_func.h"
#include "midb_client.h"
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define SENDMAIL_VERSION    "2.0"

#define SOCKET_TIMEOUT      30

typedef struct _RCPT_ITEM {
	SINGLE_LIST_NODE node;
	char address[256];
} RCPT_ITEM;

enum {
	SMTP_SEND_OK = 0,
	SMTP_CANNOT_CONNECT,
	SMTP_CONNECT_ERROR,
	SMTP_TIME_OUT,
	SMTP_TEMP_ERROR,
	SMTP_UNKOWN_RESPONSE,
	SMTP_PERMANENT_ERROR
};


static void parse_rcpt(char *field, SINGLE_LIST *plist);

static BOOL send_command(int sockd, const char *command, int command_len);

static int get_response(int sockd, char *response, int response_len,
	BOOL expect_3xx);

static BOOL move_to_sent(const char *maildir,
	const char *folder_name, const char *mid_string)
{
	BOOL b_result;
	
	midb_client_init("../data/midb_list.txt");
	if (0 != midb_client_run()) {
		return FALSE;
	}
	b_result = midb_client_move(maildir, folder_name, mid_string, "sent");
	midb_client_stop();
	midb_client_free();
	return b_result;
}

int main(int argc, char **argv)
{
	MAIL imail;
	char *pbuff;
	char *ptoken;
	fd_set myset;
	char **p_addr;
	char *str_value;
	char smtp_ip[16];
	MIME_POOL *ppool;
	MIME *pmime_head;
	RCPT_ITEM *pitem;
	BOOL b_connected;
	struct timeval tv;
	char temp_path[256];
	SINGLE_LIST temp_list;
	struct stat node_stat;
	struct hostent *phost;
	EMAIL_ADDR email_addr;
	struct in_addr ip_addr;
	SINGLE_LIST_NODE *pnode;
	char last_command[1024];
	char last_response[1024];
	struct sockaddr_in servaddr;
	char temp_field[MIME_FIELD_LEN];
	int smtp_port, res_val, command_len;
	int fd, sockd, opt, val_opt, opt_len;
	

	if (2 == argc && 0 == strcmp(argv[1], "--help")) {
		printf("usage: %s username maildir folder_name"
					" mid_string ip:port\n", argv[0]);
		exit(0);
	}

	if (2 == argc && 0 == strcmp(argv[1], "--version")) {
		printf("version: %s\n", SENDMAIL_VERSION);
		exit(0);
	}

	if (6 != argc) {
		printf("usage: %s username maildir folder_name"
					" mid_string ip:port\n", argv[0]);
		exit(-1);
	}
	snprintf(temp_path, 256, "%s/eml/%s", argv[2], argv[4]);
	if (0 != stat(temp_path, &node_stat)) {
		printf("can not find mail file %s\n", temp_path);
		exit(-2);
	}

	if (0 == S_ISREG(node_stat.st_mode)) {
		printf("%s is not regular file\n", temp_path);
		exit(-3);
	}

	if (NULL == extract_ip(argv[5], smtp_ip)) {
		printf("cannot find smtp server ip address in %s", argv[5]);
		exit(-4);
	}

	ptoken = strchr(argv[5], ':');
	if (NULL == ptoken) {
		smtp_port = 25;
	} else {
		smtp_port = atoi(ptoken + 1);
		if (0 == smtp_port) {
			printf("smtp server port error in %s\n", argv[5]);
			exit(-5);
		}
	}

	pbuff = malloc(node_stat.st_size);
	if (NULL == pbuff) {
		printf("fail to allocate memory for reading mail\n");
		exit(-6);
	}

	fd = open(temp_path, O_RDONLY);
	if (-1 == fd) {
		printf("fail to open mail file %s\n", temp_path);
		free(pbuff);
		exit(-7);
	}

	if (node_stat.st_size != read(fd, pbuff, node_stat.st_size)) {
		printf("fail to read mail from file into memory\n");
		free(pbuff);
		close(fd);
		exit(-8);
	}

	close(fd);

	ppool = mime_pool_init(1024, 32, FALSE);

	if (NULL == ppool) {
		free(pbuff);
		printf("fail to init mime pool\n");
		exit(-9);
	}
	
	mail_init(&imail, ppool);

	if (FALSE == mail_retrieve(&imail, pbuff, node_stat.st_size)) {
		free(pbuff);
		mime_pool_free(ppool);
		printf("fail to retrieve file into mail object\n");
		exit(-10);
	}

	pmime_head = mail_get_head(&imail);

	if (NULL == pmime_head) {
		mail_free(&imail);
		free(pbuff);
		mime_pool_free(ppool);
		printf("fail to get head from mail object\n");
		exit(-11);
	}

	
	single_list_init(&temp_list);

	if (TRUE == mime_get_field(pmime_head, "To", temp_field, 64*1024)) {
		parse_rcpt(temp_field, &temp_list);
	}

	if (TRUE == mime_get_field(pmime_head, "Cc", temp_field, 64*1024)) {
		parse_rcpt(temp_field, &temp_list);
	}

	if (TRUE == mime_get_field(pmime_head, "Bcc", temp_field, 64*1024)) {
		parse_rcpt(temp_field, &temp_list);
	}

	if (0 == single_list_get_nodes_num(&temp_list)) {
		mail_free(&imail);
		free(pbuff);
		mime_pool_free(ppool);
		printf("fail to get rcpt address from mail object\n");
		exit(-12);
	}

	
	b_connected = FALSE;
	/* try to connect to the destination MTA */
	sockd = socket(AF_INET, SOCK_STREAM, 0);
	opt = fcntl(sockd, F_GETFL, 0);
	opt |= O_NONBLOCK;
	fcntl(sockd, F_SETFL, opt);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(smtp_port);
	inet_pton(AF_INET, smtp_ip, &servaddr.sin_addr);
	if (0 == connect(sockd, (struct sockaddr*)&servaddr, sizeof(servaddr))) {
		b_connected = TRUE;
		/* set socket back to block mode */
		opt = fcntl(sockd, F_GETFL, 0);
		opt &= (~O_NONBLOCK);
		fcntl(sockd, F_SETFL, opt);
		/* end of set mode */
	} else {
		if (EINPROGRESS == errno) {
			tv.tv_sec = SOCKET_TIMEOUT;
			tv.tv_usec = 0;
			FD_ZERO(&myset);
			FD_SET(sockd, &myset);
			if (select(sockd + 1, NULL, &myset, NULL, &tv) > 0) {
				opt_len = sizeof(int);
				if (getsockopt(sockd, SOL_SOCKET, SO_ERROR, &val_opt,
					&opt_len) >= 0) {
					if (0 == val_opt) {
						b_connected = TRUE;
						/* set socket back to block mode */
						opt = fcntl(sockd, F_GETFL, 0);
						opt &= (~O_NONBLOCK);
						fcntl(sockd, F_SETFL, opt);
						/* end of set mode */
					}
				}
			}
		}
	}
	if (FALSE == b_connected) {
		goto EXIT_PROGRAM;
	}
	/* read welcome information of MTA */
	res_val = get_response(sockd, last_response, 1024, FALSE);
	switch (res_val) {
	case SMTP_TIME_OUT:
		goto EXIT_PROGRAM;
	case SMTP_PERMANENT_ERROR:
	case SMTP_TEMP_ERROR:
	case SMTP_UNKOWN_RESPONSE:
        /* send quit command to server */
        send_command(sockd, "quit\r\n", 6);
		goto EXIT_PROGRAM;
	}

	/* send helo xxx to server */
	strcpy(last_command, "helo mail.sender\r\n");
	command_len = strlen(last_command);
	if (FALSE == send_command(sockd, last_command, command_len)) {
		goto EXIT_PROGRAM;
	}
	res_val = get_response(sockd, last_response, 1024, FALSE);
	switch (res_val) {
	case SMTP_TIME_OUT:
		goto EXIT_PROGRAM;
	case SMTP_PERMANENT_ERROR:
	case SMTP_TEMP_ERROR:
	case SMTP_UNKOWN_RESPONSE:
		/* send quit command to server */
		send_command(sockd, "quit\r\n", 6);
		goto EXIT_PROGRAM;
	}

	command_len = sprintf(last_command, "mail from:<%s>\r\n", argv[1]);
	
	if (FALSE == send_command(sockd, last_command, command_len)) {
		goto EXIT_PROGRAM;
	}
	/* read mail from response information */
	res_val = get_response(sockd, last_response, 1024, FALSE);
	switch (res_val) {
	case SMTP_TIME_OUT:
		goto EXIT_PROGRAM;
	case SMTP_PERMANENT_ERROR:
	case SMTP_TEMP_ERROR:
	case SMTP_UNKOWN_RESPONSE:
		/* send quit command to server */
		send_command(sockd, "quit\r\n", 6);
		goto EXIT_PROGRAM;
	}

	while (pnode = single_list_get_from_head(&temp_list)) {
		pitem = (RCPT_ITEM*)pnode->pdata;
		command_len = sprintf(last_command, "rcpt to:<%s>\r\n", pitem->address);
		if (FALSE == send_command(sockd, last_command, command_len)) {
			goto EXIT_PROGRAM;
		}
		/* read rcpt to response information */
		res_val = get_response(sockd, last_response, 1024, FALSE);
		switch (res_val) {
		case SMTP_TIME_OUT:
			goto EXIT_PROGRAM;
		case SMTP_PERMANENT_ERROR:
		case SMTP_TEMP_ERROR:
		case SMTP_UNKOWN_RESPONSE:
			send_command(sockd, "quit\r\n", 6);
			goto EXIT_PROGRAM;
		}						
	}
	
	/* send data */
	strcpy(last_command, "data\r\n");
	command_len = strlen(last_command);
	if (FALSE == send_command(sockd, last_command, command_len)) {
		goto EXIT_PROGRAM;
	}

	/* read data response information */
	res_val = get_response(sockd, last_response, 1024, TRUE);
	switch (res_val) {
	case SMTP_TIME_OUT:
		goto EXIT_PROGRAM;
	case SMTP_PERMANENT_ERROR:
	case SMTP_TEMP_ERROR:
	case SMTP_UNKOWN_RESPONSE:
		send_command(sockd, "quit\r\n", 6);
		goto EXIT_PROGRAM;
	}

	if (FALSE == send_command(sockd, pbuff, node_stat.st_size)) {
		goto EXIT_PROGRAM;
	}

	strcpy(last_command, "\r\n.\r\n");
	command_len = strlen(last_command);
	if (FALSE == send_command(sockd, last_command, command_len)) {
		goto EXIT_PROGRAM;
	}
	res_val = get_response(sockd, last_response, 1024, FALSE);
	switch (res_val) {
	case SMTP_TIME_OUT:
		goto EXIT_PROGRAM;
	case SMTP_PERMANENT_ERROR:
	case SMTP_TEMP_ERROR:
	case SMTP_UNKOWN_RESPONSE:	
		goto EXIT_PROGRAM;
	case SMTP_SEND_OK:
		send_command(sockd, "quit\r\n", 6);
		close(sockd);
		mail_free(&imail);
		free(pbuff);
		mime_pool_free(ppool);
		if (TRUE == move_to_sent(argv[2], argv[3], argv[4])) {
			exit(0);
		} else {
			exit(-13);
		}
	}

EXIT_PROGRAM:
	close(sockd);
	mail_free(&imail);
	free(pbuff);
	mime_pool_free(ppool);
	exit(-14);
}

static BOOL send_command(int sockd, const char *command, int command_len)
{
	int write_len;

	write_len = write(sockd, command, command_len);
    if (write_len != command_len) {
		return FALSE;
	}
	return TRUE;
}


static int get_response(int sockd, char *response, int response_len,
	BOOL expect_3xx)
{
	int read_len;

	memset(response, 0, response_len);
	read_len = read(sockd, response, response_len);
	if (-1 == read_len || 0 == read_len) {
		return SMTP_TIME_OUT;
	}
	if ('\n' == response[read_len - 1] && '\r' == response[read_len - 2]){
		/* remove /r/n at the end of response */
		read_len -= 2;
	}
	response[read_len] = '\0';
	if (FALSE == expect_3xx && '2' == response[0] &&
		0 != isdigit(response[1]) && 0 != isdigit(response[2])) {
		return SMTP_SEND_OK;
	} else if(TRUE == expect_3xx && '3' == response[0] &&
		0 != isdigit(response[1]) && 0 != isdigit(response[2])) {
		return SMTP_SEND_OK;
	} else {
		if ('4' == response[0]) {
           	return SMTP_TEMP_ERROR;	
		} else if ('5' == response[0]) {
			return SMTP_PERMANENT_ERROR;
		} else {
			return SMTP_UNKOWN_RESPONSE;
		}
	}
}

static void parse_rcpt(char *field, SINGLE_LIST *plist)
{
	int i, len;
	char *ptoken;
	char *ptoken_prev;
	char temp_address[1024];
	EMAIL_ADDR email_addr;
	RCPT_ITEM *pitem;


	len = strlen(field);
	field[len] = ';';
	len ++;

	ptoken_prev = field;
	for (i=0; i<len; i++) {
		if (',' == field[i] || ';' == field[i]) {
			ptoken = field + i;
			if (ptoken - ptoken_prev >= 1024) {
				ptoken_prev = ptoken + 1;
				continue;
			}
			memcpy(temp_address, ptoken_prev, ptoken - ptoken_prev);
			temp_address[ptoken - ptoken_prev] = '\0';
			parse_email_addr(&email_addr, temp_address);
			if (0 != strlen(email_addr.local_part) &&
				0 != strlen(email_addr.domain)) {
				pitem = (RCPT_ITEM*)malloc(sizeof(RCPT_ITEM));
				pitem->node.pdata = pitem;
				sprintf(pitem->address, "%s@%s", email_addr.local_part,
					email_addr.domain);
				single_list_append_as_tail(plist, &pitem->node);
			}
			ptoken_prev = ptoken + 1;
		}
	}
}
