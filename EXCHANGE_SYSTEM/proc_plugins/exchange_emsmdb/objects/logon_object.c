#include "emsmdb_interface.h"
#include "msgchg_grouping.h"
#include "logon_object.h"
#include "exmdb_client.h"
#include "common_util.h"
#include "rop_util.h"
#include "util.h"
#include "guid.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HGROWING_SIZE									0x500

static BOOL logon_object_enlarge_propid_hash(
	LOGON_OBJECT *plogon)
{
	int tmp_id;
	void *ptmp_value;
	INT_HASH_ITER *iter;
	INT_HASH_TABLE *phash;
	
	phash = int_hash_init(plogon->ppropid_hash->capacity + 
				HGROWING_SIZE, sizeof(PROPERTY_NAME), NULL);
	if (NULL == phash) {
		return FALSE;
	}
	iter = int_hash_iter_init(plogon->ppropid_hash);
	for (int_hash_iter_begin(iter); !int_hash_iter_done(iter);
		int_hash_iter_forward(iter)) {
		ptmp_value = int_hash_iter_get_value(iter, &tmp_id);
		int_hash_add(phash, tmp_id, ptmp_value);
	}
	int_hash_iter_free(iter);
	int_hash_free(plogon->ppropid_hash);
	plogon->ppropid_hash = phash;
	return TRUE;
}

static BOOL logon_object_enlarge_propname_hash(
	LOGON_OBJECT *plogon)
{
	void *ptmp_value;
	STR_HASH_ITER *iter;
	char tmp_string[256];
	STR_HASH_TABLE *phash;
	
	phash = str_hash_init(plogon->ppropname_hash->capacity
				+ HGROWING_SIZE, sizeof(uint16_t), NULL);
	if (NULL == phash) {
		return FALSE;
	}
	iter = str_hash_iter_init(plogon->ppropname_hash);
	for (str_hash_iter_begin(iter); !str_hash_iter_done(iter);
		str_hash_iter_forward(iter)) {
		ptmp_value = str_hash_iter_get_value(iter, tmp_string);
		str_hash_add(phash, tmp_string, ptmp_value);
	}
	str_hash_iter_free(iter);
	str_hash_free(plogon->ppropname_hash);
	plogon->ppropname_hash = phash;
	return TRUE;
}

static BOOL logon_object_cache_propname(LOGON_OBJECT *plogon,
	uint16_t propid, const PROPERTY_NAME *ppropname)
{
	char tmp_guid[64];
	char tmp_string[256];
	PROPERTY_NAME tmp_name;
	
	if (NULL == plogon->ppropid_hash) {
		plogon->ppropid_hash = int_hash_init(
			HGROWING_SIZE, sizeof(PROPERTY_NAME), NULL);
		if (NULL == plogon->ppropid_hash) {
			return FALSE;
		}
	}
	if (NULL == plogon->ppropname_hash) {
		plogon->ppropname_hash = str_hash_init(
			HGROWING_SIZE, sizeof(uint16_t), NULL);
		if (NULL == plogon->ppropname_hash) {
			int_hash_free(plogon->ppropid_hash);
			return FALSE;
		}
	}
	tmp_name.kind = ppropname->kind;
	tmp_name.guid = ppropname->guid;
	guid_to_string(&ppropname->guid, tmp_guid, 64);
	switch (ppropname->kind) {
	case KIND_LID:
		tmp_name.plid = malloc(sizeof(uint32_t));
		if (NULL == tmp_name.plid) {
			return FALSE;
		}
		*tmp_name.plid = *ppropname->plid;
		tmp_name.pname = NULL;
		snprintf(tmp_string, 256, "%s:lid:%u", tmp_guid, *ppropname->plid);
		break;
	case KIND_NAME:
		tmp_name.plid = NULL;
		tmp_name.pname = strdup(ppropname->pname);
		if (NULL == tmp_name.pname) {
			return FALSE;
		}
		snprintf(tmp_string, 256, "%s:name:%s", tmp_guid, ppropname->pname);
		break;
	default:
		return FALSE;
	}
	if (NULL == int_hash_query(plogon->ppropid_hash, propid)) {
		if (1 != int_hash_add(plogon->ppropid_hash, propid, &tmp_name)) {
			if (FALSE == logon_object_enlarge_propid_hash(plogon) ||
				1 != int_hash_add(plogon->ppropid_hash, propid, &tmp_name)) {
				if (NULL != tmp_name.plid) {
					free(tmp_name.plid);
				}
				if (NULL != tmp_name.pname) {
					free(tmp_name.pname);
				}
				return FALSE;
			}
		}
	} else {
		if (NULL != tmp_name.plid) {
			free(tmp_name.plid);
		}
		if (NULL != tmp_name.pname) {
			free(tmp_name.pname);
		}
	}
	lower_string(tmp_string);
	if (NULL == str_hash_query(plogon->ppropname_hash, tmp_string)) {
		if (1 != str_hash_add(plogon->ppropname_hash, tmp_string, &propid)) {
			if (FALSE == logon_object_enlarge_propname_hash(plogon)
				|| 1 != str_hash_add(plogon->ppropname_hash,
				tmp_string, &propid)) {
				return FALSE;
			}
		}
	}
	return TRUE;
}

