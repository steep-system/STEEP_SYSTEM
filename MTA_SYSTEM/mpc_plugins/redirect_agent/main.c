#include "hook_common.h"
#include <stdio.h>

typedef BOOL (*REDIRECT_DOMAINS_QUERY)(char*);

DECLARE_API;

static REDIRECT_DOMAINS_QUERY redirect_domains_query;

static BOOL mail_hook(MESSAGE_CONTEXT *pcontext);

BOOL HOOK_LibMain(int reason, void **ppdata)
{
    /* path conatins the config files directory */
    switch (reason) {
    case PLUGIN_INIT:
		LINK_API(ppdata);
		redirect_domains_query = (REDIRECT_DOMAINS_QUERY)query_service(
								"redirect_domains_query");
		if (NULL == redirect_domains_query) {
			printf("[redirect_agent]: fail to get \"redirect_domains_query\" "
				"service\n");
            return FALSE;
		}
        if (FALSE == register_hook(mail_hook)) {
			printf("[redirect_agent]: fail to register the hook function\n");
            return FALSE;
        }
		printf("[redirect_agent]: plugin is loaded into system\n");
        return TRUE;
    case PLUGIN_FREE:
        return TRUE;
    }
}

static BOOL mail_hook(MESSAGE_CONTEXT *pcontext)
{
	char *pdomain;
	char rcpt_to[256];

	while (MEM_END_OF_FILE != mem_file_readline(&pcontext->pcontrol->f_rcpt_to,
		rcpt_to, 256)) {
		pdomain = strchr(rcpt_to, '@');
		if (NULL == pdomain) {
			continue;
		}
		pdomain ++;
		if (TRUE == redirect_domains_query(pdomain)) {
			pcontext->pcontrol->bound_type = BOUND_OUT;
			return FALSE;
		}
	}
	return FALSE;
}

