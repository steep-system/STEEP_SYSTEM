#include "smtp.h"
#include "mail_func.h"
#include "util.h"
#undef NOERROR                  /* in <sys/streams.h> on solaris 2.x */
#include <arpa/nameser.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <netdb.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#define MAXPACKET			8192 /* max size of packet */
#define MAXBUF				256          
#define MAXMXHOSTS			32   /* max num of mx records we want to see */
#define MAXMXBUFSIZ			(MAXMXHOSTS * (MAXBUF+1)) 
#define SOCKET_TIMEOUT		180
#define RETRYING_INTERVAL	180

enum {
	SMTP_SEND_OK = 0,
	SMTP_CANNOT_CONNECT,
	SMTP_CONNECT_ERROR,
	SMTP_TIME_OUT,
	SMTP_TEMP_ERROR,
	SMTP_UNKOWN_RESPONSE,
	SMTP_PERMANENT_ERROR
};


void smtp_init()
{
	/* do nothing */
}

int smtp_run()
{
	return 0;
	/* do nothing */
}

int smtp_stop()
{
	return 0;
	/* do nothing */
}

void smtp_free()
{
	/* do nothing */
}


static int smtp_getmxbyname(char* mx_name, char **MxHosts); 

static BOOL smtp_send_command(int sockd, const char *command,
	int command_len);

static int smtp_get_response(int sockd, char *response, 
	int response_len, BOOL expect_3xx);