LOGON_OBJECT* logon_object_create(uint8_t logon_flags,
	uint32_t open_flags, int logon_mode, int account_id,
	const char *account, const char *dir, GUID mailbox_guid)
{
	LOGON_OBJECT *plogon;
	
	plogon = malloc(sizeof(LOGON_OBJECT));
	if (NULL == plogon) {
		return NULL;
	}
	plogon->logon_flags = logon_flags;
	plogon->open_flags = open_flags;
	plogon->logon_mode = logon_mode;
	plogon->account_id = account_id;
	strncpy(plogon->account, account, 256);
	strncpy(plogon->dir, dir, 256);
	plogon->mailbox_guid = mailbox_guid;
	plogon->pgpinfo = NULL;
	plogon->ppropid_hash = NULL;
	plogon->ppropname_hash = NULL;
	double_list_init(&plogon->group_list);
	return plogon;
}

void logon_object_free(LOGON_OBJECT *plogon)
{
	INT_HASH_ITER *piter;
	DOUBLE_LIST_NODE *pnode;
	PROPERTY_NAME *ppropname;
	
	if (NULL != plogon->pgpinfo) {
		property_groupinfo_free(plogon->pgpinfo);
	}
	while (pnode = double_list_get_from_head(&plogon->group_list)) {
		property_groupinfo_free(pnode->pdata);
		free(pnode);
	}
	double_list_free(&plogon->group_list);
	if (NULL != plogon->ppropid_hash) {
		piter = int_hash_iter_init(plogon->ppropid_hash);
		for (int_hash_iter_begin(piter); !int_hash_iter_done(piter);
			int_hash_iter_forward(piter)) {
			ppropname = int_hash_iter_get_value(piter, NULL);
			switch( ppropname->kind) {
			case KIND_LID:
				free(ppropname->plid);
				break;
			case KIND_NAME:
				free(ppropname->pname);
				break;
			}
		}
		int_hash_iter_free(piter);
		int_hash_free(plogon->ppropid_hash);
	}
	if (NULL != plogon->ppropname_hash) {
		str_hash_free(plogon->ppropname_hash);
	}
	free(plogon);
}

BOOL logon_object_check_private(LOGON_OBJECT *plogon)
{
	if (plogon->logon_flags & LOGON_FLAG_PRIVATE) {
		return TRUE;
	} else {
		return FALSE;
	}
}

int logon_object_get_mode(LOGON_OBJECT *plogon)
{
	return plogon->logon_mode;
}

int logon_object_get_account_id(LOGON_OBJECT *plogon)
{
	return plogon->account_id;
}

const char* logon_object_get_account(LOGON_OBJECT *plogon)
{
	return plogon->account;
}

const char* logon_object_get_dir(LOGON_OBJECT *plogon)
{
	return plogon->dir;
}

GUID logon_object_get_mailbox_guid(LOGON_OBJECT *plogon)
{
	return plogon->mailbox_guid;
}

BOOL logon_object_get_named_propname(LOGON_OBJECT *plogon,
	uint16_t propid, PROPERTY_NAME *ppropname)
{
	PROPERTY_NAME *pname;
	
	if (propid < 0x8000) {
		rop_util_get_common_pset(PS_MAPI, &ppropname->guid);
		ppropname->kind = KIND_LID;
		ppropname->plid = common_util_alloc(sizeof(uint32_t));
		if (NULL == ppropname->plid) {
			return FALSE;
		}
		*ppropname->plid = propid;
	}
	if (NULL != plogon->ppropid_hash) {
		pname = int_hash_query(plogon->ppropid_hash, propid);
		if (NULL != pname) {
			*ppropname = *pname;
			return TRUE;
		}
	}
	if (FALSE == exmdb_client_get_named_propname(
		plogon->dir, propid, ppropname)) {
		return FALSE;	
	}
	if (KIND_LID == ppropname->kind ||
		KIND_NAME == ppropname->kind) {
		logon_object_cache_propname(plogon, propid, ppropname);
	}
	return TRUE;
}

