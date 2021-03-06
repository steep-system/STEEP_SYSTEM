#include "service_common.h"
#include "ip_filter.h"
#include "config_file.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

DECLARE_API;

BOOL SVC_LibMain(int reason, void **ppdata)
{
	CONFIG_FILE  *pfile;
	char file_name[256], list_path[256];
	char config_path[256], temp_buff[128];
	char *str_value, *psearch;
	char *judge_name, *add_name, *query_name;
	int audit_max, audit_interval, audit_times;
	int temp_list_size, growing_num, fd;
	struct stat node_stat;
	
	switch(reason) {
	case PLUGIN_INIT:
		LINK_API(ppdata);
		/* get the plugin name from system api */
		strcpy(file_name, get_plugin_name());
		psearch = strrchr(file_name, '.');
		if (NULL != psearch) {
			*psearch = '\0';
		}
		if (FALSE == register_talk(ip_filter_console_talk)) {
			printf("[%s]: fail to register console talk\n", file_name);
			return FALSE;
		}
		sprintf(config_path, "%s/%s.cfg", get_config_path(), file_name);
		pfile = config_file_init(config_path);
		if (NULL == pfile) {
			printf("[%s]: error to open config file!!!\n", file_name);
			return FALSE;
		}
		str_value = config_file_get_value(pfile, "AUDIT_MAX_NUM");
		if (NULL == str_value) {
			audit_max = 0;
			config_file_set_value(pfile, "AUDIT_MAX_NUM", "0");
		} else {
			audit_max = atoi(str_value);
			if (audit_max < 0) {
				audit_max = 0;
				config_file_set_value(pfile, "AUDIT_MAX_NUM", "0");
			}
		}
		printf("[%s]: audit capacity is %d\n", file_name, audit_max);	
		str_value = config_file_get_value(pfile, "AUDIT_INTERVAL");
		if (NULL == str_value) {
			audit_interval = 60;
			config_file_set_value(pfile, "AUDIT_INTERVAL", "1minute");
		} else {
			audit_interval = atoitvl(str_value);
			if (audit_interval <= 0) {
				audit_interval = 60;
				config_file_set_value(pfile, "AUDIT_INTERVAL", "1minute");
			}
		}
		itvltoa(audit_interval, temp_buff);
		printf("[%s]: audit interval is %s\n", file_name, temp_buff);
		str_value = config_file_get_value(pfile, "AUDIT_TIMES");
		if (NULL == str_value) {
			audit_times = 10;
			config_file_set_value(pfile, "AUDIT_TIMES", "10");
		} else {
			audit_times = atoi(str_value);
			if (audit_times <= 0) {
				audit_times = 10;
				config_file_set_value(pfile, "AUDIT_TIMES", "10");
			}
		}
		printf("[%s]: audit times is %d\n", file_name, audit_times);
		str_value = config_file_get_value(pfile, "TEMP_LIST_SIZE");
		if (NULL == str_value) {
			temp_list_size = 0;
			config_file_set_value(pfile, "TEMP_LIST_SIZE", "0");
		} else {
			temp_list_size = atoi(str_value);
			if (temp_list_size < 0) {
				temp_list_size = 0;
				config_file_set_value(pfile, "TEMP_LIST_SIZE", "0");
			}
		}
		printf("[%s]: temporary list capacity is %d\n", file_name,
			temp_list_size);
		str_value = config_file_get_value(pfile, "GREY_GROWING_NUM");
		if (NULL == str_value) {
			growing_num = 0;
			config_file_set_value(pfile, "GREY_GROWING_NUM", "0");
		} else {
			growing_num = atoi(str_value);
			if (growing_num < 0) {
				growing_num = 0;
				config_file_set_value(pfile, "GREY_GROWING_NUM", "0");
			}
		}
		printf("[%s]: grey list growing number is %d\n", file_name,
			growing_num);
		if (FALSE == config_file_save(pfile)) {
			printf("[%s]: fail to write configuration back to file\n",
					file_name);
			config_file_free(pfile);
			return FALSE;
		}
		judge_name = config_file_get_value(pfile, "JUDGE_SERVICE_NAME");
		add_name = config_file_get_value(pfile, "ADD_SERVICE_NAME");
		query_name = config_file_get_value(pfile, "QUERY_SERVICE_NAME");
		sprintf(list_path, "%s/%s.txt", get_data_path(), file_name);
		if (0 != stat(list_path, &node_stat)) {
			fd = open(list_path, O_WRONLY|O_CREAT|O_TRUNC, DEF_MODE);
			close(fd);
			printf("[%s]: warning! cannot find data file!!!\n", file_name);
		}
		ip_filter_init(file_name, config_path, audit_max, audit_interval, 
				audit_times, temp_list_size, list_path, growing_num);
		if (0 != ip_filter_run()) {
			printf("[%s]: fail to run the module\n", file_name);
			config_file_free(pfile);
			return FALSE;
		}
		if (NULL != judge_name &&
			FALSE == register_service(judge_name, ip_filter_judge)) {
			printf("[%s]: fail to register \"%s\" service\n", file_name,
					judge_name);
			config_file_free(pfile);
			return FALSE;
		}
		if (NULL != query_name &&
			FALSE == register_service(query_name, ip_filter_query)) {
			printf("[%s]: fail to register \"%s\" service\n", file_name,
					query_name);
			config_file_free(pfile);
			return FALSE;
		}
		if (NULL != add_name &&
			FALSE==register_service(add_name,ip_filter_add_ip_into_temp_list)) {
			printf("[%s]: fail to register \"%s\" service\n", file_name,
					add_name);
			config_file_free(pfile);
			return FALSE;
		}
		config_file_free(pfile);
		return TRUE;
	case PLUGIN_FREE:
		ip_filter_stop();
		ip_filter_free();
		return TRUE;
	}
}


