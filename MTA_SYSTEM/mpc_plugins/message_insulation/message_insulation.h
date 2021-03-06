#ifndef _H_INSULATION_QUEUE_
#define _H_INSULATION_QUEUE_
#include "hook_common.h"

void message_insulation_init(const char* config_path, const char *queue_path,
	int scan_interval, int on_valid_interval, int anon_valid_interval);

int message_insulation_run();

int message_insulation_stop();

void message_insulation_free();

BOOL message_insulation_activate(const char *file_name);

void message_insulation_console_talk(int argc, char **argv, char *result,
	int length);

#endif /* _H_INSULATION_QUEUE_ */