void smtp_send_message(const char *from, const char *rcpt, const char *message)
{
	BOOL b_connected;
	char **p_addr;
	char *pdomain, ip[16];
	char command_line[1024];
	char response_line[1024];
	int size, i, times, num;
	int command_len, sockd, opt;
	int port, val_opt, opt_len;
	struct sockaddr_in servaddr;
	struct in_addr ip_addr;
	char* mx_buff[MAXMXHOSTS];
	struct hostent *phost;
	struct timeval tv;
	fd_set myset;
	
	b_connected = FALSE;
	pdomain = strchr(rcpt, '@');
	if (NULL == pdomain) {
		return;
	}
	pdomain ++;
	size = strlen(message);
	num = smtp_getmxbyname(pdomain, mx_buff);
	if (num <= 0 || num > MAXMXHOSTS) {
		return;
	}
	memset(ip, 0, 16);
	for (i=0; i<num; i++) {
		if (NULL == extract_ip(mx_buff[i], ip)) {
			if (NULL == (phost = gethostbyname(mx_buff[i]))) {
				continue;
			}
			p_addr = phost->h_addr_list;
			for (; NULL != (*p_addr); p_addr++) {
				ip_addr.s_addr = *((unsigned int *)*p_addr);
				strcpy(ip, inet_ntoa(ip_addr));
				break;
			}
			if ('\0' != ip[0]) {
				break;
			}
		} else {
			break;
		}
	}
	if ('\0' == ip[0]) {
		return;
	}
	port = 25;
	times = 0;
SENDING_RETRY:
	if (0 != times) {
		sleep(RETRYING_INTERVAL);
	}
	/* try to connect to the destination MTA */
	sockd = socket(AF_INET, SOCK_STREAM, 0);
	opt = fcntl(sockd, F_GETFL, 0);
	opt |= O_NONBLOCK;
	fcntl(sockd, F_SETFL, opt);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &servaddr.sin_addr);
	if (0 == connect(sockd, (struct sockaddr*)&servaddr,sizeof(servaddr))) {
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
		close(sockd);
		times ++;
		if (3 == times) {
			return;
		} else {
			goto SENDING_RETRY;
		}
	}
	/* read welcome information of MTA */
	switch (smtp_get_response(sockd, response_line, 1024, FALSE)) {
	case SMTP_TIME_OUT:
		close(sockd);
		times ++;
		if (3 == times) {
			return;
		} else {
			goto SENDING_RETRY;
		}
	case SMTP_PERMANENT_ERROR:
	case SMTP_TEMP_ERROR:
	case SMTP_UNKOWN_RESPONSE:
		strcpy(command_line, "quit\r\n");
        /* send quit command to server */
        smtp_send_command(sockd, command_line, 6);
		close(sockd);
		return;
	}

	/* send helo xxx to server */
	if (FALSE == smtp_send_command(sockd, "helo system.mail\r\n", 18)) {
		close(sockd);
		times ++;
		if (3 == times) {
			return;
		} else {
			goto SENDING_RETRY;
		}
	}
	switch (smtp_get_response(sockd, response_line, 1024, FALSE)) {
	case SMTP_TIME_OUT:
	case SMTP_TEMP_ERROR:
		close(sockd);
		times ++;
		if (3 == times) {
			return;
		} else {
			goto SENDING_RETRY;
		}
	case SMTP_PERMANENT_ERROR:
	case SMTP_UNKOWN_RESPONSE:
		strcpy(command_line, "quit\r\n");
		/* send quit command to server */
		smtp_send_command(sockd, command_line, 6);
		close(sockd);
		return;
	}

	/* send mail from:<...> */
	command_len = sprintf(command_line, "mail from:<%s>\r\n", from);
	if (FALSE == smtp_send_command(sockd, command_line, command_len)) {
		close(sockd);
		times ++;
		if (3 == times) {
			return;
		} else {
			goto SENDING_RETRY;
		}
	}
	/* read mail from response information */
    switch (smtp_get_response(sockd, response_line, 1024, FALSE)) {
    case SMTP_TIME_OUT:
	case SMTP_TEMP_ERROR:
		close(sockd);
		times ++;
		if (3 == times) {
			return;
		} else {
			goto SENDING_RETRY;
		}
	case SMTP_PERMANENT_ERROR:
	case SMTP_UNKOWN_RESPONSE:
		strcpy(command_line, "quit\r\n");
        /* send quit command to server */
        smtp_send_command(sockd, command_line, 6);
		close(sockd);
        return;
    }

	/* send rcpt to:<...> */
	
	command_len = sprintf(command_line, "rcpt to:<%s>\r\n", rcpt);
	if (FALSE == smtp_send_command(sockd, command_line, command_len)) {
		close(sockd);
		times ++;
		if (3 == times) {
			return;
		} else {
			goto SENDING_RETRY;
		}
	}
	/* read rcpt to response information */
    switch (smtp_get_response(sockd, response_line, 1024, FALSE)) {
    case SMTP_TIME_OUT:
	case SMTP_TEMP_ERROR:
		close(sockd);
		times ++;
		if (3 == times) {
			return;
		} else {
			goto SENDING_RETRY;
		}
	case SMTP_PERMANENT_ERROR:
	case SMTP_UNKOWN_RESPONSE:
		strcpy(command_line, "quit\r\n");
		/* send quit command to server */
		smtp_send_command(sockd, command_line, 6);
		close(sockd);
		return;
	}
	
	/* send data */
	strcpy(command_line, "data\r\n");
	if (FALSE == smtp_send_command(sockd, command_line, 6)) {
		close(sockd);
		times ++;
		if (3 == times) {
			return;
		} else {
			goto SENDING_RETRY;
		}
	}

	/* read data response information */
    switch (smtp_get_response(sockd, response_line, 1024, TRUE)) {
    case SMTP_TIME_OUT:
	case SMTP_TEMP_ERROR:
		close(sockd);
		times ++;
		if (3 == times) {
			return;
		} else {
			goto SENDING_RETRY;
		}
	case SMTP_PERMANENT_ERROR:
	case SMTP_UNKOWN_RESPONSE:
		strcpy(command_line, "quit\r\n");
        /* send quit command to server */
        smtp_send_command(sockd, command_line, 6);
		close(sockd);
		return;
    }

	if (FALSE == smtp_send_command(sockd, message, size)) {
		close(sockd);
		times ++;
		if (3 == times) {
			return;
		} else {
			goto SENDING_RETRY;
		}
	}
	if (FALSE == smtp_send_command(sockd, "\r\n.\r\n", 5)) {
		close(sockd);
		times ++;
		if (3 == times) {
			return;
		} else {
			goto SENDING_RETRY;
		}
	}
	switch (smtp_get_response(sockd, response_line, 1024, FALSE)) {
	case SMTP_TIME_OUT:
	case SMTP_TEMP_ERROR:
		close(sockd);
		times ++;
		if (3 == times) {
			return;
		} else {
			goto SENDING_RETRY;
		}
	case SMTP_PERMANENT_ERROR:
	case SMTP_UNKOWN_RESPONSE:	
		strcpy(command_line, "quit\r\n");
		/* send quit command to server */
        smtp_send_command(sockd, command_line, 6);
		close(sockd);
		return;
	case SMTP_SEND_OK:
		strcpy(command_line, "quit\r\n");
		/* send quit command to server */
		smtp_send_command(sockd, command_line, 6);
		close(sockd);
		return;
	}
}


/*
 *	send a command string to destination
 *	@param
 *		sockd				socket fd
 *		command [in]		command string to be sent
 *		command_len			command string length
 *	@return
 *		TRUE				OK
 *		FALSE				time out
 */
static BOOL smtp_send_command(int sockd, const char *command, int command_len)
{
	int write_len;

	write_len = write(sockd, command, command_len);
    if (write_len != command_len) {
		return FALSE;
	}
	return TRUE;
}

