#ifndef _H_LIST_UI_
#define _H_LIST_UI_

void list_ui_init(const char *list_path, const char *url_link,
	const char *resource_path, int store_ratio);

int list_ui_run();

int list_ui_stop();

void list_ui_free();


#endif

