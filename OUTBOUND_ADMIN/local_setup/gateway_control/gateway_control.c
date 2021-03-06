#include "gateway_control.h"
#include "single_list.h"
#include "util.h"
#include "list_file.h"
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#define RELOAD_DOMAINS_COMMAND	"domain_list.svc reload\r\n"
#define RELOAD_IPS_COMMAND		"dns_adaptor.svc reload inbound-ips\r\n"
#define ENABLE_DOMAINS_COMMAND	"system set domain-list TRUE\r\n"
#define DISABLE_DOMAINS_COMMAND	"system set domain-list FALSE\r\n"

typedef struct _CONSOLE_PORT {
	SINGLE_LIST_NODE node;
	char smtp_ip[16];
	int smtp_port;
	char delivery_ip[16];
	int delivery_port;
} CONSOLE_PORT;

static char g_list_path[256];
static SINGLE_LIST g_console_list;

static BOOL gateway_control_send(const char *ip, int port, const char *command);

void gateway_control_init(const char *path)
{
	if (NULL != path) {
		strcpy(g_list_path, path);
	} else {
		g_list_path[0] = '\0';
	}
	single_list_init(&g_console_list);
}

int gateway_control_run()
{
	LIST_FILE *plist_file;
	char *pitem;
	int i, list_len;
	SINGLE_LIST_NODE *pnode;
	CONSOLE_PORT *pport;
	
	plist_file = list_file_init(g_list_path, "%s:16%d%s:16%d");
	if (NULL == plist_file) {
		printf("[gateway_control]: fail to open console list file, will not" 
			"notify server to reload list\n");
		return 0;
	}
	
	pitem = (char*)list_file_get_list(plist_file);
	list_len = list_file_get_item_num(plist_file);
	for (i=0; i<list_len; i++) {
		pport = (CONSOLE_PORT*)malloc(sizeof(CONSOLE_PORT));
		if (NULL== pport) {
			continue;
		}
		pport->node.pdata = pport;
		strcpy(pport->smtp_ip, pitem);
		pport->smtp_port = *(int*)(pitem + 16);
		strcpy(pport->delivery_ip, pitem + 16 + sizeof(int));
		pport->delivery_port = *(int*)(pitem + 32 + sizeof(int));
		pitem += 32 + 2*sizeof(int);
		single_list_append_as_tail(&g_console_list, &pport->node);
	}
	list_file_free(plist_file);
	return 0;
}

void gateway_control_enable_domains(BOOL b_enable)
{
	SINGLE_LIST_NODE *pnode;
	CONSOLE_PORT *pconsole;
	
	for (pnode=single_list_get_head(&g_console_list); NULL!=pnode;
		pnode=single_list_get_after(&g_console_list, pnode)) {
		pconsole = (CONSOLE_PORT*)pnode->pdata;
		if (TRUE== b_enable) {
			gateway_control_send(pconsole->smtp_ip, pconsole->smtp_port,
				ENABLE_DOMAINS_COMMAND);
			gateway_control_send(pconsole->delivery_ip, pconsole->delivery_port,
				ENABLE_DOMAINS_COMMAND);
		} else {
			gateway_control_send(pconsole->smtp_ip, pconsole->smtp_port,
				DISABLE_DOMAINS_COMMAND);
			gateway_control_send(pconsole->delivery_ip, pconsole->delivery_port,
				DISABLE_DOMAINS_COMMAND);
		}
	}
}

void gateway_control_reload_domains()
{
	SINGLE_LIST_NODE *pnode;
	CONSOLE_PORT *pconsole;
	
	for (pnode=single_list_get_head(&g_console_list); NULL!=pnode;
		pnode=single_list_get_after(&g_console_list, pnode)) {
		pconsole = (CONSOLE_PORT*)pnode->pdata;
		gateway_control_send(pconsole->smtp_ip, pconsole->smtp_port,
			RELOAD_DOMAINS_COMMAND);
		gateway_control_send(pconsole->delivery_ip, pconsole->delivery_port,
			RELOAD_DOMAINS_COMMAND);
	}
}

void gateway_control_reload_ips()
{
	SINGLE_LIST_NODE *pnode;
	CONSOLE_PORT *pconsole;
	
	for (pnode=single_list_get_head(&g_console_list); NULL!=pnode;
		pnode=single_list_get_after(&g_console_list, pnode)) {
		pconsole = (CONSOLE_PORT*)pnode->pdata;
		gateway_control_send(pconsole->delivery_ip, pconsole->delivery_port,
			RELOAD_IPS_COMMAND);
	}
}

int gateway_control_stop()
{
	SINGLE_LIST_NODE *pnode;

	while (pnode=single_list_get_from_head(&g_console_list)) {
		free(pnode->pdata);
	}
	return 0;
}

void gateway_control_free()
{
	single_list_free(&g_console_list);

}

static BOOL gateway_control_send(const char *ip, int port, const char *command)
{
	int sockd, cmd_len;
	int read_len, offset;
	struct sockaddr_in servaddr;
	char temp_buff[1024];

	sockd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &servaddr.sin_addr);
	if (0 != connect(sockd, (struct sockaddr*)&servaddr, sizeof(servaddr))) {
		close(sockd);
		return FALSE;
	}
	offset = 0;
	memset(temp_buff, 0, 1024);
	/* read welcome information */
	do {
		read_len = read(sockd, temp_buff + offset, 1024 - offset);
		if (-1 == read_len || 0 == read_len) {
			close(sockd);
			return FALSE;
		}
		offset += read_len;
		if (NULL != search_string(temp_buff, "console> ", offset)) {
			break;
		}
	} while (offset < 1024);
	if (offset >= 1024) {
		close(sockd);
		return FALSE;
	}

	/* send command */
	write(sockd, command, strlen(command));
	read(sockd, temp_buff, 1024);
	write(sockd, "quit\r\n", 6);
	close(sockd);
	return TRUE;
}