BOOL logon_object_get_named_propnames(LOGON_OBJECT *plogon,
	const PROPID_ARRAY *ppropids, PROPNAME_ARRAY *ppropnames)
{
	int i;
	int *pindex_map;
	PROPERTY_NAME *pname;
	PROPID_ARRAY tmp_propids;
	PROPNAME_ARRAY tmp_propnames;
	
	if (0 == ppropids->count) {
		ppropnames->count = 0;
		return TRUE;
	}
	pindex_map = common_util_alloc(ppropids->count*sizeof(int));
	if (NULL == pindex_map) {
		return FALSE;
	}
	ppropnames->ppropname = common_util_alloc(
		sizeof(PROPERTY_NAME)*ppropids->count);
	if (NULL == ppropnames->ppropname) {
		return FALSE;
	}
	ppropnames->count = ppropids->count;
	tmp_propids.count = 0;
	tmp_propids.ppropid = common_util_alloc(
			sizeof(uint16_t)*ppropids->count);
	if (NULL == tmp_propids.ppropid) {
		return FALSE;
	}
	for (i=0; i<ppropids->count; i++) {
		if (ppropids->ppropid[i] < 0x8000) {
			rop_util_get_common_pset(PS_MAPI,
				&ppropnames->ppropname[i].guid);
			ppropnames->ppropname[i].kind = KIND_LID;
			ppropnames->ppropname[i].plid =
				common_util_alloc(sizeof(uint32_t));
			if (NULL == ppropnames->ppropname[i].plid) {
				return FALSE;
			}
			*ppropnames->ppropname[i].plid = ppropids->ppropid[i];
			pindex_map[i] = i;
			continue;
		}
		if (NULL != plogon->ppropid_hash) {
			pname = int_hash_query(plogon->ppropid_hash,
								ppropids->ppropid[i]);
		} else {
			pname = NULL;
		}
		if (NULL != pname) {
			pindex_map[i] = i;
			ppropnames->ppropname[i] = *pname;
		} else {
			tmp_propids.ppropid[tmp_propids.count] =
								ppropids->ppropid[i];
			tmp_propids.count ++;
			pindex_map[i] = tmp_propids.count * (-1);
		}
	}
	if (0 == tmp_propids.count) {
		return TRUE;
	}
	if (FALSE == exmdb_client_get_named_propnames(
		plogon->dir, &tmp_propids, &tmp_propnames)) {
		return FALSE;	
	}
	for (i=0; i<ppropids->count; i++) {
		if (pindex_map[i] < 0) {
			ppropnames->ppropname[i] =
				tmp_propnames.ppropname[(-1)*pindex_map[i] - 1];
			if (KIND_LID == ppropnames->ppropname[i].kind ||
				KIND_NAME == ppropnames->ppropname[i].kind) {
				logon_object_cache_propname(plogon,
					ppropids->ppropid[i], ppropnames->ppropname + i);
			}
		}
	}
	return TRUE;
}

BOOL logon_object_get_named_propid(LOGON_OBJECT *plogon,
	BOOL b_create, const PROPERTY_NAME *ppropname,
	uint16_t *ppropid)
{
	GUID guid;
	uint16_t *pid;
	char tmp_guid[64];
	char tmp_string[256];
	
	rop_util_get_common_pset(PS_MAPI, &guid);
	if (0 == guid_compare(&ppropname->guid, &guid)) {
		if (KIND_LID == ppropname->kind) {
			*ppropid = *ppropname->plid;
		} else {
			*ppropid = 0;
		}
		return TRUE;
	}
	guid_to_string(&ppropname->guid, tmp_guid, 64);
	switch (ppropname->kind) {
	case KIND_LID:
		snprintf(tmp_string, 256, "%s:lid:%u", tmp_guid, *ppropname->plid);
		break;
	case KIND_NAME:
		snprintf(tmp_string, 256, "%s:name:%s", tmp_guid, ppropname->pname);
		lower_string(tmp_string);
		break;
	default:
		*ppropid = 0;
		return TRUE;
	}
	if (NULL != plogon->ppropname_hash) {
		pid = str_hash_query(plogon->ppropname_hash, tmp_string);
		if (NULL != pid) {
			*ppropid = *pid;
			return TRUE;
		}
	}
	if (FALSE == exmdb_client_get_named_propid(
		plogon->dir, b_create, ppropname, ppropid)) {
		return FALSE;
	}
	if (0 == *ppropid) {
		return TRUE;
	}
	logon_object_cache_propname(plogon, *ppropid, ppropname);
	return TRUE;
}

BOOL logon_object_get_named_propids(LOGON_OBJECT *plogon,
	BOOL b_create, const PROPNAME_ARRAY *ppropnames,
	PROPID_ARRAY *ppropids)
{
	int i;
	GUID guid;
	uint16_t *pid;
	int *pindex_map;
	char tmp_guid[64];
	char tmp_string[256];
	PROPID_ARRAY tmp_propids;
	PROPNAME_ARRAY tmp_propnames;
	
	if (0 == ppropnames->count) {
		ppropids->count = 0;
		return TRUE;
	}
	rop_util_get_common_pset(PS_MAPI, &guid);
	pindex_map = common_util_alloc(ppropnames->count*sizeof(int));
	if (NULL == pindex_map) {
		return FALSE;
	}
	ppropids->count = ppropnames->count;
	ppropids->ppropid = common_util_alloc(
		sizeof(uint16_t)*ppropnames->count);
	if (NULL == ppropids->ppropid) {
		return FALSE;
	}
	tmp_propnames.count = 0;
	tmp_propnames.ppropname = common_util_alloc(
		sizeof(PROPERTY_NAME)*ppropnames->count);
	if (NULL == tmp_propnames.ppropname) {
		return FALSE;
	}
	for (i=0; i<ppropnames->count; i++) {
		if (0 == guid_compare(&ppropnames->ppropname[i].guid, &guid)) {
			if (KIND_LID == ppropnames->ppropname[i].kind) {
				ppropids->ppropid[i] = *ppropnames->ppropname[i].plid;
			} else {
				ppropids->ppropid[i] = 0;
			}
			pindex_map[i] = i;
			continue;
		}
		guid_to_string(&ppropnames->ppropname[i].guid, tmp_guid, 64);
		switch (ppropnames->ppropname[i].kind) {
		case KIND_LID:
			snprintf(tmp_string, 256, "%s:lid:%u",
				tmp_guid, *ppropnames->ppropname[i].plid);
			break;
		case KIND_NAME:
			snprintf(tmp_string, 256, "%s:name:%s",
				tmp_guid, ppropnames->ppropname[i].pname);
			lower_string(tmp_string);
			break;
		default:
			ppropids->ppropid[i] = 0;
			pindex_map[i] = i;
			continue;
		}
		if (NULL != plogon->ppropname_hash) {
			pid = str_hash_query(plogon->ppropname_hash, tmp_string);
		} else {
			pid = NULL;
		}
		if (NULL != pid) {
			pindex_map[i] = i;
			ppropids->ppropid[i] = *pid;
		} else {
			tmp_propnames.ppropname[tmp_propnames.count] =
									ppropnames->ppropname[i];
			tmp_propnames.count ++;
			pindex_map[i] = tmp_propnames.count * (-1);
		}
	}
	if (0 == tmp_propnames.count) {
		return TRUE;
	}
	if (FALSE == exmdb_client_get_named_propids(plogon->dir,
		b_create, &tmp_propnames, &tmp_propids)) {
		return FALSE;	
	}
	for (i=0; i<ppropnames->count; i++) {
		if (pindex_map[i] < 0) {
			ppropids->ppropid[i] =
				tmp_propids.ppropid[(-1)*pindex_map[i] - 1];
			if (0 != ppropids->ppropid[i]) {
				logon_object_cache_propname(plogon,
					ppropids->ppropid[i], ppropnames->ppropname + i);
			}
		}
	}
	return TRUE;
}

