#ifndef _H_SYSTEM_SERVICES_
#define _H_SYSTEM_SERVICES_

#include "common_types.h"
#include "mem_file.h"
#include "xarray.h"
#include "double_list.h"
#include "single_list.h"

void system_services_init();

int system_services_run();

int system_services_stop();

void system_services_free();

extern BOOL (*system_services_get_user_lang)(const char*, char*);
extern BOOL (*system_services_get_timezone)(const char*, char *);
extern BOOL (*system_services_get_username_from_id)(int, char*);
extern BOOL (*system_services_get_id_from_username)(const char*, int*);
extern BOOL (*system_services_get_user_ids)(const char*, int*, int*, int*);
extern BOOL (*system_services_lang_to_charset)(const char*, char*);
extern const char* (*system_services_cpid_to_charset)(uint32_t);
extern uint32_t (*system_services_charset_to_cpid)(const char*);
extern const char* (*system_services_lcid_to_ltag)(uint32_t);
extern uint32_t (*system_services_ltag_to_lcid)(const char*);
extern const char* (*system_services_mime_to_extension)(const char*);
extern const char* (*system_services_extension_to_mime)(const char*);
extern void (*system_services_broadcast_event)(const char*);
#endif /* _H_SYSTEM_SERVICES_ */
