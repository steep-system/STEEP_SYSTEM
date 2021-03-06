#ifndef _H_TABLE_UI_
#define _H_TABLE_UI_

void table_ui_init(const char *list_path, const char *dns_path,
	const char *mount_path, const char *url_link,
	const char *resource_path);

int table_ui_run();

int table_ui_stop();

void table_ui_free();

#endif

