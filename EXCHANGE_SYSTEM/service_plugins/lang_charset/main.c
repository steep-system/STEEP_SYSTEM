#include "service_common.h"
#include "str_hash.h"
#include "list_file.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

DECLARE_API;

enum{
	REFRESH_OK,
	REFRESH_FILE_ERROR,
	REFRESH_HASH_FAIL
};


static int table_refresh();

static BOOL table_query(const char* str, char *charset);

static void console_talk(int argc, char **argv, char *result, int length);

static STR_HASH_TABLE* g_hash_table = NULL;
static pthread_rwlock_t g_refresh_lock;
static char g_list_path[256];

BOOL SVC_LibMain(int reason, void **ppdata)
{
	char *str_value, *psearch;
	char file_name[256];
	
	
	switch(reason) {
	case PLUGIN_INIT:
		LINK_API(ppdata);
		/* get the plugin name from system api */
		strcpy(file_name, get_plugin_name());
		psearch = strrchr(file_name, '.');
		if (NULL != psearch) {
			*psearch = '\0';
		}
		
		sprintf(g_list_path, "%s/%s.txt", get_data_path(), file_name);
		pthread_rwlock_init(&g_refresh_lock, NULL);
		if (REFRESH_OK != table_refresh()) {
			printf("[lang_charset]: fail to load hash table\n", file_name);
			return FALSE;
		}
		if (FALSE == register_service("lang_to_charset",
			table_query)) {
			printf("[lang_charset]: fail to register \"lang_to_charset\" service\n");
			return FALSE;
		}
		
		if (FALSE == register_talk(console_talk)) {
			printf("[lang_charset]: fail to register console talk\n", file_name);
			return FALSE;
		}
		
		return TRUE;
	case PLUGIN_FREE:
		if (NULL != g_hash_table) {
			str_hash_free(g_hash_table);
			g_hash_table = NULL;
		}
		pthread_rwlock_destroy(&g_refresh_lock);
		g_list_path[0] = '\0';
		return TRUE;
	}
}



static BOOL table_query(const char* lang, char *charset)
{
	char *pcharset;
	char temp_string[32];
	
	if (NULL == lang) {
		return FALSE;
	}
	strncpy(temp_string, lang, sizeof(temp_string));
	temp_string[sizeof(temp_string) - 1] = '\0';
	lower_string(temp_string);
	
	pthread_rwlock_rdlock(&g_refresh_lock);
    pcharset = str_hash_query(g_hash_table, temp_string);
	if (NULL == pcharset) {
		charset[0] = '\0';
	} else {
		strncpy(charset, pcharset, 32);
	}
	pthread_rwlock_unlock(&g_refresh_lock);
	if (NULL == pcharset) {
		return FALSE;
	} else {
		return TRUE;
	}
}


static int table_refresh()
{
    STR_HASH_TABLE *phash = NULL;
    int i, list_len, hash_cap;
	LIST_FILE *plist_file;
	char *pitem;
	
    /* initialize the list filter */
	plist_file = list_file_init(g_list_path, "%s:32%s:32");
	if (NULL == plist_file) {
		printf("[lang_charset]: fail to open list file\n");
		return REFRESH_FILE_ERROR;
	}
	pitem = (char*)list_file_get_list(plist_file);
	list_len = list_file_get_item_num(plist_file);
	hash_cap = list_len + 1;
	
    phash = str_hash_init(hash_cap, 32, NULL);
	if (NULL == phash) {
		printf("[lang_charset]: fail to allocate hash map");
		list_file_free(plist_file);
		return REFRESH_HASH_FAIL;
	}
    for (i=0; i<list_len; i++) {
		lower_string(pitem + 64*i);
        str_hash_add(phash, pitem + 64*i, pitem + 64*i + 32);   
    }
    list_file_free(plist_file);
	
	pthread_rwlock_wrlock(&g_refresh_lock);
	if (NULL != g_hash_table) {
		str_hash_free(g_hash_table);
	}
    g_hash_table = phash;
    pthread_rwlock_unlock(&g_refresh_lock);

    return REFRESH_OK;
}


static void console_talk(int argc, char **argv, char *result, int length)
{
	char charset[32];
	char help_string[] = "250 lang charset help information:\r\n"
						 "\t%s reload\r\n"
						 "\t    --reload the string table from list file\r\n"
						 "\t%s search <string>\r\n"
						 "\t    --search string in the string table";

	if (1 == argc) {
		strncpy(result, "550 too few arguments", length);
		return;
	}
	if (2 == argc && 0 == strcmp("--help", argv[1])) {
		snprintf(result, length, help_string, argv[0], argv[0]);
		result[length - 1] ='\0';
		return;
	}
	if (2 == argc && 0 == strcmp("reload", argv[1])) {
		switch(table_refresh()) {
		case REFRESH_OK:
			strncpy(result, "250 lang charset reload OK", length);
			return;
		case REFRESH_FILE_ERROR:
			strncpy(result, "550 lang charset file error", length);
			return;
		case REFRESH_HASH_FAIL:
			strncpy(result, "550 hash map error for lang charset", length);
			return;
		}
	}
	
	if (3 == argc && 0 == strcmp("search", argv[1])) {
		if (TRUE == table_query(argv[2], charset)) {
			snprintf(result, length, "250 charset for %s is %s", argv[2], charset);
		} else {
			snprintf(result, length, "550 charset not found for %s", argv[2]);
		}
		return;
	}
	snprintf(result, length, "550 invalid argument %s", argv[1]);
	return;
}

