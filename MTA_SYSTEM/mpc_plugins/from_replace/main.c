#include "hook_common.h"
#include "str_hash.h"
#include "list_file.h"
#include "util.h"
#include <pthread.h>


DECLARE_API;


enum{
	REFRESH_OK,
	REFRESH_FILE_ERROR,
	REFRESH_HASH_FAIL
};


static STR_HASH_TABLE* g_hash_table;
static pthread_rwlock_t g_refresh_lock;
static char g_list_path[256];


static int table_refresh();

static BOOL table_query(const char* str, char *buff);

static void console_talk(int argc, char **argv, char *result, int length);
	
static BOOL mail_hook(MESSAGE_CONTEXT *pcontext);

BOOL HOOK_LibMain(int reason, void **ppdata)
{
	char *psearch;
	char file_name[256];
	
    /* path conatins the config files directory */
    switch (reason) {
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
		
		g_hash_table = NULL;

		if (REFRESH_OK != table_refresh()) {
			printf("[from_replace]: fail to load replace table\n");
			return FALSE;
		}
        if (FALSE == register_hook(mail_hook)) {
			printf("[from_replace]: fail to register the hook function\n");
            return FALSE;
        }
		register_talk(console_talk);
		printf("[from_replace]: plugin is loaded into system\n");
        return TRUE;
    case PLUGIN_FREE:
    	if (NULL != g_hash_table) {
        	str_hash_free(g_hash_table);
        	g_hash_table = NULL;
    	}
    	pthread_rwlock_destroy(&g_refresh_lock);
        return TRUE;
    }
}

static BOOL mail_hook(MESSAGE_CONTEXT *pcontext)
{
	char from_buff[256];

	if (TRUE == table_query(pcontext->pcontrol->from, from_buff)) {
		strcpy(pcontext->pcontrol->from, from_buff);
	}
	return FALSE;
}


static BOOL table_query(const char* str, char *buff)
{
	char *presult;
	char temp_string[256];
	
	if (NULL == str) {
		return FALSE;
	}
	strncpy(temp_string, str, sizeof(temp_string));
	temp_string[sizeof(temp_string) - 1] = '\0';
	lower_string(temp_string);
	pthread_rwlock_rdlock(&g_refresh_lock);
    presult = str_hash_query(g_hash_table, temp_string);
    if (NULL != presult) {
    	strcpy(buff, presult);	
    }
	pthread_rwlock_unlock(&g_refresh_lock);
	if (NULL == presult) {
		return FALSE;
	} else {
		return TRUE;
	}
}

/*
 *  refresh the string list, the list is from the
 *  file which is specified in configuration file.
 *
 *  @return
 *		REFRESH_OK			OK
 *		FILE_ERROR	fail to open list file
 *		REFRESH_HASH_FAIL		fail to open hash map
 */
static int table_refresh()
{
    STR_HASH_TABLE *phash = NULL;
    int i, list_len;
	LIST_FILE *plist_file;
	char *pitem;
	
    /* initialize the list filter */
	plist_file = list_file_init(g_list_path, "%s:256%s:256");
	if (NULL == plist_file) {
		return REFRESH_FILE_ERROR;
	}
	pitem = (char*)list_file_get_list(plist_file);
	list_len = list_file_get_item_num(plist_file);
	
    phash = str_hash_init(list_len + 1, 256, NULL);
	if (NULL == phash) {
		printf("[from_replace]: fail to allocate hash map\n");
		list_file_free(plist_file);
		return REFRESH_HASH_FAIL;
	}
    for (i=0; i<list_len; i++) {
		if (NULL == strchr(pitem + 2*256*i + 256, '@') ||
			NULL != strchr(pitem + 2*256*i + 256, ' ')) {
			printf("[from_replace]: address format error in line %d\n", i);
			continue;
		}
		lower_string(pitem + 2*256*i);
        str_hash_add(phash, pitem + 2*256*i, pitem + 2*256*i + 256);   
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


/*
 *	string table's console talk
 *	@param
 *		argc			arguments number
 *		argv [in]		arguments value
 *		result [out]	buffer for retrieving result
 *		length			result buffer length
 */
static void console_talk(int argc, char **argv, char *result, int length)
{
	char help_string[] = "250 from replace help information:\r\n"
						 "\t%s reload\r\n"
						 "\t    --reload the replace table from list file";

	if (1 == argc) {
		strncpy(result, "550 too few arguments", length);
		return;
	}
	if (2 == argc && 0 == strcmp("--help", argv[1])) {
		snprintf(result, length, help_string, argv[0]);
		result[length - 1] ='\0';
		return;
	}
	if (2 == argc && 0 == strcmp("reload", argv[1])) {
		switch(table_refresh()) {
		case REFRESH_OK:
			strncpy(result, "250 replace table reload OK", length);
			return;
		case REFRESH_FILE_ERROR:
			strncpy(result, "550 relpace list file error", length);
			return;
		case REFRESH_HASH_FAIL:
			strncpy(result, "550 hash map error for replace table", length);
			return;
		}
	}
	snprintf(result, length, "550 invalid argument %s", argv[1]);
	return;
}