PROPERTY_GROUPINFO* logon_object_get_last_property_groupinfo(
	LOGON_OBJECT *plogon)
{
	if (NULL == plogon->pgpinfo) {
		plogon->pgpinfo = msgchg_grouping_get_groupinfo(
			plogon, msgchg_grouping_get_last_group_id());
	}
	return plogon->pgpinfo;
}

PROPERTY_GROUPINFO* logon_object_get_property_groupinfo(
	LOGON_OBJECT *plogon, uint32_t group_id)
{
	DOUBLE_LIST_NODE *pnode;
	PROPERTY_GROUPINFO *pgpinfo;
	
	if (group_id == msgchg_grouping_get_last_group_id()) {
		return logon_object_get_last_property_groupinfo(plogon);
	}
	for (pnode=double_list_get_head(&plogon->group_list); NULL!=pnode;
		pnode=double_list_get_after(&plogon->group_list, pnode)) {
		pgpinfo = (PROPERTY_GROUPINFO*)pnode->pdata;
		if (pgpinfo->group_id == group_id) {
			return pgpinfo;
		}
	}
	pnode = malloc(sizeof(DOUBLE_LIST_NODE));
	if (NULL == pnode) {
		return NULL;
	}
	pgpinfo = msgchg_grouping_get_groupinfo(plogon, group_id);
	if (NULL == pgpinfo) {
		free(pnode);
		return NULL;
	}
	pnode->pdata = pgpinfo;
	double_list_append_as_tail(&plogon->group_list, pnode);
	return pgpinfo;
}

BOOL logon_object_get_all_proptags(LOGON_OBJECT *plogon,
	PROPTAG_ARRAY *pproptags)
{
	PROPTAG_ARRAY tmp_proptags;
	
	if (FALSE == exmdb_client_get_store_all_proptags(
		plogon->dir, &tmp_proptags)) {
		return FALSE;	
	}
	pproptags->pproptag = common_util_alloc(
		sizeof(uint32_t)*(tmp_proptags.count + 25));
	if (NULL == pproptags->pproptag) {
		return FALSE;
	}
	memcpy(pproptags->pproptag, tmp_proptags.pproptag,
				sizeof(uint32_t)*tmp_proptags.count);
	pproptags->count = tmp_proptags.count;
	if (TRUE == logon_object_check_private(plogon)) {
		pproptags->pproptag[pproptags->count] =
					PROP_TAG_MAILBOXOWNERNAME;
		pproptags->count ++;
		pproptags->pproptag[pproptags->count] =
					PROP_TAG_MAILBOXOWNERENTRYID;
		pproptags->count ++;
		pproptags->pproptag[pproptags->count] =
				PROP_TAG_MAXIMUMSUBMITMESSAGESIZE;
		pproptags->count ++;
		pproptags->pproptag[pproptags->count] =
						PROP_TAG_EMAILADDRESS;
		pproptags->count ++;
		pproptags->pproptag[pproptags->count] =
		PROP_TAG_ADDRESSBOOKDISPLAYNAMEPRINTABLE;
		pproptags->count ++;
	} else {
		pproptags->pproptag[pproptags->count] =
						PROP_TAG_HIERARCHYSERVER;
		pproptags->count ++;
		/* TODO!!! for PROP_TAG_EMAILADDRESS
			check if mail address of public folder exits! */
	}
	pproptags->pproptag[pproptags->count] =
			PROP_TAG_DELETEDASSOCMESSAGESIZE;
	pproptags->count ++;
	pproptags->pproptag[pproptags->count] =
	PROP_TAG_DELETEDASSOCMESSAGESIZEEXTENDED;
	pproptags->count ++;
	pproptags->pproptag[pproptags->count] =
			PROP_TAG_DELETEDASSOCMSGCOUNT;
	pproptags->count ++;
	pproptags->pproptag[pproptags->count] =
				PROP_TAG_DELETEDMESSAGESIZE;
	pproptags->count ++;
	pproptags->pproptag[pproptags->count] =
		PROP_TAG_DELETEDMESSAGESIZEEXTENDED;
	pproptags->count ++;
	pproptags->pproptag[pproptags->count] =
					PROP_TAG_DELETEDMSGCOUNT;
	pproptags->count ++;
	pproptags->pproptag[pproptags->count] =
		PROP_TAG_DELETEDNORMALMESSAGESIZE;
	pproptags->count ++;
	pproptags->pproptag[pproptags->count] =
		PROP_TAG_DELETEDNORMALMESSAGESIZEEXTENDED;
	pproptags->count ++;
	pproptags->pproptag[pproptags->count] =
			PROP_TAG_EXTENDEDRULESIZELIMIT;
	pproptags->count ++;
	pproptags->pproptag[pproptags->count] =
				PROP_TAG_ASSOCMESSAGESIZE;
	pproptags->count ++;
	pproptags->pproptag[pproptags->count] =
					PROP_TAG_MESSAGESIZE;
	pproptags->count ++;
	pproptags->pproptag[pproptags->count] =
				PROP_TAG_NORMALMESSAGESIZE;
	pproptags->count ++;
	pproptags->pproptag[pproptags->count] =
						PROP_TAG_USERENTRYID;
	pproptags->count ++;
	pproptags->pproptag[pproptags->count] =
					PROP_TAG_CONTENTCOUNT;
	pproptags->count ++;
	pproptags->pproptag[pproptags->count] =
			PROP_TAG_ASSOCIATEDCONTENTCOUNT;
	pproptags->count ++;
	pproptags->pproptag[pproptags->count] =
					PROP_TAG_TESTLINESPEED;
	pproptags->count ++;
	return TRUE;
}

