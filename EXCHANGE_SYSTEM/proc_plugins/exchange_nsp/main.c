#include "guid.h"
#include "util.h"
#include "nsp_ndr.h"
#include "ab_tree.h"
#include "common_util.h"
#include "proc_common.h"
#include "config_file.h"
#include "nsp_interface.h"
#include <string.h>
#include <stdio.h>

DECLARE_API;

static int exchange_nsp_ndr_pull(int opnum, NDR_PULL* pndr, void **ppin);

static int exchange_nsp_dispatch(int opnum, const GUID *pobject,
	uint64_t handle, void *pin, void **ppout);
	
static void exchange_nsp_pull_free(int opnum, void *pin);

static int exchange_nsp_ndr_push(int opnum, NDR_PUSH *pndr, void *pout);

static void exchange_nsp_push_free(int opnum, void *pout);

static void exchange_nsp_unbind(uint64_t handle);

#define MAPI_E_SUCCESS 0x00000000

#define MAPI_E_LOGON_FAILED 0x80040111

BOOL PROC_LibMain(int reason, void **ppdata)
{
	int org_size;
	BOOL b_check;
	char *org_name;
	int table_size;
	int max_item_num;
	void *pendpoint1;
	void *pendpoint2;
	int cache_interval;
	CONFIG_FILE *pfile;
	char temp_buff[32];
	char file_name[256];
	char temp_path[256];
	char *str_value, *psearch;
	DCERPC_INTERFACE interface;
	
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
		sprintf(temp_path, "%s/%s.cfg", get_config_path(), file_name);
		pfile = config_file_init(temp_path);
		if (NULL == pfile) {
			printf("[exchange_nsp]: error to open config file!!!\n");
			return FALSE;
		}
		org_name = config_file_get_value(pfile, "X500_ORG_NAME");
		if (NULL == org_name) {
			org_name = "gridware information";
		}
		printf("[exchange_nsp]: x500 org name is \"%s\"\n", org_name);
		str_value = config_file_get_value(pfile, "HASH_TABLE_SIZE");
		if (NULL == str_value) {
			table_size = 3000;
			config_file_set_value(pfile, "HASH_TABLE_SIZE", "3000");
		} else {
			table_size = atoi(str_value);
			if (table_size <= 0) {
				table_size = 3000;
				config_file_set_value(pfile, "HASH_TABLE_SIZE", "3000");
			}
		}
		printf("[exchange_nsp]: hash table size is %d\n", table_size);
		str_value = config_file_get_value(pfile, "CACHE_INTERVAL");
		if (NULL == str_value) {
			cache_interval = 300;
			config_file_set_value(pfile, "CACHE_INTERVAL", "5minutes");
		} else {
			cache_interval = atoitvl(str_value);
			if (cache_interval > 24*3600 || cache_interval < 60) {
				cache_interval = 300;
				config_file_set_value(pfile, "CACHE_INTERVAL", "5minutes");
			}
		}
		itvltoa(cache_interval, temp_buff);
		printf("[exchange_nsp]: address book tree item"
				" cache interval is %s\n", temp_buff);
		str_value = config_file_get_value(pfile, "MAX_ITEM_NUM");
		if (NULL == str_value) {
			max_item_num = 100000;
			config_file_set_value(pfile, "MAX_ITEM_NUM", "100000");
		} else {
			max_item_num = atoi(str_value);
			if (max_item_num <= 0) {
				max_item_num = 100000;
				config_file_set_value(pfile, "MAX_ITEM_NUM", "100000");
			}
		}
		printf("[exchange_nsp]: maximum item number is %d\n", max_item_num);
		str_value = config_file_get_value(pfile, "SESSION_CHECK");
		if (NULL != str_value && (0 == strcasecmp(str_value,
			"ON") || 0 == strcasecmp(str_value, "TRUE"))) {
			b_check = TRUE;
			printf("[exchange_nsp]: bind session will be checked\n");
		} else {
			b_check = FALSE;
		}
		common_util_init();
		ab_tree_init(org_name, table_size, cache_interval, max_item_num);
		config_file_save(pfile);
		config_file_free(pfile);
		pendpoint1 = register_endpoint("*", 6001);
		if (NULL == pendpoint1) {
			printf("[exchange_nsp]: fail to register endpoint with port 6001\n");
			return FALSE;
		}
		pendpoint2 = register_endpoint("*", 6004);
		if (NULL == pendpoint2) {
			printf("[exchange_nsp]: fail to register endpoint with port 6004\n");
			return FALSE;
		}
		strcpy(interface.name, "exchangeNSP");
		guid_from_string(&interface.uuid, "f5cc5a18-4264-101a-8c59-08002b2f8426");
		interface.version = 56;
		interface.ndr_pull = exchange_nsp_ndr_pull;
		interface.dispatch = exchange_nsp_dispatch;
		interface.ndr_push = exchange_nsp_ndr_push;
		interface.unbind = exchange_nsp_unbind;
		interface.reclaim = NULL;
		if (FALSE == register_interface(pendpoint1, &interface) ||
			FALSE == register_interface(pendpoint2, &interface)) {
			printf("[exchange_nsp]: fail to register interface\n");
			return FALSE;
		}
		if (0 != common_util_run()) {
			printf("[exchange_nsp]: fail to run common util\n");
			return FALSE;
		}
		if (0 != ab_tree_run()) {
			printf("[exchange_nsp]: fail to run address book tree\n");
			return FALSE;
		}
		nsp_interface_init(b_check);
		if (0 != nsp_interface_run()) {
			printf("[exchange_nsp]: fail to run nsp interface\n");
			return FALSE;
		}
		printf("[exchange_nsp]: plugin is loaded into system\n");
		return TRUE;
	case PLUGIN_FREE:
		nsp_interface_stop();
		nsp_interface_free();
		ab_tree_stop();
		ab_tree_free();
		common_util_stop();
		common_util_free();
		return TRUE;
	}
}

