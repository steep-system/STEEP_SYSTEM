#include "message.h"
#include <time.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#define	MESSAGE_BODY_1	\
"Received: from unknown (helo localhost) (unkown@127.0.0.1)\r\n\
	by herculiz with SMTP\r\n\
From: \"System supervisor\"<supervise-alarm@system.mail>\r\n\
To: "

/* fill mime to */

#define MESSAGE_BODY_2	"\r\nDate: "

/* fill date */

#define MESSAGE_BODY_3	\
"\r\nSubject: Mail system supervisor alarm, faulure on %s:%d!!!\r\n\
Content-Type: multipart/related;\r\n\
	boundary=\"----=_NextPart_000_0005_5F7091A5.ABB0874\"\r\n\r\n\
This is a multi-part message in MIME format.\r\n\r\n\
------=_NextPart_000_0005_5F7091A5.ABB0874\r\n\
Content-Transfer-Encoding: 8bit\r\n\
Content-Type: text/html;\r\n\
	charset=\"us-ascii\"\r\n\r\n\
<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\r\n\
<HTML><HEAD><STYLE TYPE=\"text/css\"><!--\r\n\
BODY {FONT-SIZE: 10pt;FONT-WEIGHT: bold;COLOR: #ff0000;\r\n\
FONT-FAMILY: sans-serif, Verdana, Arial, Helvetica}\r\n\
TD {FONT-SIZE: 8pt; FONT-FAMILY: sans-serif, Verdana, Arial, Helvetica}\r\n\
A:active {COLOR: #3b53b1; TEXT-DECORATION: none}\r\n\
A:link {COLOR: #3b53b1; TEXT-DECORATION: none}\r\n\
A:visited {COLOR: #0000ff; TEXT-DECORATION: none}\r\n\
A:hover {COLOR: #0000ff; TEXT-DECORATION: underline}\r\n\
.AlarmTitle {FONT-WEIGHT: bold; FONT-SIZE: 13pt; COLOR: #ffffff}\r\n\
--></STYLE>\r\n<TITLE> Mail system supervisor alarm!!! </TITLE>\r\n\
<META http-equiv=Content-Type content=\"text/html; charset=us-ascii\">\r\n\
<META content=\"MSHTML 6.00.2900.2912\" name=GENERATOR></HEAD>\r\n\
<BODY bottomMargin=0 leftMargin=0 topMargin=0 rightMargin=0\r\n\
marginheight=\"0\" marginwidth=\"0\">\r\n\
<CENTER><TABLE cellSpacing=0 cellPadding=0 width=\"100%\" border=0><TBODY>\r\n\
<TR><TD noWrap align=middle background=\r\n\
\"cid:001501c695cb$9bc2ea60$6601a8c0@herculiz\" height=55>\r\n\
<SPAN class=AlarmTitle> Mail system supervisor alarm!!! </SPAN>\r\n\
<TD vAlign=bottom noWrap width=\"22%\"\r\n\
background=\"cid:001501c695cb$9bc2ea60$6601a8c0@herculiz\"><A\r\n\
href=\"%s\" target=_blank><IMG height=48\r\n\
src=\"cid:001901c695cb$9bc53450$6601a8c0@herculiz\" width=195 align=right\r\n\
border=0></A></TD></TR></TBODY></TABLE><BR>\r\n\
<TABLE cellSpacing=1 cellPadding=1 width=\"90%\" border=0> <TBODY><TR>\r\n\
<P></P><BR><P></P><BR><P></P><BR><BR>\r\n"

/* content */

#define MESSAGE_BODY_4	\
"</TBODY></TABLE></TD></TR></TBODY></TABLE>\r\n\
<P></P><BR><P></P><BR></CENTER></BODY></HTML>\r\n\
------=_NextPart_000_0005_5F7091A5.ABB0874\r\n\
Content-ID: <001501c695cb$9bc2ea60$6601a8c0@herculiz>\r\n\
Content-Transfer-Encoding: base64\r\n\
Content-Type: image/gif\r\n\r\n"

/* fill image di1.gif */