static BOOL logon_object_check_readonly_property(
	LOGON_OBJECT *plogon, uint32_t proptag)
{
	if (PROPVAL_TYPE_OBJECT == (proptag & 0xFFFF)) {
		return TRUE;
	}
	switch (proptag) {
	case PROP_TAG_ACCESSLEVEL:
	case PROP_TAG_ADDRESSBOOKDISPLAYNAMEPRINTABLE:
	case PROP_TAG_ADDRESSBOOKDISPLAYNAMEPRINTABLE_STRING8:
	case PROP_TAG_CODEPAGEID:
	case PROP_TAG_CONTENTCOUNT:
	case PROP_TAG_DELETEAFTERSUBMIT:
	case PROP_TAG_DELETEDASSOCMESSAGESIZE:
	case PROP_TAG_DELETEDASSOCMESSAGESIZEEXTENDED:
	case PROP_TAG_DELETEDASSOCMSGCOUNT:
	case PROP_TAG_DELETEDMESSAGESIZE:
	case PROP_TAG_DELETEDMESSAGESIZEEXTENDED:
	case PROP_TAG_DELETEDMSGCOUNT:
	case PROP_TAG_DELETEDNORMALMESSAGESIZE:
	case PROP_TAG_DELETEDNORMALMESSAGESIZEEXTENDED:
	case PROP_TAG_EMAILADDRESS:
	case PROP_TAG_EMAILADDRESS_STRING8:
	case PROP_TAG_EXTENDEDRULESIZELIMIT:
	case PROP_TAG_INTERNETARTICLENUMBER:
	case PROP_TAG_LOCALEID:
	case PROP_TAG_MAXIMUMSUBMITMESSAGESIZE:
	case PROP_TAG_MAILBOXOWNERENTRYID:
	case PROP_TAG_MAILBOXOWNERNAME:
	case PROP_TAG_MAILBOXOWNERNAME_STRING8:
	case PROP_TAG_MESSAGESIZE:
	case PROP_TAG_MESSAGESIZEEXTENDED:
	case PROP_TAG_ASSOCMESSAGESIZE:
	case PROP_TAG_ASSOCMESSAGESIZEEXTENDED:
	case PROP_TAG_NORMALMESSAGESIZE:
	case PROP_TAG_NORMALMESSAGESIZEEXTENDED:
	case PROP_TAG_OBJECTTYPE:
	case PROP_TAG_OUTOFOFFICESTATE:
	case PROP_TAG_PROHIBITRECEIVEQUOTA:
	case PROP_TAG_PROHIBITSENDQUOTA:
	case PROP_TAG_RECORDKEY:
	case PROP_TAG_SEARCHKEY:
	case PROP_TAG_SORTLOCALEID:
	case PROP_TAG_STORAGEQUOTALIMIT:
	case PROP_TAG_STOREENTRYID:
	case PROP_TAG_STOREOFFLINE:
	case PROP_TAG_STOREPROVIDER:
	case PROP_TAG_STORERECORDKEY:
	case PROP_TAG_STORESTATE:
	case PROP_TAG_STORESUPPORTMASK:
	case PROP_TAG_TESTLINESPEED:
	case PROP_TAG_USERENTRYID:
	case PROP_TAG_VALIDFOLDERMASK:
	case PROP_TAG_HIERARCHYSERVER:
		return TRUE;
	}
	return FALSE;
}