static int exchange_nsp_ndr_pull(int opnum, NDR_PULL* pndr, void **ppin)
{
	
	switch (opnum) {
	case 0:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPIBIND_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspibind(pndr, *ppin);
	case 1:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPIUNBIND_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspiunbind(pndr, *ppin);
	case 2:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPIUPDATESTAT_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspiupdatestat(pndr, *ppin);
	case 3:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPIQUERYROWS_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspiqueryrows(pndr, *ppin);
	case 4:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPISEEKENTRIES_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspiseekentries(pndr, *ppin);
	case 5:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPIGETMATCHES_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspigetmatches(pndr, *ppin);
	case 6:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPIRESORTRESTRICTION_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspiresortrestriction(pndr, *ppin);
	case 7:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPIDNTOMID_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspidntomid(pndr, *ppin);
	case 8:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPIGETPROPLIST_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspigetproplist(pndr, *ppin);
	case 9:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPIGETPROPS_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspigetprops(pndr, *ppin);
	case 10:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPICOMPAREMIDS_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspicomparemids(pndr, *ppin);
	case 11:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPIMODPROPS_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspimodprops(pndr, *ppin);
	case 12:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPIGETSPECIALTABLE_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspigetspecialtable(pndr, *ppin);
	case 13:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPIGETTEMPLATEINFO_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspigettemplateinfo(pndr, *ppin);
	case 14:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPIMODLINKATT_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspimodlinkatt(pndr, *ppin);
	case 16:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPIQUERYCOLUMNS_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspiquerycolumns(pndr, *ppin);
	case 19:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPIRESOLVENAMES_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspiresolvenames(pndr, *ppin);
	case 20:
		*ppin = ndr_stack_alloc(NDR_STACK_IN, sizeof(NSPIRESOLVENAMESW_IN));
		if (NULL == *ppin) {
			return NDR_ERR_ALLOC;
		}
		return nsp_ndr_pull_nspiresolvenamesw(pndr, *ppin);
	default:
		return NDR_ERR_BAD_SWITCH;
	}
}