#define MESSAGE_BODY_5	\
"------=_NextPart_000_0005_5F7091A5.ABB0874\r\n\
Content-ID: <001901c695cb$9bc53450$6601a8c0@herculiz>\r\n\
Content-Transfer-Encoding: base64\r\n\
Content-Type: image/gif\r\n\r\n"

/* fill image logo_bb.gif */

#define MESSAGE_BODY_6	"------=_NextPart_000_0005_5F7091A5.ABB0874--\r\n"

static char g_background_path[256];
static char g_logo_path[256];
static char g_logo_link[256];

void message_init(const char *background_path, const char *logo_path,
	const char *logo_link)
{
	strcpy(g_background_path, background_path);
	strcpy(g_logo_path, logo_path);
	strcpy(g_logo_link, logo_link);
}

int message_run()
{
	/* do nothing */
}

int message_stop()
{
	/* do nothing */
}

void message_free()
{
	/* do nothing */
}

void message_supervising(char *buff, int message_type, int id)
{
	if (MESSAGE_SUPERVISING_SMTP == message_type) {
		sprintf(buff,
				"From: \"System supervisor\"<supervise-alarm@system.mail>\r\n"
				"To: \"Supervise mailbox\"\r\n"
				"X-SMTP-ID: <%d>\r\n"
				"Subject: This is supervising messge, please don't delete me\r\n"
				"\r\nMail system supervising message body!\r\n", id);
	} else if (MESSAGE_SUPERVISING_POP3 == message_type) {
		sprintf(buff,
				"From: \"System supervisor\"<supervise-alarm@system.mail>\r\n"
				"To: \"Supervise mailbox\"\r\n"
				"X-POP3-ID: <%d>\r\n"
				"Subject: This is supervising messge, please don't delete me\r\n"
				"\r\nMail system supervising message body!\r\n", id);
	}
}