static BOOL logon_object_get_calculated_property(
	LOGON_OBJECT *plogon, uint32_t proptag, void **ppvalue)
{
	int i;
	int temp_len;
	void *pvalue;
	EMSMDB_INFO *pinfo;
	char temp_buff[1024];
	DCERPC_INFO rpc_info;
	static uint64_t tmp_ll = 0;
	static uint8_t test_buff[256];
	static BINARY test_bin = {256, test_buff};
	
	switch (proptag) {
	case PROP_TAG_MESSAGESIZE:
		*ppvalue = common_util_alloc(sizeof(uint32_t));
		if (NULL == *ppvalue) {
			return FALSE;
		}
		if (FALSE == exmdb_client_get_store_property(
			plogon->dir, 0, PROP_TAG_MESSAGESIZEEXTENDED,
			&pvalue) || NULL == pvalue) {
			return FALSE;	
		}
		if (*(uint64_t*)pvalue > 0x7FFFFFFF) {
			**(uint32_t**)ppvalue = 0x7FFFFFFF;
		} else {
			**(uint32_t**)ppvalue = *(uint64_t*)pvalue;
		}
		return TRUE;
	case PROP_TAG_ASSOCMESSAGESIZE:
		*ppvalue = common_util_alloc(sizeof(uint32_t));
		if (NULL == *ppvalue) {
			return FALSE;
		}
		if (FALSE == exmdb_client_get_store_property(
			plogon->dir, 0, PROP_TAG_ASSOCMESSAGESIZEEXTENDED,
			&pvalue) || NULL == pvalue) {
			return FALSE;	
		}
		if (*(uint64_t*)pvalue > 0x7FFFFFFF) {
			**(uint32_t**)ppvalue = 0x7FFFFFFF;
		} else {
			**(uint32_t**)ppvalue = *(uint64_t*)pvalue;
		}
		return TRUE;
	case PROP_TAG_NORMALMESSAGESIZE:
		*ppvalue = common_util_alloc(sizeof(uint32_t));
		if (NULL == *ppvalue) {
			return FALSE;
		}
		if (FALSE == exmdb_client_get_store_property(
			plogon->dir, 0, PROP_TAG_NORMALMESSAGESIZEEXTENDED,
			&pvalue) || NULL == pvalue) {
			return FALSE;	
		}
		if (*(uint64_t*)pvalue > 0x7FFFFFFF) {
			**(uint32_t**)ppvalue = 0x7FFFFFFF;
		} else {
			**(uint32_t**)ppvalue = *(uint64_t*)pvalue;
		}
		return TRUE;
	case PROP_TAG_ADDRESSBOOKDISPLAYNAMEPRINTABLE:
	case PROP_TAG_ADDRESSBOOKDISPLAYNAMEPRINTABLE_STRING8:
		if (FALSE == logon_object_check_private(plogon)) {
			return FALSE;
		}
		*ppvalue = common_util_alloc(256);
		if (NULL == *ppvalue) {
			return FALSE;
		}
		if (FALSE == common_util_get_user_displayname(
			plogon->account, *ppvalue)) {
			return FALSE;	
		}
		temp_len = strlen(*ppvalue);
		for (i=0; i<temp_len; i++) {
			if (0 == isascii(((char*)(*ppvalue))[i])) {
				strcpy(*ppvalue, plogon->account);
				pvalue = strchr(*ppvalue, '@');
				*(char*)pvalue = '\0';
				break;
			}
		}
		return TRUE;
	case PROP_TAG_CODEPAGEID:
		pinfo = emsmdb_interface_get_emsmdb_info();
		*ppvalue = &pinfo->cpid;
		return TRUE;
	case PROP_TAG_DELETEDASSOCMESSAGESIZE:
	case PROP_TAG_DELETEDASSOCMESSAGESIZEEXTENDED:
	case PROP_TAG_DELETEDASSOCMSGCOUNT:
	case PROP_TAG_DELETEDMESSAGESIZE:
	case PROP_TAG_DELETEDMESSAGESIZEEXTENDED:
	case PROP_TAG_DELETEDMSGCOUNT:
	case PROP_TAG_DELETEDNORMALMESSAGESIZE:
	case PROP_TAG_DELETEDNORMALMESSAGESIZEEXTENDED:
		*ppvalue = &tmp_ll;
		return TRUE;
	case PROP_TAG_EMAILADDRESS:
	case PROP_TAG_EMAILADDRESS_STRING8:
		if (TRUE == logon_object_check_private(plogon)) {
			if (FALSE == common_util_username_to_essdn(
				plogon->account, temp_buff)) {
				return FALSE;	
			}
		} else {
			if (FALSE == common_util_public_to_essdn(
				plogon->account, temp_buff)) {
				return FALSE;	
			}
		}
		*ppvalue = common_util_alloc(strlen(temp_buff) + 1);
		if (NULL == *ppvalue) {
			return FALSE;
		}
		strcpy(*ppvalue, temp_buff);
		return TRUE;
	case PROP_TAG_EXTENDEDRULESIZELIMIT:
		*ppvalue = common_util_alloc(sizeof(uint32_t));
		if (NULL == *ppvalue) {
			return FALSE;
		}
		*(uint32_t*)(*ppvalue) = common_util_get_param(
						COMMON_UTIL_MAX_EXTRULE_LENGTH);
		return TRUE;
	case PROP_TAG_HIERARCHYSERVER:
		if (TRUE == logon_object_check_private(plogon)) {
			return FALSE;
		}
		common_util_get_domain_server(plogon->account, temp_buff);
		*ppvalue = common_util_alloc(strlen(temp_buff) + 1);
		if (NULL == *ppvalue) {
			return FALSE;
		}
		strcpy(*ppvalue, temp_buff);
		return TRUE;
	case PROP_TAG_LOCALEID:
		pinfo = emsmdb_interface_get_emsmdb_info();
		*ppvalue = &pinfo->lcid_string;
		return TRUE;
	case PROP_TAG_MAILBOXOWNERENTRYID:
		if (FALSE == logon_object_check_private(plogon)) {
			return FALSE;
		}
		*ppvalue = common_util_username_to_addressbook_entryid(
												plogon->account);
		if (NULL == *ppvalue) {
			return FALSE;
		}
		return TRUE;
	case PROP_TAG_MAILBOXOWNERNAME:
		if (FALSE == logon_object_check_private(plogon)) {
			return FALSE;
		}
		if (FALSE == common_util_get_user_displayname(
			plogon->account, temp_buff)) {
			return FALSE;	
		}
		if ('\0' == temp_buff[0]) {
			*ppvalue = common_util_alloc(strlen(plogon->account) + 1);
			if (NULL == *ppvalue) {
				return FALSE;
			}
			strcpy(*ppvalue, plogon->account);
		} else {
			*ppvalue = common_util_alloc(strlen(temp_buff) + 1);
			if (NULL == *ppvalue) {
				return FALSE;
			}
			strcpy(*ppvalue, temp_buff);
		}
		return TRUE;
	case PROP_TAG_MAILBOXOWNERNAME_STRING8:
		if (FALSE == logon_object_check_private(plogon)) {
			return FALSE;
		}
		if (FALSE == common_util_get_user_displayname(
			plogon->account, temp_buff)) {
			return FALSE;	
		}
		temp_len = 2*strlen(temp_buff) + 1;
		*ppvalue = common_util_alloc(temp_len);
		if (NULL == *ppvalue) {
			return FALSE;
		}
		if (common_util_convert_string(FALSE,
			temp_buff, *ppvalue, temp_len) < 0) {
			return FALSE;	
		}
		if ('\0' == ((char*)*ppvalue)[0]) {
			strcpy(*ppvalue, plogon->account);
		}
		return TRUE;
	case PROP_TAG_MAXIMUMSUBMITMESSAGESIZE:
		*ppvalue = common_util_alloc(sizeof(uint32_t));
		if (NULL == *ppvalue) {
			return FALSE;
		}
		*(uint32_t*)(*ppvalue) = common_util_get_param(
							COMMON_UTIL_MAX_MAIL_LENGTH);
		return TRUE;
	case PROP_TAG_SORTLOCALEID:
		pinfo = emsmdb_interface_get_emsmdb_info();
		*ppvalue = &pinfo->lcid_sort;
		return TRUE;
	case PROP_TAG_STORERECORDKEY:
		*ppvalue = common_util_guid_to_binary(plogon->mailbox_guid);
		return TRUE;
	case PROP_TAG_USERENTRYID:
		rpc_info = get_rpc_info();
		*ppvalue = common_util_username_to_addressbook_entryid(
											rpc_info.username);
		if (NULL == *ppvalue) {
			return FALSE;
		}
		return TRUE;
	case PROP_TAG_TESTLINESPEED:
		*ppvalue = &test_bin;
		return TRUE;
	}
	return FALSE;
}