static int exchange_nsp_dispatch(int opnum, const GUID *pobject,
	uint64_t handle, void *pin, void **ppout)
{
	
	switch (opnum) {
	case 0:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPIBIND_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPIBIND_OUT*)(*ppout))->result = nsp_interface_bind(handle, ((NSPIBIND_IN*)pin)->flags,
											&((NSPIBIND_IN*)pin)->stat, ((NSPIBIND_IN*)pin)->pserver_guid,
											&((NSPIBIND_OUT*)(*ppout))->handle);
		((NSPIBIND_OUT*)(*ppout))->pserver_guid = ((NSPIBIND_IN*)pin)->pserver_guid;
		return DISPATCH_SUCCESS;
	case 1:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPIUNBIND_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPIUNBIND_OUT*)(*ppout))->result = nsp_interface_unbind(&((NSPIUNBIND_IN*)pin)->handle,
												((NSPIUNBIND_IN*)pin)->reserved);
		((NSPIUNBIND_OUT*)(*ppout))->handle = ((NSPIUNBIND_IN*)pin)->handle;
		return DISPATCH_SUCCESS;
	case 2:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPIUPDATESTAT_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPIUPDATESTAT_OUT*)(*ppout))->result = nsp_interface_update_stat(((NSPIUPDATESTAT_IN*)pin)->handle,
													((NSPIUPDATESTAT_IN*)pin)->reserved,
													&((NSPIUPDATESTAT_IN*)pin)->stat,
													((NSPIUPDATESTAT_IN*)pin)->pdelta);
		((NSPIUPDATESTAT_OUT*)(*ppout))->stat = ((NSPIUPDATESTAT_IN*)pin)->stat;
		((NSPIUPDATESTAT_OUT*)(*ppout))->pdelta = ((NSPIUPDATESTAT_IN*)pin)->pdelta;
		return DISPATCH_SUCCESS;
	case 3:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPIQUERYROWS_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPIQUERYROWS_OUT*)(*ppout))->result = nsp_interface_query_rows(((NSPIQUERYROWS_IN*)pin)->handle,
													((NSPIQUERYROWS_IN*)pin)->flags,
													&((NSPIQUERYROWS_IN*)pin)->stat,
													((NSPIQUERYROWS_IN*)pin)->table_count,
													((NSPIQUERYROWS_IN*)pin)->ptable,
													((NSPIQUERYROWS_IN*)pin)->count,
													((NSPIQUERYROWS_IN*)pin)->pproptags,
													&((NSPIQUERYROWS_OUT*)(*ppout))->prows);
		((NSPIQUERYROWS_OUT*)(*ppout))->stat = ((NSPIQUERYROWS_IN*)pin)->stat;
		return DISPATCH_SUCCESS;
	case 4:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPISEEKENTRIES_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPISEEKENTRIES_OUT*)(*ppout))->result = nsp_interface_seek_entries(((NSPISEEKENTRIES_IN*)pin)->handle,
													((NSPISEEKENTRIES_IN*)pin)->reserved,
													&((NSPISEEKENTRIES_IN*)pin)->stat,
													&((NSPISEEKENTRIES_IN*)pin)->target,
													((NSPISEEKENTRIES_IN*)pin)->ptable,
													((NSPISEEKENTRIES_IN*)pin)->pproptags,
													&((NSPISEEKENTRIES_OUT*)(*ppout))->prows);
		((NSPISEEKENTRIES_OUT*)(*ppout))->stat = ((NSPISEEKENTRIES_IN*)pin)->stat;
		return DISPATCH_SUCCESS;
	case 5:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPIGETMATCHES_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPIGETMATCHES_OUT*)(*ppout))->result = nsp_interface_get_matches(((NSPIGETMATCHES_IN*)pin)->handle,	
													((NSPIGETMATCHES_IN*)pin)->reserved1,
													&((NSPIGETMATCHES_IN*)pin)->stat,
													((NSPIGETMATCHES_IN*)pin)->preserved,
													((NSPIGETMATCHES_IN*)pin)->reserved2,
													((NSPIGETMATCHES_IN*)pin)->pfilter,
													((NSPIGETMATCHES_IN*)pin)->ppropname,
													((NSPIGETMATCHES_IN*)pin)->requested,
													&((NSPIGETMATCHES_OUT*)(*ppout))->poutmids,
													((NSPIGETMATCHES_IN*)pin)->pproptags,
													&((NSPIGETMATCHES_OUT*)(*ppout))->prows);
		((NSPIGETMATCHES_OUT*)(*ppout))->stat = ((NSPIGETMATCHES_IN*)pin)->stat;
		return DISPATCH_SUCCESS;
	case 6:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPIRESORTRESTRICTION_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPIRESORTRESTRICTION_OUT*)(*ppout))->result = nsp_interface_resort_restriction(((NSPIRESORTRESTRICTION_IN*)pin)->handle,
															((NSPIRESORTRESTRICTION_IN*)pin)->reserved,
															&((NSPIRESORTRESTRICTION_IN*)pin)->stat,
															&((NSPIRESORTRESTRICTION_IN*)pin)->inmids,
															&((NSPIRESORTRESTRICTION_IN*)pin)->poutmids);
		((NSPIRESORTRESTRICTION_OUT*)(*ppout))->stat = ((NSPIRESORTRESTRICTION_IN*)pin)->stat;
		((NSPIRESORTRESTRICTION_OUT*)(*ppout))->poutmids = ((NSPIRESORTRESTRICTION_IN*)pin)->poutmids;
		return DISPATCH_SUCCESS;
	case 7:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPIDNTOMID_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPIDNTOMID_OUT*)(*ppout))->result = nsp_interface_dntomid(((NSPIDNTOMID_IN*)pin)->handle,
												((NSPIDNTOMID_IN*)pin)->reserved,
												&((NSPIDNTOMID_IN*)pin)->names,
												&((NSPIDNTOMID_OUT*)(*ppout))->poutmids);
		return DISPATCH_SUCCESS;
	case 8:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPIGETPROPLIST_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPIGETPROPLIST_OUT*)(*ppout))->result = nsp_interface_get_proplist(((NSPIGETPROPLIST_IN*)pin)->handle,
													((NSPIGETPROPLIST_IN*)pin)->flags,
													((NSPIGETPROPLIST_IN*)pin)->mid,
													((NSPIGETPROPLIST_IN*)pin)->codepage,
													&((NSPIGETPROPLIST_OUT*)(*ppout))->pproptags);
		return DISPATCH_SUCCESS;
	case 9:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPIGETPROPS_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPIGETPROPS_OUT*)(*ppout))->result = nsp_interface_get_props(((NSPIGETPROPS_IN*)pin)->handle,
												((NSPIGETPROPS_IN*)pin)->flags,
												&((NSPIGETPROPS_IN*)pin)->stat,
												((NSPIGETPROPS_IN*)pin)->pproptags,
												&((NSPIGETPROPS_OUT*)(*ppout))->prows);
		return DISPATCH_SUCCESS;
	case 10:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPICOMPAREMIDS_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPICOMPAREMIDS_OUT*)(*ppout))->result1 = nsp_interface_compare_mids(((NSPICOMPAREMIDS_IN*)pin)->handle,
													((NSPICOMPAREMIDS_IN*)pin)->reserved,
													&((NSPICOMPAREMIDS_IN*)pin)->stat,
													((NSPICOMPAREMIDS_IN*)pin)->mid1,
													((NSPICOMPAREMIDS_IN*)pin)->mid2,
													&((NSPICOMPAREMIDS_OUT*)(*ppout))->result);
		return DISPATCH_SUCCESS;
	case 11:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPIMODPROPS_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPIMODPROPS_OUT*)(*ppout))->result = nsp_interface_mod_props(((NSPIMODPROPS_IN*)pin)->handle,
												((NSPIMODPROPS_IN*)pin)->reserved,
												&((NSPIMODPROPS_IN*)pin)->stat,
												((NSPIMODPROPS_IN*)pin)->pproptags,
												&((NSPIMODPROPS_IN*)pin)->row);
		return DISPATCH_SUCCESS;
	case 12:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPIGETSPECIALTABLE_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPIGETSPECIALTABLE_OUT*)(*ppout))->result = nsp_interface_get_specialtable(((NSPIGETSPECIALTABLE_IN*)pin)->handle,
														((NSPIGETSPECIALTABLE_IN*)pin)->flags,
														&((NSPIGETSPECIALTABLE_IN*)pin)->stat,
														&((NSPIGETSPECIALTABLE_IN*)pin)->version,
														&((NSPIGETSPECIALTABLE_OUT*)(*ppout))->prows);
		((NSPIGETSPECIALTABLE_OUT*)(*ppout))->version = ((NSPIGETSPECIALTABLE_IN*)pin)->version;
		return DISPATCH_SUCCESS;
	case 13:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPIGETTEMPLATEINFO_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPIGETTEMPLATEINFO_OUT*)(*ppout))->result = nsp_interface_get_templateinfo(((NSPIGETTEMPLATEINFO_IN*)pin)->handle,
														((NSPIGETTEMPLATEINFO_IN*)pin)->flags,
														((NSPIGETTEMPLATEINFO_IN*)pin)->type,
														((NSPIGETTEMPLATEINFO_IN*)pin)->pdn,
														((NSPIGETTEMPLATEINFO_IN*)pin)->codepage,
														((NSPIGETTEMPLATEINFO_IN*)pin)->locale_id,
														&((NSPIGETTEMPLATEINFO_OUT*)(*ppout))->pdata);
		return DISPATCH_SUCCESS;
	case 14:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPIMODLINKATT_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPIMODLINKATT_OUT*)(*ppout))->result = nsp_interface_mod_linkatt(((NSPIMODLINKATT_IN*)pin)->handle,
													((NSPIMODLINKATT_IN*)pin)->flags,
													((NSPIMODLINKATT_IN*)pin)->proptag,
													((NSPIMODLINKATT_IN*)pin)->mid,
													&((NSPIMODLINKATT_IN*)pin)->entry_ids);
		return DISPATCH_SUCCESS;
	case 16:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPIQUERYCOLUMNS_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPIQUERYCOLUMNS_OUT*)(*ppout))->result = nsp_interface_query_columns(((NSPIQUERYCOLUMNS_IN*)pin)->handle,
													((NSPIQUERYCOLUMNS_IN*)pin)->reserved,
													((NSPIQUERYCOLUMNS_IN*)pin)->flags,
													&((NSPIQUERYCOLUMNS_OUT*)(*ppout))->pcolumns);
		return DISPATCH_SUCCESS;
	case 19:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPIRESOLVENAMES_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPIRESOLVENAMES_OUT*)(*ppout))->result = nsp_interface_resolve_names(((NSPIRESOLVENAMES_IN*)pin)->handle,
													((NSPIRESOLVENAMES_IN*)pin)->reserved,
													&((NSPIRESOLVENAMES_IN*)pin)->stat,
													((NSPIRESOLVENAMES_IN*)pin)->pproptags,
													&((NSPIRESOLVENAMES_IN*)pin)->strs,
													&((NSPIRESOLVENAMES_OUT*)(*ppout))->pmids,
													&((NSPIRESOLVENAMES_OUT*)(*ppout))->prows);
		return DISPATCH_SUCCESS;
	case 20:
		*ppout = ndr_stack_alloc(NDR_STACK_OUT, sizeof(NSPIRESOLVENAMESW_OUT));
		if (NULL == *ppout) {
			return DISPATCH_FAIL;
		}
		((NSPIRESOLVENAMESW_OUT*)(*ppout))->result = nsp_interface_resolve_namesw(((NSPIRESOLVENAMESW_IN*)pin)->handle,
													((NSPIRESOLVENAMESW_IN*)pin)->reserved,
													&((NSPIRESOLVENAMESW_IN*)pin)->stat,
													((NSPIRESOLVENAMESW_IN*)pin)->pproptags,
													&((NSPIRESOLVENAMESW_IN*)pin)->strs,
													&((NSPIRESOLVENAMESW_OUT*)(*ppout))->pmids,
													&((NSPIRESOLVENAMESW_OUT*)(*ppout))->prows);
		return DISPATCH_SUCCESS;
	default:
		return DISPATCH_FAIL;
	}
}

