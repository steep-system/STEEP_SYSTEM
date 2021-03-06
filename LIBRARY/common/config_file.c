/*
 *	config file parser, which parse a (key = value) format config file.
 *	The comments is start with '#' at the leading of each comment line
 *
 */
#include "config_file.h"
#include "util.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_LINE_LEN		1024
#define EXT_ENTRY_NUM		64

static void config_file_parse_line(CONFIG_FILE *cfg, char* line);

/*
 *	init a config file object with the specified filename
 *
 *	@param
 *		filename [in]		the config filename that mapped 
 *							the object
 *	@return
 *		a pointer point to the config file object, NULL if
 *		some error occurs
 */
CONFIG_FILE* config_file_init(char* filename)
{
	CONFIG_FILE* cfg = NULL;	/* the config object pointer	*/
	char line[MAX_LINE_LEN];	/* current line being processed */
	FILE *fin = NULL;			/* umta config file pointer		*/
	size_t i, table_size;		   /* loop counter, table line num */
	

	if (NULL == (fin = fopen(filename, "r+"))) {
		debug_info("[config_file]: config_file_init: open %s fail", filename);
		return NULL;
	}

	if (NULL == (cfg = malloc(sizeof(CONFIG_FILE)))) {
		debug_info("[config_file]: config_file_init: %s, alloc fail", filename);
		fclose(fin);
		return NULL;
	}
	memset(cfg, 0, sizeof(CONFIG_FILE));
	for (table_size = 0; fgets(line, MAX_LINE_LEN, fin); table_size++) {
		if (line[0] == '\r' || line[0] == '\n' || line[0] == '#') {
			table_size--;
		}
	}
	cfg->total_entries = table_size + EXT_ENTRY_NUM;
	if (NULL == (cfg->config_table = malloc(cfg->total_entries * 
		sizeof(CONFIG_ENTRY)))) {
		debug_info("[config_file]: config_file_init: %s, alloc fail", filename);
		free(cfg);
		fclose(fin);
		return NULL;
	}
	memset(cfg->config_table, 0, cfg->total_entries * sizeof(CONFIG_ENTRY));
	
	rewind(fin);
	/* read the first 2 entries from each line, the rest are comments */

	for (i=0; fgets(line, MAX_LINE_LEN, fin); i++) {
		if (line[0] == '\r' || line[0] == '\n' || line[0] == '#') {
			i--;
			continue;
		}
		/* prevent line exceed maximum length ---MAX_LINE_LEN */
		line[sizeof(line) - 1] = '\0';
		config_file_parse_line(cfg, line);
	}

	fclose(fin);
	strcpy(cfg->file_name, filename);
	return cfg;
}

/*
 *	free the specified config file object
 *
 *	@param
 *		cfg_file [in]	the object to release
 */
void config_file_free(CONFIG_FILE* cfg_file)
{
	if (NULL == cfg_file) {
		return;
	}
	free(cfg_file->config_table);
	free(cfg_file);
}

/*
 *	get the value from the specified config file object with 
 *	the key as the entry
 *
 *	@param
 *		cfg_file [in]		the specified config file object
 *		key [in]			which mapped to the return value
 *
 *	@return
 *		the value that mapped the specified key
 */
char* config_file_get_value(CONFIG_FILE* cfg_file, char* key)
{
	size_t i, len;

#ifdef _DEBUG_UMTA
	if (NULL == cfg_file || NULL == key) {
		debug_info("[config_file]: config_file_get_value: invalid param");
		return NULL;
	}
#endif
	len = strlen(key);
	for (i=0; i<len; i++) {
		if ((key[i] >= '0' && key[i] <= '9') ||
			(key[i] >= 'a' && key[i] <= 'z') ||
			(key[i] >= 'A' && key[i] <= 'Z') ||
			'-' == key[i] || '_' == key[i]) {
			continue;
		} else {
			return NULL;
		}
	}
	for (i = 0; i < cfg_file->num_entries; i++) {
		if (0 == strcasecmp(key, cfg_file->config_table[i].keyname)) {
			return cfg_file->config_table[i].value;
		}
	}
	return NULL;
}

/*
 *	parse the specified line and store the first two item into
 *	the key and value buffer. the line is like:
 *			key = value		# comments
 *
 *	@param
 *		cfg [in]		config file object
 *		line [int]		line read from file
 */
static void config_file_parse_line(CONFIG_FILE *cfg, char* line)
{
	char temp_buf[MAX_LINE_LEN];
	char *equal_ptr = NULL;
	char *sharp_ptr = NULL;
	char *cr_ptr	= NULL;
	char *lf_ptr	= NULL;
	size_t index;

#ifdef _DEBUG_UMTA
	if (NULL == cfg || NULL == line) {
		debug_info("[config_file]: config_file_parse_line, param NULL");
		return;
	}
#endif
	strcpy(temp_buf, line);
	cr_ptr = strchr(temp_buf, '\r');
	if (NULL != cr_ptr) {
		*cr_ptr = '\0';
	}
	lf_ptr = strchr(temp_buf, '\n');
	if (NULL != lf_ptr) {
		*lf_ptr = '\0';
	}
	ltrim_string(temp_buf);
	rtrim_string(temp_buf);
	equal_ptr = strchr(temp_buf, '=');
	sharp_ptr = strchr(temp_buf, '#');
	if (NULL == equal_ptr) {
		return;
	}
	if ('\0' == *(equal_ptr + 1)) {
		return;
	}
	if (NULL != sharp_ptr && sharp_ptr < equal_ptr) {
		return;
	}
	if (NULL != sharp_ptr) {
		*sharp_ptr = '\0';
	}
	*equal_ptr = '\0';
	equal_ptr ++;
	rtrim_string(temp_buf);
	ltrim_string(equal_ptr);
	if (0 == strlen(temp_buf) || 0 == strlen(equal_ptr)) {
		return;
	}
	index = cfg->num_entries;
	cfg->num_entries ++;
	cfg->config_table[index].is_touched = FALSE;
	strcpy(cfg->config_table[index].keyname, temp_buf);
	strcpy(cfg->config_table[index].value, equal_ptr);
	return;
}