BOOL logon_object_get_properties(LOGON_OBJECT *plogon,
	const PROPTAG_ARRAY *pproptags, TPROPVAL_ARRAY *ppropvals)
{
	int i;
	void *pvalue;
	EMSMDB_INFO *pinfo;
	PROPTAG_ARRAY tmp_proptags;
	TPROPVAL_ARRAY tmp_propvals;
	static uint32_t err_code = EC_ERROR;
	
	pinfo = emsmdb_interface_get_emsmdb_info();
	if (NULL == pinfo) {
		return FALSE;
	}
	ppropvals->ppropval = common_util_alloc(
		sizeof(TAGGED_PROPVAL)*pproptags->count);
	if (NULL == ppropvals->ppropval) {
		return FALSE;
	}
	tmp_proptags.count = 0;
	tmp_proptags.pproptag = common_util_alloc(
			sizeof(uint32_t)*pproptags->count);
	if (NULL == tmp_proptags.pproptag) {
		return FALSE;
	}
	ppropvals->count = 0;
	for (i=0; i<pproptags->count; i++) {
		if (TRUE == logon_object_get_calculated_property(
			plogon, pproptags->pproptag[i], &pvalue)) {
			if (NULL != pvalue) {
				ppropvals->ppropval[ppropvals->count].proptag =
											pproptags->pproptag[i];
				ppropvals->ppropval[ppropvals->count].pvalue = pvalue;
			} else {
				ppropvals->ppropval[ppropvals->count].proptag =
					(pproptags->pproptag[i]&0xFFFF0000)|PROPVAL_TYPE_ERROR;
				ppropvals->ppropval[ppropvals->count].pvalue = &err_code;
			}
			ppropvals->count ++;
		} else {
			tmp_proptags.pproptag[tmp_proptags.count] =
											pproptags->pproptag[i];
			tmp_proptags.count ++;
		}
	}
	if (0 == tmp_proptags.count) {
		return TRUE;
	}
	if (FALSE == exmdb_client_get_store_properties(plogon->dir,
		pinfo->cpid, &tmp_proptags, &tmp_propvals)) {
		return FALSE;	
	}
	if (0 == tmp_propvals.count) {
		return TRUE;
	}
	memcpy(ppropvals->ppropval + ppropvals->count,
		tmp_propvals.ppropval,
		sizeof(TAGGED_PROPVAL)*tmp_propvals.count);
	ppropvals->count += tmp_propvals.count;
	return TRUE;	
}