static int exchange_nsp_ndr_push(int opnum, NDR_PUSH *pndr, void *pout)
{
	
	switch (opnum) {
	case 0:
		return nsp_ndr_push_nspibind(pndr, pout);
	case 1:
		return nsp_ndr_push_nspiunbind(pndr, pout);
	case 2:
		return nsp_ndr_push_nspiupdatestat(pndr, pout);
	case 3:
		return nsp_ndr_push_nspiqueryrows(pndr, pout);
	case 4:
		return nsp_ndr_push_nspiseekentries(pndr, pout);
	case 5:
		return nsp_ndr_push_nspigetmatches(pndr, pout);
	case 6:
		return nsp_ndr_push_nspiresortrestriction(pndr, pout);
	case 7:
		return nsp_ndr_push_nspidntomid(pndr, pout);
	case 8:
		return nsp_ndr_push_nspigetproplist(pndr, pout);
	case 9:
		return nsp_ndr_push_nspigetprops(pndr, pout);
	case 10:
		return nsp_ndr_push_nspicomparemids(pndr, pout);
	case 11:
		return nsp_ndr_push_nspimodprops(pndr, pout);
	case 12:
		return nsp_ndr_push_nspigetspecialtable(pndr, pout);
	case 13:
		return nsp_ndr_push_nspigettemplateinfo(pndr, pout);
	case 14:
		return nsp_ndr_push_nspimodlinkatt(pndr, pout);
	case 16:
		return nsp_ndr_push_nspiquerycolumns(pndr, pout);
	case 19:
		return nsp_ndr_push_nspiresolvenames(pndr, pout);
	case 20:
		return nsp_ndr_push_nspiresolvenamesw(pndr, pout);
	default:
		return NDR_ERR_BAD_SWITCH;
	}
}

static void exchange_nsp_unbind(uint64_t handle)
{
	nsp_interface_unbind_rpc_handle(handle);
}
