#include "system_log.h"
#include "acl_control.h"
#include "password_ui.h"
#include "config_file.h"
#include "util.h"
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>


int main(int argc, char **argv)
{
	char *str_value;
	char work_path[256];
	char temp_path[256];
	char data_path[256];
	char token_path[256];
	char acl_path[256];
	char lang_path[256];
	int timeout;
	CONFIG_FILE *pconfig;

	umask(0);
	if (NULL == getcwd(work_path, 256)) {
		exit(-1);
	}
	sprintf(temp_path, "%s/../config/athena.cfg", work_path);
	pconfig = config_file_init(temp_path);
	if (NULL == pconfig) {
		exit(-1);
	}
	str_value = config_file_get_value(pconfig, "DATA_FILE_PATH");
	if (NULL == str_value) {
		strcpy(data_path, "../data");
	} else {
		strcpy(data_path, str_value);
	}
	str_value = config_file_get_value(pconfig, "LOG_FILE_PATH");
	if (NULL == str_value) {
		str_value = "../logs/athena_log.txt";
	}
	sprintf(temp_path, "%s/%s", work_path, str_value);
	system_log_init(temp_path);
	str_value = config_file_get_value(pconfig, "TOKEN_FILE_PATH");
	if (NULL == str_value) {
		strcpy(token_path, "../token");
	} else {
		strcpy(token_path, str_value);
	}
	sprintf(temp_path, "%s/%s/session.shm", work_path, token_path);
	sprintf(acl_path, "%s/%s/system_users.txt", work_path, data_path);
	str_value = config_file_get_value(pconfig, "UI_TIMEOUT");
	if (NULL == str_value) {
		timeout = 600;
	} else {
		timeout = atoitvl(str_value);
		if (timeout <= 0) {
			timeout = 600;
		}
	}
	acl_control_init(temp_path, acl_path, timeout);
	str_value = config_file_get_value(pconfig, "LOGO_LINK");
	if (NULL == str_value) {
		str_value = "http://www.gridware.com.cn";
	}
	sprintf(lang_path, "%s/%s/system_password", work_path, data_path);
	password_ui_init(acl_path, str_value, lang_path);
	str_value = config_file_get_value(pconfig, "HTTP_ACCEPT_LANGUAGE");
	if (NULL != str_value && '\0' != str_value) {
		setenv("HTTP_ACCEPT_LANGUAGE", str_value, 1);
	}
	config_file_free(pconfig);
	if (0 != system_log_run()) {
		exit(-2);
	}
	if (0 != acl_control_run()) {
		exit(-3);
	}
	if (0 != password_ui_run()) {
		exit(-4);
	}
	password_ui_stop();
	password_ui_free();
	acl_control_stop();
	acl_control_free();
	system_log_stop();
	system_log_free();
	exit(0);
}

