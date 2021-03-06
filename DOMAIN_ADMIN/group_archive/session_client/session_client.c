#include "session_client.h"
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <stdio.h>

#define SOCKET_TIMEOUT				30

static char g_session_ip[16];
static int g_session_port;

static BOOL session_client_readline_timeout(int sockd, char *buff, int length);

void session_client_init(const char *ip, int port)
{
	strcpy(g_session_ip, ip);
	g_session_port = port;
}

int session_client_run()
{
	/* do nothing */
	return 0;
}

int session_client_stop()
{
	/* do nothing */
	return 0;
}

BOOL session_client_check(const char *domainname, const char *session)
{
	int sockd, len;
	char temp_buff[1024];
	struct sockaddr_in servaddr;
	
	
	sockd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(g_session_port);
	inet_pton(AF_INET, g_session_ip, &servaddr.sin_addr);
	if (0 != connect(sockd, (struct sockaddr*)&servaddr,sizeof(servaddr))) {
		close(sockd);
		return FALSE;
	}
	fcntl(sockd, F_SETFL, O_NONBLOCK);

	if (FALSE == session_client_readline_timeout(sockd, temp_buff, 1024)) {
		close(sockd);
		return FALSE;
	}
	if (0 != strcasecmp(temp_buff, "OK")) {
		close(sockd);
		return FALSE;
	}
	len = snprintf(temp_buff, 1024, "CHECK %s %s\r\n", domainname, session);
	write(sockd, temp_buff, len);
	
	if (FALSE == session_client_readline_timeout(sockd, temp_buff, 1024) ||
		0 != strcasecmp(temp_buff, "TRUE")) {
		close(sockd);
		return FALSE;
	}
	write(sockd, "QUIT\r\n", 6);
	close(sockd);
	return TRUE;
}

BOOL session_client_produce(const char *domainname, char *session)
{
	int sockd, len;
	char temp_buff[1024];
	struct sockaddr_in servaddr;
	
	sockd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(g_session_port);
	inet_pton(AF_INET, g_session_ip, &servaddr.sin_addr);
	if (0 != connect(sockd, (struct sockaddr*)&servaddr,sizeof(servaddr))) {
		close(sockd);
		return FALSE;
	}
	fcntl(sockd, F_SETFL, O_NONBLOCK);

	if (FALSE == session_client_readline_timeout(sockd, temp_buff, 1024)) {
		close(sockd);
		return FALSE;
	}
	if (0 != strcasecmp(temp_buff, "OK")) {
		close(sockd);
		return FALSE;
	}
	len = snprintf(temp_buff, 1024, "ALLOC %s\r\n", domainname);
	write(sockd, temp_buff, len);
	
	if (FALSE == session_client_readline_timeout(sockd, temp_buff, 1024) ||
		0 != strncasecmp(temp_buff, "TRUE ", 5)) {
		write(sockd, "QUIT\r\n", 6);
		close(sockd);
		return FALSE;
	}
	write(sockd, "QUIT\r\n", 6);
	close(sockd);
	memcpy(session, temp_buff + 5, 32);
	session[32] = '\0';
	return TRUE;
}

BOOL session_client_remove(const char *domainname, const char *session)
{
	int sockd, len;
	char temp_buff[1024];
	struct sockaddr_in servaddr;
	
	sockd = socket(AF_INET, SOCK_STREAM, 0);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(g_session_port);
	inet_pton(AF_INET, g_session_ip, &servaddr.sin_addr);
	if (0 != connect(sockd, (struct sockaddr*)&servaddr,sizeof(servaddr))) {
		close(sockd);
		return FALSE;
	}
	fcntl(sockd, F_SETFL, O_NONBLOCK);

	if (FALSE == session_client_readline_timeout(sockd, temp_buff, 1024)) {
		close(sockd);
		return FALSE;
	}
	if (0 != strcasecmp(temp_buff, "OK")) {
		close(sockd);
		return FALSE;
	}
	len = snprintf(temp_buff, 1024, "FREE %s %s\r\nQUIT\r\n", domainname,
			session);
	write(sockd, temp_buff, len);
	close(sockd);
	return TRUE;
}

void session_client_free()
{
	/* do nothing */

}


static BOOL session_client_readline_timeout(int sockd, char *buff, int length)
{
    int offset;
    int temp_len;
    int i, read_len;
    time_t first_time;
    time_t last_time;
    char temp_line[1024];

    offset = 0;
    time(&first_time);
    while (TRUE) {
        read_len = read(sockd, temp_line, 1024 - offset);
        if (-1 == read_len) {
            read_len = 0;
        }
        offset += read_len;
        for (i=0; i<offset; i++) {
            if ('\r' == temp_line[i] && '\n' == temp_line[i + 1]) {
                temp_len = (i < length - 1) ? i : length - 1;
                memcpy(buff, temp_line, temp_len);
                buff[temp_len] = '\0';
                return TRUE;
            }
        }
        time(&last_time);
        if (last_time - first_time > SOCKET_TIMEOUT || 1024 == offset) {
            return FALSE;
        }
        usleep(10000);
    }
}