void message_alarm_message(char *buff, int type, const char *command,
	const char *response, const char *ip, int port, const char *to)
{
	char *ptr;
	char img_buf[MESSAGE_BUFF_SIZE/2];
	int len, fd;
	time_t cur_time;
	struct tm temp_tm;
	struct stat node_stat;
	
	ptr = buff;
	memcpy(ptr, MESSAGE_BODY_1, sizeof(MESSAGE_BODY_1) - 1);
	ptr += sizeof(MESSAGE_BODY_1) - 1;
	
	len = strlen(to);
	memcpy(ptr, to, len);
	ptr += len;

	memcpy(ptr, MESSAGE_BODY_2, sizeof(MESSAGE_BODY_2) - 1);
	ptr += sizeof(MESSAGE_BODY_2) - 1;

	time(&cur_time);
	localtime_r(&cur_time, &temp_tm);
	len = strftime(ptr, 128, "%a, %d %b %Y %H:%M:%S %z", &temp_tm);
	ptr += len;

	len = sprintf(ptr, MESSAGE_BODY_3, ip, port, g_logo_link);
	ptr += len;
	
	switch (type) {
	case MESSAGE_SMTP_CANNOT_CONNECT:
		len = sprintf(ptr, "SMTP service on %s:%d doesn't work, connection "
				"cannot be established with this server", ip, port);
		ptr += len;
		break;
	case MESSAGE_SMTP_CONNECT_ERROR:
		len = sprintf(ptr, "connection request is refused by SMTP service on "
				"%s:%d, response is \"%s\"", ip, port, response);
		ptr += len;
		break;
	case MESSAGE_SMTP_TIME_OUT:
		len = sprintf(ptr, "SMTP transaction time out with SMTP "
				"service on %s:%d", ip, port);
		ptr += len;
		break;
	case MESSAGE_SMTP_TEMP_ERROR:
	case MESSAGE_SMTP_UNKOWN_RESPONSE:
	case MESSAGE_SMTP_PERMANENT_ERROR:
		if (0 == strcmp(command, "\r\n.\r\n")) {
			len = sprintf(ptr, "SMTP service on %s:%d responds %s after "
					"command <crlf>.<crlf>", ip, port, response);
		} else {
			len = sprintf(ptr, "SMTP service on %s:%d responds %s after "
					"command %s", ip, port, response, command);
		}
		ptr += len;
		break;
	case MESSAGE_SMTP_AUTH_FAIL:
		len = sprintf(ptr, "password authentification can not be passed with "
				"SMTP service on %s:%d", ip, port);
		ptr += len;
		break;
	case MESSAGE_ALARM_QUEUE:
		len = sprintf(ptr, "message cannot be delivered to user's mail box "
				"with SMTP service on %s:%d, jam may occur in message queue",
				ip, port);
		ptr += len;
		break;
	case MESSAGE_POP3_CANNOT_CONNECT:
		len = sprintf(ptr, "POP3 service on %s:%d doesn't work, connection "
				"cannot be established with this server", ip, port);
		ptr += len;
		break;
	case MESSAGE_POP3_CONNECT_ERROR:
		len = sprintf(ptr, "connection request is refused by POP3 service on "
				"%s:%d, response is \"%s\"", ip, port, response);
		ptr += len;
		break;
	case MESSAGE_POP3_TIME_OUT:
		len = sprintf(ptr, "POP3 transaction time out with POP3 "
				"service on %s:%d", ip, port);
		ptr += len;
		break;
	case MESSAGE_POP3_RESPONSE_ERROR:
		len = sprintf("POP3 service on %s:%d responds %s after command %s",
				ip, port, command, response);
		ptr += len;
		break;
	case MESSAGE_POP3_UPDATE_FAIL:
		len = sprintf(ptr, "POP3 service on %s:%d fail to update state of "
				"message deleting after \"quit\" command, response is \"%s\"",
				ip, port, response);
		ptr += len;
		break;
	case MESSAGE_POP3_AUTH_FAIL:
		len = sprintf(ptr, "password authentification can not be passed with "
				"POP3 service on %s:%d", ip, port);
		ptr += len;
		break;
	case MESSAGE_POP3_RETRIEVE_NONE:
		len = sprintf(ptr, "message cannot be retrieved from POP3 service on "
				"%s:%d, message may be lost or jam occurs in message queue",
				ip, port);
		ptr += len;
		break;
	}

	memcpy(ptr, MESSAGE_BODY_4, sizeof(MESSAGE_BODY_4) - 1);
	ptr += sizeof(MESSAGE_BODY_4) - 1;

	if (0 != stat(g_background_path, &node_stat) ||
		node_stat.st_size > sizeof(img_buf)) {
		goto NEXT_IMAGE;
	}
	fd = open(g_background_path, O_RDONLY);
	if (-1 == fd) {
		goto NEXT_IMAGE;
	}
	if (node_stat.st_size != read(fd, img_buf, sizeof(img_buf))) {
		goto NEXT_IMAGE;
	}
	close(fd);
	if (0 != encode64_ex(img_buf, node_stat.st_size, ptr, MESSAGE_BUFF_SIZE,
		&len)) {
		goto NEXT_IMAGE;
	}
	ptr += len;
NEXT_IMAGE:
	memcpy(ptr, MESSAGE_BODY_5, sizeof(MESSAGE_BODY_5) - 1);
	ptr += sizeof(MESSAGE_BODY_5) - 1;

	if (0 != stat(g_logo_path, &node_stat) ||
		node_stat.st_size > sizeof(img_buf)) {
		goto FINAL_MESSAGE;
	}
	fd = open(g_logo_path, O_RDONLY);
	if (-1 == fd) {
		goto FINAL_MESSAGE;
	}
	if (node_stat.st_size != read(fd, img_buf, sizeof(img_buf))) {
		goto FINAL_MESSAGE;
	}
	close(fd);
	if (0 != encode64_ex(img_buf, node_stat.st_size, ptr, MESSAGE_BUFF_SIZE,
		&len)) {
		goto FINAL_MESSAGE;
	}
	ptr += len;
FINAL_MESSAGE:
	memcpy(ptr, MESSAGE_BODY_6, sizeof(MESSAGE_BODY_6));
}