BOOL config_file_set_value(CONFIG_FILE* cfg_file, char* key, char* value)
{
	size_t index, i, len;   

#ifdef _DEBUG_UMTA
	if (NULL == cfg_file || NULL == key || NULL == value) {
		debug_info("[config_file]: config_file_set_value: invalid param");
		return FALSE;
	}
#endif
	len = strlen(key);
	for (i=0; i<len; i++) {
		if ((key[i] >= '0'&& key[i] <= '9') ||
			(key[i] >= 'a' && key[i] <= 'z') ||
			(key[i] >= 'A' && key[i] <= 'Z') ||
			'-' == key[i] || '_' == key[i]) {
			continue;
		} else {
			return FALSE;
		}
	}
	len = strlen(value);
	for (i=0; i<len; i++) {
		if ('#' == value[i]) {
			return FALSE;
		}
	}
	for (i=0; i<cfg_file->num_entries; i++) {
		if (0 == strcasecmp(key, cfg_file->config_table[i].keyname)) {
			strcpy(cfg_file->config_table[i].value, value);
			cfg_file->config_table[i].is_touched = TRUE;
			return TRUE;
		}
	}
	
	if (cfg_file->num_entries == cfg_file->total_entries) {
		return FALSE;
	}
	index = cfg_file->num_entries;
	cfg_file->num_entries ++;
	cfg_file->config_table[index].is_touched = TRUE;
	strcpy(cfg_file->config_table[index].keyname, key);
	strcpy(cfg_file->config_table[index].value, value);
	return TRUE;
}

BOOL config_file_save(CONFIG_FILE* cfg_file)
{
	size_t i, fd, size, len, written;
	struct stat node_stat;
	char *pbuff, *ptr, *psearch;
	char *plf, *psharp;
	char *pequal, *plf2, *psharp2;

	for (i=0; i<cfg_file->num_entries; i++) {
		if (TRUE == cfg_file->config_table[i].is_touched) {
			break;
		}
	}
	if (i == cfg_file->num_entries) {
		return TRUE;
	}
	if (0 != stat(cfg_file->file_name, &node_stat) ||
		0 == S_ISREG(node_stat.st_mode)) {
		return FALSE;
	}
	pbuff = malloc(node_stat.st_size + MAX_LINE_LEN * EXT_ENTRY_NUM);
	if (NULL == pbuff) {
		return FALSE;
	}
	fd = open(cfg_file->file_name, O_RDWR);
	if (-1 == fd) {
		free(pbuff);
		return FALSE;
	}
	if (node_stat.st_size != read(fd, pbuff, node_stat.st_size)) {
		free(pbuff);
		close(fd);
		return FALSE;
	}
	size = node_stat.st_size;
	for (i=0; i<cfg_file->num_entries; i++) {
		if (FALSE == cfg_file->config_table[i].is_touched) {
			continue;
		}
		psearch = pbuff - 1;
		while (psearch = search_string(psearch + 1,
			cfg_file->config_table[i].keyname, size)) {
			plf = (char*)memrchr(pbuff, '\n', psearch - pbuff);
			psharp = (char*)memrchr(pbuff, '#', psearch - pbuff);
			if (NULL == psharp || psharp < plf) {
				pequal = memchr(psearch, '=', size - (psearch - pbuff));
				if (NULL == pequal) {
					continue;
				}
				plf2 = memchr(psearch, '\n', size - (psearch - pbuff));
				if (NULL == plf2) {
					plf2 = pbuff + size;
				}
				psharp = memchr(psearch, '#', size - (psearch - pbuff));
				if (NULL == psharp) {
					psharp = pbuff + size;
				}
				if (plf2 < pequal || psharp < pequal) {
					continue;
				}
				for (ptr=psearch+strlen(cfg_file->config_table[i].keyname);
					ptr<pequal; ptr++) {
					if (*ptr != ' ' && *ptr != '\t') {
						break;
					}
				}
				if (ptr == pequal) {
					break;
				}
			}
		}
		if (NULL == psearch) {
			if ('\n' != pbuff[size - 1]) {
				pbuff[size] = '\n';
				size ++;
			}
			len = strlen(cfg_file->config_table[i].keyname);
			memcpy(pbuff + size, cfg_file->config_table[i].keyname, len);
			size += len;
			memcpy(pbuff + size, " = ", 3);
			size += 3;
			len = strlen(cfg_file->config_table[i].value);
			memcpy(pbuff + size, cfg_file->config_table[i].value, len);
			size += len;
		} else {
			len = strlen(cfg_file->config_table[i].value);
			if (len < plf2 - pequal - 1) {
				pequal[1] = ' ';
				memcpy(pequal + 2, cfg_file->config_table[i].value, len);
				memset (pequal + 2 + len, ' ', plf2 - pequal - 2 - len);
			} else {
				memmove(pequal + len + 2, plf2, size - (plf2 - pbuff));
				pequal[1] = ' ';
				memcpy(pequal + 2, cfg_file->config_table[i].value, len);
				size += len + 1 - (plf2 - pequal - 1);
			}
		}
		cfg_file->config_table[i].is_touched = FALSE;
	}
	lseek(fd, 0, SEEK_SET);
	written = write(fd, pbuff, size);
	free(pbuff);
	close(fd);
	if (written != size ) {
		return FALSE;
	} else {
		return TRUE;
	}
}