BOOL logon_object_set_properties(LOGON_OBJECT *plogon,
	const TPROPVAL_ARRAY *ppropvals, PROBLEM_ARRAY *pproblems)
{
	int i;
	EMSMDB_INFO *pinfo;
	PROBLEM_ARRAY tmp_problems;
	PROPERTY_PROBLEM *pproblem;
	TPROPVAL_ARRAY tmp_propvals;
	uint16_t *poriginal_indices;
	
	pinfo = emsmdb_interface_get_emsmdb_info();
	if (NULL == pinfo) {
		return FALSE;
	}
	pproblems->count = 0;
	pproblems->pproblem = common_util_alloc(
		sizeof(PROPERTY_PROBLEM)*ppropvals->count);
	if (NULL == pproblems->pproblem) {
		return FALSE;
	}
	tmp_propvals.count = 0;
	tmp_propvals.ppropval = common_util_alloc(
		sizeof(TAGGED_PROPVAL)*ppropvals->count);
	if (NULL == tmp_propvals.ppropval) {
		return FALSE;
	}
	poriginal_indices = common_util_alloc(
		sizeof(uint16_t)*ppropvals->count);
	if (NULL == poriginal_indices) {
		return FALSE;
	}
	for (i=0; i<ppropvals->count; i++) {
		if (TRUE == logon_object_check_readonly_property(
			plogon, ppropvals->ppropval[i].proptag)) {
			pproblems->pproblem[pproblems->count].index = i;
			pproblems->pproblem[pproblems->count].proptag =
							ppropvals->ppropval[i].proptag;
			pproblems->pproblem[pproblems->count].err = 
										EC_ACCESS_DENIED;
			pproblems->count ++;
		} else {
			tmp_propvals.ppropval[tmp_propvals.count] =
									ppropvals->ppropval[i];
			poriginal_indices[tmp_propvals.count] = i;
			tmp_propvals.count ++;
		}
	}
	if (0 == tmp_propvals.count) {
		return TRUE;
	}
	if (FALSE == exmdb_client_set_store_properties(plogon->dir,
		pinfo->cpid, &tmp_propvals, &tmp_problems)) {
		return FALSE;	
	}
	if (0 == tmp_problems.count) {
		return TRUE;
	}
	for (i=0; i<tmp_problems.count; i++) {
		tmp_problems.pproblem[i].index =
			poriginal_indices[tmp_problems.pproblem[i].index];
	}
	memcpy(pproblems->pproblem + pproblems->count,
		tmp_problems.pproblem, tmp_problems.count*
		sizeof(PROPERTY_PROBLEM));
	pproblems->count += tmp_problems.count;
	qsort(pproblems->pproblem, pproblems->count,
		sizeof(PROPERTY_PROBLEM), common_util_problem_compare);
	return TRUE;
}

BOOL logon_object_remove_properties(LOGON_OBJECT *plogon,
	const PROPTAG_ARRAY *pproptags, PROBLEM_ARRAY *pproblems)
{
	int i;
	PROPERTY_PROBLEM *pproblem;
	PROPTAG_ARRAY tmp_proptags;
	
	pproblems->count = 0;
	pproblems->pproblem = common_util_alloc(
		sizeof(PROPERTY_PROBLEM)*pproptags->count);
	if (NULL == pproblems->pproblem) {
		return FALSE;
	}
	tmp_proptags.count = 0;
	tmp_proptags.pproptag = common_util_alloc(
		sizeof(uint32_t)*pproptags->count);
	if (NULL == tmp_proptags.pproptag) {
		return FALSE;
	}
	for (i=0; i<pproptags->count; i++) {
		if (TRUE == logon_object_check_readonly_property(
			plogon, pproptags->pproptag[i])) {
			pproblems->pproblem[pproblems->count].index = i;
			pproblems->pproblem[pproblems->count].proptag =
									pproptags->pproptag[i];
			pproblems->pproblem[pproblems->count].err = 
										EC_ACCESS_DENIED;
			pproblems->count ++;
		} else {
			tmp_proptags.pproptag[tmp_proptags.count] =
									pproptags->pproptag[i];
			tmp_proptags.count ++;
		}
	}
	if (0 == tmp_proptags.count) {
		return TRUE;
	}
	if (FALSE == exmdb_client_remove_store_properties(
		plogon->dir, &tmp_proptags)) {
		return FALSE;	
	}
	return TRUE;
}