/*
 *	get response from server
 *	@param
 *		sockd					socket fd
 *		response [out]			buffer for save response
 *		response_len			response buffer length
 *		reason [out]			fail reason
 *	@retrun
 *		SMTP_TIME_OUT			time out
 *		SMTP_TEMP_ERROR		temp fail
 *		SMTP_UNKOWN_RESPONSE	unkown fail
 *		SMTP_PERMANENT_ERROR	permanent fail
 *		SMTP_SEND_OK		OK
 */
static int smtp_get_response(int sockd, char *response, int response_len,
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


 /************************************************************************/
/* BELOW Is the function for getting mx record from UC berkeley.        */
/************************************************************************/

/*
 * Copyright (c) 1983 Eric P. Allman
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */
           

#if defined(BIND_493)
typedef u_char  qbuf_t;
#else
typedef char    qbuf_t;
#endif                

#if defined(BIND_493)
typedef char    nbuf_t;
#else
typedef u_char  nbuf_t;
#endif

/*
 *  Fetch mx hosts for the specified domain and push the host
 *  onto the stack
 *
 *  @param
 *      mx_name [in]    the mx name such as (sina.com, yahoo.com)
 *
 *  @return
 *      number of mx hosts found.
 */

static int smtp_getmxbyname(char* mx_name, char **MxHosts) 
{
typedef union {
    HEADER hdr;
    u_char buf[MAXPACKET];
} querybuf;

	static char hostbuf[MAXMXBUFSIZ];
    querybuf answer;            /* answer buffer from nameserver */
    HEADER *hp;                 /* answer buffer header */
    int ancount, qdcount;       /* answer count and query count */
    u_char *msg, *eom, *cp;     /* answer buffer positions */
    int type, class, dlen;      /* record type, class and length */
    u_short pref;               /* mx preference value */
    u_short prefer[MAXMXHOSTS]; /* saved preferences of mx records */
    char *bp;                   /* hostbuf pointer */
    int nmx;                    /* number of mx hosts found */
    int i, j, n;

    /* query the nameserver to retrieve mx records for the given domain */
    errno   = 0;            /* reset before querying nameserver */
    h_errno = 0;

    n = res_search(mx_name, C_IN, T_MX, (u_char *)&answer, sizeof(answer));
    if (n < 0) {
#ifndef _SOLARIS_
        if (_res.options & RES_DEBUG) {
            debug_info("[dns_adaptor]: res_search failed");
		}
#endif
        return -1;
    }

    errno   = 0;            /* reset after we got an answer */

    if (n < HFIXEDSZ) {
        h_errno = NO_RECOVERY;
        return -2;
    }

    /* avoid problems after truncation in tcp packets */
    if (n > sizeof(answer)) {
        n = sizeof(answer);
	}

    /* valid answer received. skip the query record. */
    hp  = (HEADER *)&answer;
    qdcount = ntohs((u_short)hp->qdcount);
    ancount = ntohs((u_short)hp->ancount);

    msg = (u_char *)&answer;
    eom = (u_char *)&answer + n;
    cp  = (u_char *)&answer + HFIXEDSZ;

    while (qdcount-- > 0 && cp < eom) {
        n = dn_skipname(cp, eom);
        if (n < 0) {
            return -1;
		}
        cp += n;
        cp += QFIXEDSZ;
    }

    /* loop through the answer buffer and extract mx records. */
    nmx = 0;
    bp  = hostbuf;

    while (ancount-- > 0 && cp < eom && nmx < MAXMXHOSTS) {

        n = dn_expand(msg, eom, cp, (nbuf_t *)bp, MAXBUF);
        if (n < 0) {
            break;
		}
        cp += n;

        type = _getshort(cp);
        cp += INT16SZ;

        class = _getshort(cp);
        cp += INT16SZ;

        /* ttl = _getlong(cp); */
        cp += INT32SZ;

        dlen = _getshort(cp);
        cp += INT16SZ;

        if (type != T_MX || class != C_IN) {
            cp += dlen;
            continue;
        }

        pref = _getshort(cp);
        cp  += INT16SZ;

        n = dn_expand(msg, eom, cp, (nbuf_t *)bp, MAXBUF);
        if (n < 0) {
            break;
		}
        cp += n;

        prefer[nmx] = pref;
        MxHosts[nmx] = bp;
        nmx++;

        n = strlen(bp) + 1;
        bp += n;
    }

    /* sort all records by preference. */
    for (i = 0; i < nmx; i++) {
        for (j = i + 1; j < nmx; j++) {
            if (prefer[i] < prefer[j]) {
                register u_short tmppref;
                register char *tmphost;

                tmppref   = prefer[i];
                prefer[i] = prefer[j];
                prefer[j] = tmppref;

                tmphost = MxHosts[i];
                MxHosts[i] = MxHosts[j];
                MxHosts[j] = tmphost;
            }
        }
    }
    return nmx;
}

