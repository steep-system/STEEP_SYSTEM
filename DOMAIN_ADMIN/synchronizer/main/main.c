#include "processing_engine.h"
#include "file_operation.h"
#include "url_downloader.h"
#include "config_file.h"
#include "mail_func.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#define SYNCHRONIZER_VERSION      "1.0"

static BOOL g_notify_stop = FALSE;

static void term_handler(int signo);

int main(int argc, char **argv)
{
	char *str_value;
	char data_path[256];
	char log_path[256];
	char mount_path[256];
	CONFIG_FILE *pconfig;

	if (2 != argc) {
		printf("%s <cfg file>\n", argv[0]);
		return -10;
	}
	if (2 == argc && 0 == strcmp(argv[1], "--help")) {
		printf("%s <cfg file>\n", argv[0]);
		return 0;
	}
	if (2 == argc && 0 == strcmp(argv[1], "--version")) {
		printf("version: %s\n", SYNCHRONIZER_VERSION);
		return 0;
	}
	pconfig = config_file_init(argv[1]);
	if (NULL == pconfig) {
		printf("[system]: fail to open config file %s\n", argv[1]);
		return -1;
	}
	str_value = config_file_get_value(pconfig, "DATA_FILE_PATH");
	if (NULL == str_value) {
		strcpy(data_path, "../data");
		config_file_set_value(pconfig, "DATA_FILE_PATH", "../data");
	} else {
		strcpy(data_path, str_value);
	}
	printf("[system]: data path is %s\n", data_path);

	str_value = config_file_get_value(pconfig, "GATEWAY_MOUNT_PATH");
	if (NULL == str_value) {
		strcpy(mount_path, "../gateway");
		config_file_set_value(pconfig, "GATEWAY_MOUNT_PATH", "../gateway");
	} else {
		strcpy(mount_path, str_value);
	}
	printf("[system]: gateway mount path is %s\n", mount_path);

	
	str_value = config_file_get_value(pconfig, "MASTER_ADDRESS");
	
	if (NULL == str_value) {
		printf("[system]: web adaptor will not work\n");
	} else {
		printf("[system]: web adaptor will work\n");
	}
	
	
	
	url_downloader_init();
	file_operation_init(mount_path);
	processing_engine_init(str_value, data_path, argv[1]);
	
	config_file_save(pconfig);
	config_file_free(pconfig);
	
	if (0 != url_downloader_run()) {
		printf("[system]: fail to run url downloader\n");
		return -1;
	}
	if (0 != file_operation_run()) {
		printf("[system]: fail to run file operation\n");
		return -2;
	}
	if (0 != processing_engine_run()) {
		printf("[system]: fail to processing engine\n");
		return -3;
	}
	printf("[system]: SYNCHRONIZER run OK\n");
	
	signal(SIGTERM, term_handler);
	while (TRUE != g_notify_stop) {
		sleep(1);
	}
	
	processing_engine_stop();
	file_operation_stop();
	url_downloader_stop();

	url_downloader_free();
	file_operation_free();
	processing_engine_free();
	return 0;
}

static void term_handler(int signo)
{
	g_notify_stop = TRUE;
}
