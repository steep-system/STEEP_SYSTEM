#include "rops.h"
#include "propval.h"
#include "common_util.h"
#include "proc_common.h"
#include "exmdb_client.h"
#include "logon_object.h"
#include "folder_object.h"
#include "stream_object.h"
#include "rop_processor.h"
#include "message_object.h"
#include "processor_types.h"
#include "emsmdb_interface.h"
#include "attachment_object.h"


uint32_t rop_getpropertyidsfromnames(uint8_t flags,
	const PROPNAME_ARRAY *ppropnames, PROPID_ARRAY *ppropids,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	BOOL b_create;
	int object_type;
	LOGON_OBJECT *plogon;
	
	plogon = rop_processor_get_logon_object(plogmap, logon_id);
	if (NULL == plogon) {
		return EC_ERROR;
	}
	if (NULL == rop_processor_get_object(
		plogmap, logon_id, hin, &object_type)) {
		return EC_NULL_OBJECT;
	}
	switch (object_type) {
	case OBJECT_TYPE_LOGON:
	case OBJECT_TYPE_FOLDER:
	case OBJECT_TYPE_MESSAGE:
	case OBJECT_TYPE_ATTACHMENT:
		break;
	default:
		return EC_NOT_SUPPORTED;
	}
	if (PROPIDS_FROM_NAMES_FLAG_GETORCREATE == flags) {
		if (TRUE == logon_object_check_private(plogon) &&
			LOGON_MODE_GUEST == logon_object_get_mode(plogon)) {
			b_create = FALSE;
		} else {
			b_create = TRUE;
		}
	} else if (PROPIDS_FROM_NAMES_FLAG_GETONLY == flags) {
		b_create = FALSE;
	} else {
		return EC_INVALID_PARAMETER;
	}
	if (0 == ppropnames->count &&
		OBJECT_TYPE_LOGON == object_type) {
		if (FALSE == exmdb_client_get_all_named_propids(
			logon_object_get_dir(plogon), ppropids)) {
			return EC_ERROR;	
		}
		return EC_SUCCESS;
	}
	if (FALSE == logon_object_get_named_propids(
		plogon, b_create, ppropnames, ppropids)) {
		return EC_ERROR;
	}
	return EC_SUCCESS;
}

uint32_t rop_getnamesfrompropertyids(
	const PROPID_ARRAY *ppropids, PROPNAME_ARRAY *ppropnames,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	int object_type;
	LOGON_OBJECT *plogon;
	
	plogon = rop_processor_get_logon_object(plogmap, logon_id);
	if (NULL == plogon) {
		return EC_ERROR;
	}
	if (NULL == rop_processor_get_object(
		plogmap, logon_id, hin, &object_type)) {
		return EC_NULL_OBJECT;
	}
	switch (object_type) {
	case OBJECT_TYPE_LOGON:
	case OBJECT_TYPE_FOLDER:
	case OBJECT_TYPE_MESSAGE:
	case OBJECT_TYPE_ATTACHMENT:
		if (FALSE == logon_object_get_named_propnames(
			plogon, ppropids, ppropnames)) {
			return EC_ERROR;	
		}
		return EC_SUCCESS;
	default:
		return EC_NOT_SUPPORTED;
	}
}

uint32_t rop_getpropertiesspecific(uint16_t size_limit,
	uint16_t want_unicode, const PROPTAG_ARRAY *pproptags,
	PROPERTY_ROW *prow, void *plogmap, uint8_t logon_id,
	uint32_t hin)
{
	int i;
	uint32_t cpid;
	void *pobject;
	BOOL b_unicode;
	int object_type;
	uint32_t tmp_size;
	uint16_t proptype;
	EMSMDB_INFO *pinfo;
	uint32_t total_size;
	TPROPVAL_ARRAY propvals;
	PROPTAG_ARRAY *ptmp_proptags;
	
	/* we ignore the size_limit as
		mentioned in MS-OXCPRPT 3.2.5.1 */
	pobject = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pobject) {
		return EC_NULL_OBJECT;
	}
	if (0 == want_unicode) {
		b_unicode = FALSE;
	} else {
		b_unicode = TRUE;
	}
	ptmp_proptags = common_util_trim_proptags(pproptags);
	if (NULL == ptmp_proptags) {
		return EC_OUT_OF_MEMORY;
	}
	switch (object_type) {
	case OBJECT_TYPE_LOGON:
		if (FALSE == logon_object_get_properties(
			pobject, ptmp_proptags, &propvals)) {
			return EC_ERROR;	
		}
		pinfo = emsmdb_interface_get_emsmdb_info();
		if (NULL == pinfo) {
			return EC_ERROR;
		}
		cpid = pinfo->cpid;
		break;
	case OBJECT_TYPE_FOLDER:
		if (FALSE == folder_object_get_properties(
			pobject, ptmp_proptags, &propvals)) {
			return EC_ERROR;	
		}
		pinfo = emsmdb_interface_get_emsmdb_info();
		if (NULL == pinfo) {
			return EC_ERROR;
		}
		cpid = pinfo->cpid;
		break;
	case OBJECT_TYPE_MESSAGE:
		if (FALSE == message_object_get_properties(
			pobject, 0, ptmp_proptags, &propvals)) {
			return EC_ERROR;	
		}
		cpid = message_object_get_cpid(pobject);
		break;
	case OBJECT_TYPE_ATTACHMENT:
		if (FALSE == attachment_object_get_properties(
			pobject, 0, ptmp_proptags, &propvals)) {
			return EC_ERROR;	
		}
		cpid = attachment_object_get_cpid(pobject);
		break;
	default:
		return EC_NOT_SUPPORTED;	
	}
	total_size = 0;
	for (i=0; i<propvals.count; i++) {
		tmp_size = propval_size(
			propvals.ppropval[i].proptag & 0xFFFF,
			propvals.ppropval[i].pvalue);
		if (tmp_size > 0x8000) {
			propvals.ppropval[i].proptag &= 0xFFFF0000;
			propvals.ppropval[i].proptag |= PROPVAL_TYPE_ERROR;
			propvals.ppropval[i].pvalue =
				common_util_alloc(sizeof(uint32_t));
			if (NULL == propvals.ppropval[i].pvalue) {
				return EC_OUT_OF_MEMORY;
			}
			*(uint32_t*)propvals.ppropval[i].pvalue =
									EC_OUT_OF_MEMORY;
			continue;
		}
		total_size += tmp_size;
	}
	if (total_size > 0x7000) {
		for (i=0; i<propvals.count; i++) {
			proptype = propvals.ppropval[i].proptag & 0xFFFF;
			switch (proptype) {
			case PROPVAL_TYPE_BINARY:
			case PROPVAL_TYPE_OBJECT:
			case PROPVAL_TYPE_STRING:
			case PROPVAL_TYPE_WSTRING:
				if (0x1000 < propval_size(proptype,
					propvals.ppropval[i].pvalue)) {
					propvals.ppropval[i].proptag &= 0xFFFF0000;
					propvals.ppropval[i].proptag |= PROPVAL_TYPE_ERROR;
					propvals.ppropval[i].pvalue =
						common_util_alloc(sizeof(uint32_t));
					if (NULL == propvals.ppropval[i].pvalue) {
						return EC_OUT_OF_MEMORY;
					}
					*(uint32_t*)propvals.ppropval[i].pvalue =
											EC_OUT_OF_MEMORY;
				}
				break;
			}
		}
	}
	if (FALSE == common_util_propvals_to_row_ex(
		cpid, b_unicode, &propvals, pproptags, prow)) {
		return EC_OUT_OF_MEMORY;	
	}
	return EC_SUCCESS;
}

uint32_t rop_getpropertiesall(uint16_t size_limit,
	uint16_t want_unicode, TPROPVAL_ARRAY *ppropvals,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	int i;
	uint32_t cpid;
	void *pobject;
	BOOL b_unicode;
	int object_type;
	EMSMDB_INFO *pinfo;
	PROPTAG_ARRAY proptags;
	PROPTAG_ARRAY *ptmp_proptags;
	
	pobject = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pobject) {
		return EC_NULL_OBJECT;
	}
	switch (object_type) {
	case OBJECT_TYPE_LOGON:
		if (FALSE == logon_object_get_all_proptags(
			pobject, &proptags)) {
			return EC_ERROR;
		}
		ptmp_proptags = common_util_trim_proptags(&proptags);
		if (NULL == ptmp_proptags) {
			return EC_OUT_OF_MEMORY;
		}
		if (FALSE == logon_object_get_properties(
			pobject, ptmp_proptags, ppropvals)) {
			return EC_ERROR;	
		}
		for (i=0; i<ppropvals->count; i++) {
			if (propval_size(ppropvals->ppropval[i].proptag & 0xFFFF,
				ppropvals->ppropval[i].pvalue) > size_limit) {
				ppropvals->ppropval[i].proptag &= 0xFFFF0000;
				ppropvals->ppropval[i].proptag |= PROPVAL_TYPE_ERROR;
				ppropvals->ppropval[i].pvalue =
					common_util_alloc(sizeof(uint32_t));
				if (NULL == ppropvals->ppropval[i].pvalue) {
					return EC_OUT_OF_MEMORY;
				}
				*(uint32_t*)ppropvals->ppropval[i].pvalue =
													EC_OUT_OF_MEMORY;
			}
		}
		pinfo = emsmdb_interface_get_emsmdb_info();
		if (NULL == pinfo) {
			return EC_ERROR;
		}
		cpid = pinfo->cpid;
		break;
	case OBJECT_TYPE_FOLDER:
		if (FALSE == folder_object_get_all_proptags(
			pobject, &proptags)) {
			return EC_ERROR;
		}
		ptmp_proptags = common_util_trim_proptags(&proptags);
		if (NULL == ptmp_proptags) {
			return EC_OUT_OF_MEMORY;
		}
		if (FALSE == folder_object_get_properties(
			pobject, ptmp_proptags, ppropvals)) {
			return EC_ERROR;	
		}
		for (i=0; i<ppropvals->count; i++) {
			if (propval_size(ppropvals->ppropval[i].proptag & 0xFFFF,
				ppropvals->ppropval[i].pvalue) > size_limit) {
				ppropvals->ppropval[i].proptag &= 0xFFFF0000;
				ppropvals->ppropval[i].proptag |= PROPVAL_TYPE_ERROR;
				ppropvals->ppropval[i].pvalue =
								common_util_alloc(sizeof(uint32_t));
				if (NULL == ppropvals->ppropval[i].pvalue) {
					return EC_OUT_OF_MEMORY;
				}
				*(uint32_t*)ppropvals->ppropval[i].pvalue =
													EC_OUT_OF_MEMORY;
			}
		}
		pinfo = emsmdb_interface_get_emsmdb_info();
		if (NULL == pinfo) {
			return EC_ERROR;
		}
		cpid = pinfo->cpid;
		break;
	case OBJECT_TYPE_MESSAGE:
		if (FALSE == message_object_get_all_proptags(
			pobject, &proptags)) {
			return EC_ERROR;
		}
		ptmp_proptags = common_util_trim_proptags(&proptags);
		if (NULL == ptmp_proptags) {
			return EC_OUT_OF_MEMORY;
		}
		if (FALSE == message_object_get_properties(pobject,
			size_limit, ptmp_proptags, ppropvals)) {
			return EC_ERROR;	
		}
		cpid = attachment_object_get_cpid(pobject);
		break;
	case OBJECT_TYPE_ATTACHMENT:
		if (FALSE == attachment_object_get_all_proptags(
			pobject, &proptags)) {
			return EC_ERROR;
		}
		ptmp_proptags = common_util_trim_proptags(&proptags);
		if (NULL == ptmp_proptags) {
			return EC_OUT_OF_MEMORY;
		}
		if (FALSE == attachment_object_get_properties(pobject,
			size_limit, ptmp_proptags, ppropvals)) {
			return EC_ERROR;	
		}
		cpid = attachment_object_get_cpid(pobject);
		break;
	default:
		return EC_NOT_SUPPORTED;	
	}
	for (i=0; i<ppropvals->count; i++) {
		if (ppropvals->ppropval[i].proptag & 0xFFFF
			!= PROPVAL_TYPE_UNSPECIFIED) {
			continue;	
		}
		if (FALSE == common_util_convert_unspecified(cpid,
			b_unicode, ppropvals->ppropval[i].pvalue)) {
			return EC_OUT_OF_MEMORY;	
		}
	}
	return EC_SUCCESS;
}

uint32_t rop_getpropertieslist(PROPTAG_ARRAY *pproptags,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	void *pobject;
	int object_type;
	
	pobject = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pobject) {
		return EC_NULL_OBJECT;
	}
	switch (object_type) {
	case OBJECT_TYPE_LOGON:
		if (FALSE == logon_object_get_all_proptags(
			pobject, pproptags)) {
			return EC_ERROR;
		}
		return EC_SUCCESS;
	case OBJECT_TYPE_FOLDER:
		if (FALSE == folder_object_get_all_proptags(
			pobject, pproptags)) {
			return EC_ERROR;
		}
		return EC_SUCCESS;
	case OBJECT_TYPE_MESSAGE:
		if (FALSE == message_object_get_all_proptags(
			pobject, pproptags)) {
			return EC_ERROR;
		}
		return EC_SUCCESS;
	case OBJECT_TYPE_ATTACHMENT:
		if (FALSE == attachment_object_get_all_proptags(
			pobject, pproptags)) {
			return EC_ERROR;
		}
		return EC_SUCCESS;
	default:
		return EC_NOT_SUPPORTED;	
	}
}

uint32_t rop_setproperties(const TPROPVAL_ARRAY *ppropvals,
	PROBLEM_ARRAY *pproblems, void *plogmap,
	uint8_t logon_id, uint32_t hin)
{
	void *pobject;
	int object_type;
	uint32_t tag_access;
	uint32_t permission;
	DCERPC_INFO rpc_info;
	LOGON_OBJECT *plogon;
	
	plogon = rop_processor_get_logon_object(plogmap, logon_id);
	if (NULL == plogon) {
		return EC_ERROR;
	}
	pobject = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pobject) {
		return EC_NULL_OBJECT;
	}
	switch (object_type) {
	case OBJECT_TYPE_LOGON:
		if (LOGON_MODE_GUEST == logon_object_get_mode(plogon)) {
			return EC_ACCESS_DENIED;
		}
		if (FALSE == logon_object_set_properties(
			pobject, ppropvals, pproblems)) {
			return EC_ERROR;
		}
		return EC_SUCCESS;
	case OBJECT_TYPE_FOLDER:
		rpc_info = get_rpc_info();
		if (LOGON_MODE_OWNER != logon_object_get_mode(plogon)) {
			if (FALSE == exmdb_client_check_folder_permission(
				logon_object_get_dir(plogon),
				folder_object_get_id(pobject),
				rpc_info.username, &permission)) {
				return EC_ERROR;	
			}
			if (0 == (permission & PERMISSION_FOLDEROWNER)) {
				return EC_ACCESS_DENIED;
			}
		}
		if (FALSE == folder_object_set_properties(
			pobject, ppropvals, pproblems)) {
			return EC_ERROR;
		}
		return EC_SUCCESS;
	case OBJECT_TYPE_MESSAGE:
		tag_access = message_object_get_tag_access(pobject);
		if (0 == (tag_access & TAG_ACCESS_MODIFY)) {
			return EC_ACCESS_DENIED;
		}
		if (FALSE == message_object_set_properties(
			pobject, ppropvals, pproblems)) {
			return EC_ERROR;
		}
		return EC_SUCCESS;
	case OBJECT_TYPE_ATTACHMENT:
		tag_access = attachment_object_get_tag_access(pobject);
		if (0 == (tag_access & TAG_ACCESS_MODIFY)) {
			return EC_ACCESS_DENIED;
		}
		if (FALSE == attachment_object_set_properties(
			pobject, ppropvals, pproblems)) {
			return EC_ERROR;
		}
		return EC_SUCCESS;
	default:
		return EC_NOT_SUPPORTED;	
	}
}

uint32_t rop_setpropertiesnoreplicate(
	const TPROPVAL_ARRAY *ppropvals, PROBLEM_ARRAY *pproblems,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	return rop_setproperties(ppropvals,
		pproblems, plogmap, logon_id, hin);
}

uint32_t rop_deleteproperties(
	const PROPTAG_ARRAY *pproptags, PROBLEM_ARRAY *pproblems,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	void *pobject;
	int object_type;
	uint32_t tag_access;
	uint32_t permission;
	DCERPC_INFO rpc_info;
	LOGON_OBJECT *plogon;
	
	plogon = rop_processor_get_logon_object(plogmap, logon_id);
	if (NULL == plogon) {
		return EC_ERROR;
	}
	pobject = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pobject) {
		return EC_NULL_OBJECT;
	}
	switch (object_type) {
	case OBJECT_TYPE_LOGON:
		if (LOGON_MODE_GUEST == logon_object_get_mode(plogon)) {
			return EC_ACCESS_DENIED;
		}
		if (FALSE == logon_object_remove_properties(
			pobject, pproptags, pproblems)) {
			return EC_ERROR;
		}
		return EC_SUCCESS;
	case OBJECT_TYPE_FOLDER:
		rpc_info = get_rpc_info();
		if (LOGON_MODE_OWNER != logon_object_get_mode(plogon)) {
			if (FALSE == exmdb_client_check_folder_permission(
				logon_object_get_dir(plogon),
				folder_object_get_id(pobject),
				rpc_info.username, &permission)) {
				return EC_ERROR;	
			}
			if (0 == (permission & PERMISSION_FOLDEROWNER)) {
				return EC_ACCESS_DENIED;
			}
		}
		if (FALSE == folder_object_remove_properties(
			pobject, pproptags, pproblems)) {
			return EC_ERROR;
		}
		return EC_SUCCESS;
	case OBJECT_TYPE_MESSAGE:
		tag_access = message_object_get_tag_access(pobject);
		if (0 == (tag_access & TAG_ACCESS_MODIFY)) {
			return EC_ACCESS_DENIED;
		}
		if (FALSE == message_object_remove_properties(
			pobject, pproptags, pproblems)) {
			return EC_ERROR;
		}
		return EC_SUCCESS;
	case OBJECT_TYPE_ATTACHMENT:
		tag_access = attachment_object_get_tag_access(pobject);
		if (0 == (tag_access & TAG_ACCESS_MODIFY)) {
			return EC_ACCESS_DENIED;
		}
		if (FALSE == attachment_object_remove_properties(
			pobject, pproptags, pproblems)) {
			return EC_ERROR;
		}
		return EC_SUCCESS;
	default:
		return EC_NOT_SUPPORTED;	
	}
}

uint32_t rop_deletepropertiesnoreplicate(
	const PROPTAG_ARRAY *pproptags, PROBLEM_ARRAY *pproblems,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	return rop_deleteproperties(pproptags,
		pproblems, plogmap, logon_id, hin);
}

uint32_t rop_querynamedproperties(uint8_t query_flags,
	const GUID *pguid, PROPIDNAME_ARRAY *ppropidnames,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	int i;
	void *pobject;
	int object_type;
	uint16_t propid;
	LOGON_OBJECT *plogon;
	PROPID_ARRAY propids;
	PROPTAG_ARRAY proptags;
	PROPNAME_ARRAY propnames;
	
	plogon = rop_processor_get_logon_object(plogmap, logon_id);
	if (NULL == plogon) {
		return EC_ERROR;
	}
	pobject = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pobject) {
		return EC_NULL_OBJECT;
	}
	if ((query_flags & QUERY_FLAG_NOIDS) &&
		(query_flags & QUERY_FLAG_NOSTRINGS)) {
		ppropidnames->count = 0;
		ppropidnames->ppropid = NULL;
		ppropidnames->ppropname = NULL;
		return EC_SUCCESS;	
	}
	switch (object_type) {
	case OBJECT_TYPE_LOGON:
		if (FALSE == logon_object_get_all_proptags(
			pobject, &proptags)) {
			return EC_ERROR;	
		}
		break;
	case OBJECT_TYPE_FOLDER:
		if (FALSE == folder_object_get_all_proptags(
			pobject, &proptags)) {
			return EC_ERROR;	
		}
		break;
	case OBJECT_TYPE_MESSAGE:
		if (FALSE == message_object_get_all_proptags(
			pobject, &proptags)) {
			return EC_ERROR;
		}
		break;
	case OBJECT_TYPE_ATTACHMENT:
		if (FALSE == attachment_object_get_all_proptags(
			pobject, &proptags)) {
			return EC_ERROR;	
		}
		break;
	default:
		return EC_NOT_SUPPORTED;	
	}
	propids.count = 0;
	propids.ppropid = common_util_alloc(
		sizeof(uint16_t)*proptags.count);
	if (NULL == propids.ppropid) {
		return EC_OUT_OF_MEMORY;
	}
	for (i=0; i<proptags.count; i++) {
		propid = proptags.pproptag[i] >> 16;
		if (propid & 0x8000) {
			propids.ppropid[propids.count] = propid;
			propids.count ++;
		}
	}
	if (0 == propids.count) {
		ppropidnames->count = 0;
		ppropidnames->ppropid = NULL;
		ppropidnames->ppropname = NULL;
		return EC_SUCCESS;
	}
	ppropidnames->count = 0;
	ppropidnames->ppropid = common_util_alloc(
				sizeof(uint16_t)*propids.count);
	if (NULL == ppropidnames->ppropid) {
		return EC_OUT_OF_MEMORY;
	}
	ppropidnames->ppropname = common_util_alloc(
			sizeof(PROPERTY_NAME)*propids.count);
	if (NULL == ppropidnames->ppropid) {
		return EC_OUT_OF_MEMORY;
	}
	if (FALSE == logon_object_get_named_propnames(
		plogon, &propids, &propnames)) {
		return EC_ERROR;	
	}
	for (i=0; i<propids.count; i++) {
		if (KIND_NONE == propnames.ppropname[i].kind) {
			continue;
		}
		if (NULL != pguid && 0 != memcmp(pguid,
			&propnames.ppropname[i].guid, sizeof(GUID))) {
			continue;
		}
		if ((query_flags & QUERY_FLAG_NOSTRINGS) &&
			KIND_NAME == propnames.ppropname[i].kind) {
			continue;
		}
		if ((query_flags & QUERY_FLAG_NOIDS) &&
			KIND_LID == ppropidnames->ppropname[i].kind) {
			continue;
		}
		ppropidnames->ppropid[ppropidnames->count] =
										propids.ppropid[i];
		ppropidnames->ppropname[ppropidnames->count] =
								ppropidnames->ppropname[i];
		ppropidnames->count ++;
	}
	return EC_SUCCESS;
}

uint32_t rop_copyproperties(uint8_t want_asynchronous,
	uint8_t copy_flags, const PROPTAG_ARRAY *pproptags,
	PROBLEM_ARRAY *pproblems, void *plogmap,
	uint8_t logon_id, uint32_t hsrc, uint32_t hdst)
{
	int i;
	BOOL b_force;
	int dst_type;
	BOOL b_result;
	void *pobject;
	int object_type;
	void *pobject_dst;
	uint32_t permission;
	uint32_t tag_access;
	DCERPC_INFO rpc_info;
	LOGON_OBJECT *plogon;
	PROPTAG_ARRAY proptags;
	PROPTAG_ARRAY proptags1;
	TPROPVAL_ARRAY propvals;
	MESSAGE_CONTENT msgctnt;
	PROBLEM_ARRAY tmp_problems;
	uint16_t *poriginal_indices;
	
	/* we don't support COPY_FLAG_MOVE, just
		like exchange 2010 or later */
	if (copy_flags & ~(COPY_FLAG_MOVE|COPY_FLAG_NOOVERWRITE)) {
		return EC_INVALID_PARAMETER;
	}
	plogon = rop_processor_get_logon_object(plogmap, logon_id);
	if (NULL == plogon) {
		return EC_ERROR;
	}
	pobject = rop_processor_get_object(plogmap,
				logon_id, hsrc, &object_type);
	if (NULL == pobject) {
		return EC_NULL_OBJECT;
	}
	pobject_dst = rop_processor_get_object(
		plogmap, logon_id, hdst, &dst_type);
	if (NULL == pobject_dst) {
		return EC_DST_NULL_OBJECT;
	}
	if (dst_type != object_type) {
		return EC_DECLINE_COPY;
	}
	if (OBJECT_TYPE_FOLDER == object_type &&
		(COPY_FLAG_MOVE & copy_flags)) {
		return EC_NOT_SUPPORTED;
	}
	proptags.count = 0;
	proptags.pproptag = common_util_alloc(
		sizeof(uint32_t)*pproptags->count);
	if (NULL == proptags.pproptag) {
		return EC_OUT_OF_MEMORY;
	}
	pproblems->count = 0;
	pproblems->pproblem = common_util_alloc(
		sizeof(PROPERTY_PROBLEM)*pproptags->count);
	if (NULL == pproblems->pproblem) {
		return EC_OUT_OF_MEMORY;
	}
	poriginal_indices = common_util_alloc(
		sizeof(uint16_t)*pproptags->count);
	if (NULL == poriginal_indices) {
		return EC_ERROR;
	}
	switch (object_type) {
	case OBJECT_TYPE_FOLDER:
		rpc_info = get_rpc_info();
		if (LOGON_MODE_OWNER != logon_object_get_mode(plogon)) {
			if (FALSE == exmdb_client_check_folder_permission(
				logon_object_get_dir(plogon),
				folder_object_get_id(pobject_dst),
				rpc_info.username, &permission)) {
				return EC_ERROR;	
			}
			if (0 == (permission & PERMISSION_FOLDEROWNER)) {
				return EC_ACCESS_DENIED;
			}
		}
		if (copy_flags & COPY_FLAG_NOOVERWRITE) {
			if (FALSE == folder_object_get_all_proptags(
				pobject_dst, &proptags1)) {
				return EC_ERROR;	
			}
		}
		for (i=0; i<pproptags->count; i++) {
			if (TRUE == folder_object_check_readonly_property(
				pobject_dst, pproptags->pproptag[i])) {
				pproblems->pproblem[pproblems->count].index = i;
				pproblems->pproblem[pproblems->count].proptag =
										pproptags->pproptag[i];
				pproblems->pproblem[pproblems->count].err = 
											EC_ACCESS_DENIED;
				pproblems->count ++;
				continue;
			}
			if ((copy_flags & COPY_FLAG_NOOVERWRITE) &&
				common_util_index_proptags(&proptags1,
				pproptags->pproptag[i]) >= 0) {
				continue;
			}
			proptags.pproptag[proptags.count] = 
							pproptags->pproptag[i];
			poriginal_indices[proptags.count] = i;
			proptags.count ++;
		}
		if (FALSE == folder_object_get_properties(
			pobject, &proptags, &propvals)) {
			return EC_ERROR;	
		}
		for (i=0; i<proptags.count; i++) {
			if (NULL == common_util_get_propvals(
				&propvals, proptags.pproptag[i])) {
				pproblems->pproblem[pproblems->count].index =
										poriginal_indices[i];
				pproblems->pproblem[pproblems->count].proptag = 
										pproptags->pproptag[i];
				pproblems->pproblem[pproblems->count].err =
												EC_NOT_FOUND;
				pproblems->count ++;
			}
		}
		if (FALSE == folder_object_set_properties(
			pobject_dst, &propvals, &tmp_problems)) {
			return EC_ERROR;	
		}
		for (i=0; i<tmp_problems.count; i++) {
			tmp_problems.pproblem[i].index = common_util_index_proptags(
							pproptags, tmp_problems.pproblem[i].proptag);
		}
		memcpy(pproblems->pproblem + pproblems->count,
			tmp_problems.pproblem, tmp_problems.count*
			sizeof(PROPERTY_PROBLEM));
		pproblems->count += tmp_problems.count;
		qsort(pproblems->pproblem, pproblems->count,
			sizeof(PROPERTY_PROBLEM), common_util_problem_compare);
		return EC_SUCCESS;
	case OBJECT_TYPE_MESSAGE:
		tag_access = message_object_get_tag_access(pobject_dst);
		if (0 == (tag_access & TAG_ACCESS_MODIFY)) {
			return EC_ACCESS_DENIED;
		}
		b_force = TRUE;
		if (copy_flags & COPY_FLAG_NOOVERWRITE) {
			b_force = FALSE;
			if (FALSE == message_object_get_all_proptags(
				pobject_dst, &proptags1)) {
				return EC_ERROR;	
			}
		}
		for (i=0; i<pproptags->count; i++) {
			if (PROP_TAG_MESSAGEATTACHMENTS == pproptags->pproptag[i]) {
				if (FALSE == message_object_copy_attachments(
					pobject_dst, pobject, b_force, &b_result)) {
					return EC_ERROR;
				}
				if (FALSE == b_result) {
					pproblems->pproblem[pproblems->count].index = i;
					pproblems->pproblem[pproblems->count].proptag =
										PROP_TAG_MESSAGEATTACHMENTS;
					pproblems->pproblem[pproblems->count].err =
												EC_ACCESS_DENIED;
					pproblems->count ++;
				}
				continue;
			} else if (PROP_TAG_MESSAGERECIPIENTS == pproptags->pproptag[i]) {
				if (FALSE == message_object_copy_rcpts(
					pobject_dst, pobject, b_force, &b_result)) {
					return EC_ERROR;
				}
				if (FALSE == b_result) {
					pproblems->pproblem[pproblems->count].index = i;
					pproblems->pproblem[pproblems->count].proptag =
										PROP_TAG_MESSAGERECIPIENTS;
					pproblems->pproblem[pproblems->count].err =
												EC_ACCESS_DENIED;
					pproblems->count ++;
				}
				continue;
			}
			if (TRUE == message_object_check_readonly_property(
				pobject_dst, pproptags->pproptag[i])) {
				pproblems->pproblem[pproblems->count].index = i;
				pproblems->pproblem[pproblems->count].proptag =
										pproptags->pproptag[i];
				pproblems->pproblem[pproblems->count].err = 
											EC_ACCESS_DENIED;
				pproblems->count ++;
				continue;
			}
			if ((copy_flags & COPY_FLAG_NOOVERWRITE) &&
				common_util_index_proptags(&proptags1,
				pproptags->pproptag[i]) >= 0) {
				continue;
			}
			proptags.pproptag[proptags.count] = 
							pproptags->pproptag[i];
			poriginal_indices[proptags.count] = i;
			proptags.count ++;
		}
		if (FALSE == message_object_get_properties(
			pobject, 0, &proptags, &propvals)) {
			return EC_ERROR;	
		}
		for (i=0; i<proptags.count; i++) {
			if (NULL == common_util_get_propvals(
				&propvals, proptags.pproptag[i])) {
				pproblems->pproblem[pproblems->count].index =
										poriginal_indices[i];
				pproblems->pproblem[pproblems->count].proptag = 
										pproptags->pproptag[i];
				pproblems->pproblem[pproblems->count].err =
												EC_NOT_FOUND;
				pproblems->count ++;
			}
		}
		if (FALSE == message_object_set_properties(
			pobject_dst, &propvals, &tmp_problems)) {
			return EC_ERROR;	
		}
		for (i=0; i<tmp_problems.count; i++) {
			tmp_problems.pproblem[i].index = common_util_index_proptags(
							pproptags, tmp_problems.pproblem[i].proptag);
		}
		memcpy(pproblems->pproblem + pproblems->count,
			tmp_problems.pproblem, tmp_problems.count*
			sizeof(PROPERTY_PROBLEM));
		pproblems->count += tmp_problems.count;
		qsort(pproblems->pproblem, pproblems->count,
			sizeof(PROPERTY_PROBLEM), common_util_problem_compare);
		return EC_SUCCESS;
	case OBJECT_TYPE_ATTACHMENT:
		tag_access = attachment_object_get_tag_access(pobject_dst);
		if (0 == (tag_access & TAG_ACCESS_MODIFY)) {
			return EC_ACCESS_DENIED;
		}
		if (copy_flags & COPY_FLAG_NOOVERWRITE) {
			if (FALSE == attachment_object_get_all_proptags(
				pobject_dst, &proptags1)) {
				return EC_ERROR;
			}
		}
		for (i=0; i<pproptags->count; i++) {
			if (TRUE == attachment_object_check_readonly_property(
				pobject_dst, pproptags->pproptag[i])) {
				pproblems->pproblem[pproblems->count].index = i;
				pproblems->pproblem[pproblems->count].proptag =
										pproptags->pproptag[i];
				pproblems->pproblem[pproblems->count].err = 
											EC_ACCESS_DENIED;
				pproblems->count ++;
				continue;
			}
			if ((copy_flags & COPY_FLAG_NOOVERWRITE) &&
				common_util_index_proptags(&proptags1,
				pproptags->pproptag[i]) >= 0) {
				continue;
			}
			proptags.pproptag[proptags.count] = 
							pproptags->pproptag[i];
			poriginal_indices[proptags.count] = i;
			proptags.count ++;
		}
		if (FALSE == attachment_object_get_properties(
			pobject, 0, &proptags, &propvals)) {
			return EC_ERROR;	
		}
		for (i=0; i<proptags.count; i++) {
			if (NULL == common_util_get_propvals(
				&propvals, proptags.pproptag[i])) {
				pproblems->pproblem[pproblems->count].index =
										poriginal_indices[i];
				pproblems->pproblem[pproblems->count].proptag = 
										pproptags->pproptag[i];
				pproblems->pproblem[pproblems->count].err =
												EC_NOT_FOUND;
				pproblems->count ++;
			}
		}
		if (FALSE == attachment_object_set_properties(
			pobject_dst, &propvals, &tmp_problems)) {
			return EC_ERROR;	
		}
		for (i=0; i<tmp_problems.count; i++) {
			tmp_problems.pproblem[i].index = common_util_index_proptags(
							pproptags, tmp_problems.pproblem[i].proptag);
		}
		memcpy(pproblems->pproblem + pproblems->count,
			tmp_problems.pproblem, tmp_problems.count*
			sizeof(PROPERTY_PROBLEM));
		pproblems->count += tmp_problems.count;
		qsort(pproblems->pproblem, pproblems->count,
			sizeof(PROPERTY_PROBLEM), common_util_problem_compare);
		return EC_SUCCESS;
	default:
		return EC_NOT_SUPPORTED;	
	}
}

uint32_t rop_copyto(uint8_t want_asynchronous,
	uint8_t want_subobjects, uint8_t copy_flags,
	const PROPTAG_ARRAY *pexcluded_proptags,
	PROBLEM_ARRAY *pproblems, void *plogmap,
	uint8_t logon_id, uint32_t hsrc, uint32_t hdst)
{
	int i;
	BOOL b_fai;
	BOOL b_sub;
	BOOL b_guest;
	BOOL b_force;
	BOOL b_cycle;
	int dst_type;
	BOOL b_normal;
	BOOL b_collid;
	void *pobject;
	BOOL b_partial;
	int object_type;
	void *pobject_dst;
	EMSMDB_INFO *pinfo;
	uint32_t tag_access;
	uint32_t permission;
	const char *username;
	LOGON_OBJECT *plogon;
	DCERPC_INFO rpc_info;
	PROPTAG_ARRAY proptags;
	PROPTAG_ARRAY proptags1;
	TPROPVAL_ARRAY propvals;
	PROPTAG_ARRAY tmp_proptags;
	
	/* we don't support COPY_FLAG_MOVE, just
		like exchange 2010 or later */
	if (copy_flags & ~(COPY_FLAG_MOVE|COPY_FLAG_NOOVERWRITE)) {
		return EC_INVALID_PARAMETER;
	}
	plogon = rop_processor_get_logon_object(plogmap, logon_id);
	if (NULL == plogon) {
		return EC_ERROR;
	}
	pobject = rop_processor_get_object(plogmap,
				logon_id, hsrc, &object_type);
	if (NULL == pobject) {
		return EC_NULL_OBJECT;
	}
	pobject_dst = rop_processor_get_object(
		plogmap, logon_id, hdst, &dst_type);
	if (NULL == pobject_dst) {
		return EC_DST_NULL_OBJECT;
	}
	if (dst_type != object_type) {
		return EC_DECLINE_COPY;
	}
	if (OBJECT_TYPE_FOLDER == object_type &&
		(COPY_FLAG_MOVE & copy_flags)) {
		return EC_NOT_SUPPORTED;
	}
	if (copy_flags & COPY_FLAG_NOOVERWRITE) {
		b_force = FALSE;
	} else {
		b_force = TRUE;
	}
	switch (object_type) {
	case OBJECT_TYPE_FOLDER:
		/* MS-OXCPRPT 3.2.5.8, public folder not supported */
		if (FALSE == logon_object_check_private(plogon)) {
			return EC_NOT_SUPPORTED;
		}
		rpc_info = get_rpc_info();
		if (LOGON_MODE_OWNER != logon_object_get_mode(plogon)) {
			if (FALSE == exmdb_client_check_folder_permission(
				logon_object_get_dir(plogon),
				folder_object_get_id(pobject),
				rpc_info.username, &permission)) {
				return EC_ERROR;	
			}
			if (permission & PERMISSION_FOLDEROWNER) {
				username = NULL;
			} else {
				if (0 == (permission & PERMISSION_READANY)) {
					return EC_ACCESS_DENIED;
				}
				username = rpc_info.username;
			}
			if (FALSE == exmdb_client_check_folder_permission(
				logon_object_get_dir(plogon),
				folder_object_get_id(pobject_dst),
				rpc_info.username, &permission)) {
				return EC_ERROR;	
			}
			if (0 == (permission & PERMISSION_FOLDEROWNER)) {
				return EC_ACCESS_DENIED;
			}
			
		} else {
			username = NULL;
		}
		if (common_util_index_proptags(pexcluded_proptags,
			PROP_TAG_CONTAINERHIERARCHY) < 0) {
			if (FALSE == exmdb_client_check_folder_cycle(
				logon_object_get_dir(plogon),
				folder_object_get_id(pobject),
				folder_object_get_id(pobject_dst), &b_cycle)) {
				return EC_ERROR;	
			}
			if (TRUE == b_cycle) {
				return EC_FOLDER_CYCLE;
			}
			b_sub = TRUE;
		} else {
			b_sub = FALSE;
		}
		if (common_util_index_proptags(pexcluded_proptags,
			PROP_TAG_CONTAINERCONTENTS) < 0) {
			b_normal = TRUE;
		} else {
			b_normal = FALSE;
		}
		if (common_util_index_proptags(pexcluded_proptags,
			PROP_TAG_FOLDERASSOCIATEDCONTENTS) < 0) {
			b_fai = TRUE;	
		} else {
			b_fai = FALSE;
		}
		if (FALSE == folder_object_get_all_proptags(
			pobject, &proptags)) {
			return EC_ERROR;	
		}
		common_util_reduce_proptags(&proptags, pexcluded_proptags);
		tmp_proptags.count = 0;
		tmp_proptags.pproptag = common_util_alloc(
					sizeof(uint32_t)*proptags.count);
		if (NULL == tmp_proptags.pproptag) {
			return EC_OUT_OF_MEMORY;
		}
		if (FALSE == b_force) {
			if (FALSE == folder_object_get_all_proptags(
				pobject_dst, &proptags1)) {
				return EC_ERROR;	
			}
		}
		for (i=0; i<proptags.count; i++) {
			if (TRUE == folder_object_check_readonly_property(
				pobject_dst, proptags.pproptag[i])) {
				continue;
			}
			if (FALSE == b_force && common_util_index_proptags(
				&proptags1, proptags.pproptag[i]) >= 0) {
				continue;
			}
			tmp_proptags.pproptag[tmp_proptags.count] = 
									proptags.pproptag[i];
			tmp_proptags.count ++;
		}
		if (FALSE == folder_object_get_properties(
			pobject, &tmp_proptags, &propvals)) {
			return EC_ERROR;	
		}
		if (TRUE == b_sub || TRUE == b_normal || TRUE == b_fai) {
			pinfo = emsmdb_interface_get_emsmdb_info();
			if (NULL == username) {
				b_guest = FALSE;
			} else {
				b_guest = TRUE;
			}
			if (FALSE == exmdb_client_copy_folder_internal(
				logon_object_get_dir(plogon),
				logon_object_get_account_id(plogon),
				pinfo->cpid, b_guest, rpc_info.username,
				folder_object_get_id(pobject), b_normal, b_fai,
				b_sub, folder_object_get_id(pobject_dst),
				&b_collid, &b_partial)) {
				return EC_ERROR;
			}
			if (TRUE == b_collid) {
				return EC_COLLIDING_NAMES;
			}
			if (FALSE == folder_object_set_properties(
				pobject_dst, &propvals, pproblems)) {
				return EC_ERROR;	
			}
			return EC_SUCCESS;
		}
		if (FALSE == folder_object_set_properties(
			pobject_dst, &propvals, pproblems)) {
			return EC_ERROR;	
		}
		return EC_SUCCESS;
	case OBJECT_TYPE_MESSAGE:
		tag_access = message_object_get_tag_access(pobject_dst);
		if (0 == (tag_access & TAG_ACCESS_MODIFY)) {
			return EC_ACCESS_DENIED;
		}
		if (FALSE == message_object_copy_to(
			pobject_dst, pobject, pexcluded_proptags,
			b_force, &b_cycle, pproblems)) {
			return EC_ERROR;	
		}
		if (TRUE == b_cycle) {
			return EC_MESSAGE_CYCLE;
		}
		return EC_SUCCESS;
	case OBJECT_TYPE_ATTACHMENT:
		tag_access = attachment_object_get_tag_access(pobject_dst);
		if (0 == (tag_access & TAG_ACCESS_MODIFY)) {
			return EC_ACCESS_DENIED;
		}
		if (FALSE == attachment_object_copy_properties(pobject_dst,
			pobject, pexcluded_proptags, b_force, &b_cycle, pproblems)) {
			return EC_ERROR;	
		}
		if (TRUE == b_cycle) {
			return EC_MESSAGE_CYCLE;
		}
		return EC_SUCCESS;
	default:
		return EC_NOT_SUPPORTED;	
	}
}

uint32_t rop_progress(uint8_t want_cancel,
	uint32_t *pcompleted_count, uint32_t *ptotal_count,
	uint8_t *prop_id, uint8_t *ppartial_completion,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	return EC_NOT_SUPPORTED;
}

uint32_t rop_openstream(uint32_t proptag, uint8_t flags,
	uint32_t *pstream_size, void *plogmap,
	uint8_t logon_id, uint32_t hin, uint32_t *phout)
{
	void *pbuff;
	BOOL b_write;
	BINARY *pbin;
	void *pobject;
	uint32_t length;
	int object_type;
	uint32_t max_length;
	uint32_t tag_access;
	uint32_t permission;
	DCERPC_INFO rpc_info;
	LOGON_OBJECT *plogon;
	STREAM_OBJECT *pstream;
	PROPTAG_ARRAY proptags;
	PROPERTY_ROW properties_row;
	
	/* MS-OXCPERM 3.1.4.1 */
	if (PROP_TAG_SECURITYDESCRIPTORASXML == proptag) {
		return EC_NOT_SUPPORTED;
	}
	plogon = rop_processor_get_logon_object(plogmap, logon_id);
	if (NULL == plogon) {
		return EC_ERROR;
	}
	pobject = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pobject) {
		return EC_NULL_OBJECT;
	}
	if (OPENSTREAM_FLAG_CREATE == flags ||
		OPENSTREAM_FLAG_READWRITE == flags) {
		b_write = TRUE;
	} else {
		b_write = FALSE;
	}
	switch (object_type) {
	case OBJECT_TYPE_FOLDER:
		if (FALSE == logon_object_check_private(plogon) &&
			OPENSTREAM_FLAG_READONLY != flags) {
			return EC_NOT_SUPPORTED;
		}
		if (PROPVAL_TYPE_BINARY != (proptag & 0xFFFF)) {
			return EC_NOT_SUPPORTED;
		}
		if (TRUE == b_write) {
			rpc_info = get_rpc_info();
			if (LOGON_MODE_OWNER != logon_object_get_mode(plogon)) {
				if (FALSE == exmdb_client_check_folder_permission(
					logon_object_get_dir(plogon),
					folder_object_get_id(pobject),
					rpc_info.username, &permission)) {
					return EC_ERROR;	
				}
				if (0 == (permission & PERMISSION_FOLDEROWNER)) {
					return EC_ACCESS_DENIED;
				}
			}
		}
		max_length = MAX_LENGTH_FOR_FOLDER;
		break;
	case OBJECT_TYPE_MESSAGE:
	case OBJECT_TYPE_ATTACHMENT:
		switch (proptag & 0xFFFF) {
		case PROPVAL_TYPE_BINARY:
		case PROPVAL_TYPE_STRING:
		case PROPVAL_TYPE_WSTRING:
			break;
		case PROPVAL_TYPE_OBJECT:
			if (PROP_TAG_ATTACHDATAOBJECT == proptag) {
				break;
			}
			return EC_NOT_FOUND;
		default:
			return EC_NOT_SUPPORTED;	
		}
		if (TRUE == b_write) {
			if (OBJECT_TYPE_MESSAGE == object_type) {
				tag_access = message_object_get_tag_access(pobject);
			} else {
				tag_access = attachment_object_get_tag_access(pobject);
			}
			if (0 == (tag_access & TAG_ACCESS_MODIFY)) {
				return EC_ACCESS_DENIED;
			}
		}
		max_length = common_util_get_param(COMMON_UTIL_MAX_MAIL_LENGTH);
		break;
	default:
		return EC_NOT_SUPPORTED;	
	}
	pstream = stream_object_create(pobject,
		object_type, flags, proptag, max_length);
	if (NULL == pstream) {
		return EC_ERROR;
	}
	if (FALSE == stream_object_check(pstream)) {
		stream_object_free(pstream);
		return EC_NOT_FOUND;
	}
	*phout = rop_processor_add_object_handle(plogmap,
			logon_id, hin, OBJECT_TYPE_STREAM, pstream);
	if (*phout < 0) {
		stream_object_free(pstream);
		return EC_ERROR;
	}
	*pstream_size = stream_object_get_length(pstream);
	return EC_SUCCESS;
}

uint32_t rop_readstream(uint16_t byte_count,
	uint32_t max_byte_count, BINARY *pdata_bin,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	uint16_t max_rop;
	uint16_t read_len;
	uint32_t buffer_size;
	uint32_t object_type;
	STREAM_OBJECT *pstream;
	
	pstream = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pstream) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_STREAM != object_type) {
		return EC_NOT_SUPPORTED;
	}
	if (0xBABE == byte_count) {
		buffer_size = max_byte_count;
	} else {
		buffer_size = byte_count;
	}
	emsmdb_interface_get_rop_left(&max_rop);
	max_rop -= 16;
	if (buffer_size > max_rop) {
		buffer_size = max_rop;
	}
	if (0 == buffer_size) {
		pdata_bin->cb = 0;
		pdata_bin->pb = NULL;
		return EC_SUCCESS;
	}
	pdata_bin->pb = common_util_alloc(buffer_size);
	if (NULL == pdata_bin->pb) {
		return EC_OUT_OF_MEMORY;
	}
	read_len = stream_object_read(pstream,
				pdata_bin->pb, buffer_size);
	pdata_bin->cb = read_len;
	return EC_SUCCESS;
}

uint32_t rop_writestream(const BINARY *pdata_bin,
	uint16_t *pwritten_size, void *plogmap,
	uint8_t logon_id, uint32_t hin)
{
	int written_len;
	uint32_t object_type;
	STREAM_OBJECT *pstream;
	
	pstream = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pstream) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_STREAM != object_type) {
		return EC_NOT_SUPPORTED;
	}
	if (stream_object_get_open_flags(pstream) ==
		OPENSTREAM_FLAG_READONLY) {
		return EC_STREAM_ACCESS_DENIED;	
	}
	if (0 == pdata_bin->cb) {
		*pwritten_size = 0;
		return EC_SUCCESS;
	}
	if (stream_object_get_seek_position(pstream) >=
		stream_object_get_max_length(pstream)) {
		return EC_TOO_BIG;	
	}
	*pwritten_size = stream_object_write(
			pstream, pdata_bin->pb, pdata_bin->cb);
	if (0 == *pwritten_size) {
		return EC_ERROR;
	}
	if (*pwritten_size < pdata_bin->cb) {
		return EC_TOO_BIG;
	}
	return EC_SUCCESS;
}

uint32_t rop_commitstream(void *plogmap,
	uint8_t logon_id, uint32_t hin)
{
	int object_type;
	STREAM_OBJECT *pstream;
	
	pstream = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pstream) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_STREAM != object_type) {
		return EC_NOT_SUPPORTED;
	}
	switch (stream_object_get_parent_type(pstream)) {
	case OBJECT_TYPE_FOLDER:
		if (FALSE == stream_object_commit(pstream)) {
			return EC_ERROR;
		}
		return EC_SUCCESS;
	case OBJECT_TYPE_MESSAGE:
	case OBJECT_TYPE_ATTACHMENT:
		return EC_SUCCESS;
	default:
		return EC_NOT_SUPPORTED;
	}
}

uint32_t rop_getstreamsize(uint32_t *pstream_size,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	int object_type;
	STREAM_OBJECT *pstream;
	
	pstream = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pstream) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_STREAM != object_type) {
		return EC_NOT_SUPPORTED;
	}
	*pstream_size = stream_object_get_length(pstream);
	return EC_SUCCESS;
}

uint32_t rop_setstreamsize(uint64_t stream_size,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	int object_type;
	STREAM_OBJECT *pstream;
	
	if (stream_size > 0x80000000) {
		return EC_INVALID_PARAMETER;
	}
	pstream = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pstream) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_STREAM != object_type) {
		return EC_NOT_SUPPORTED;
	}
	if (stream_size > stream_object_get_max_length(pstream)) {
		return EC_TOO_BIG;
	}
	if (FALSE == stream_object_set_length(pstream, stream_size)) {
		return EC_ERROR;
	}
	return EC_SUCCESS;
}

uint32_t rop_seekstream(uint8_t seek_pos,
	int64_t offset, uint64_t *pnew_pos,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	int object_type;
	uint32_t new_pos;
	STREAM_OBJECT *pstream;
	
	switch (seek_pos) {
	case SEEK_POS_BEGIN:
	case SEEK_POS_CURRENT:
	case SEEK_POS_END:
		break;
	default:
		return EC_INVALID_PARAMETER;	
	}
	if (offset > 0x7FFFFFFF || offset < -0x7FFFFFFF) {
		return EC_SEEK_ERROR;
	}
	pstream = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pstream) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_STREAM != object_type) {
		return EC_NOT_SUPPORTED;
	}
	if (FALSE == stream_object_seek(pstream, seek_pos, offset)) {
		return EC_SEEK_ERROR;
	}
	*pnew_pos = stream_object_get_seek_position(pstream);
	return EC_SUCCESS;
}

uint32_t rop_copytostream(uint64_t byte_count,
	uint64_t *pread_bytes, uint64_t *pwritten_bytes,
	void *plogmap, uint8_t logon_id, uint32_t hsrc,
	uint32_t hdst)
{
	int object_type;
	uint32_t length;
	STREAM_OBJECT *psrc_stream;
	STREAM_OBJECT *pdst_stream;
	
	psrc_stream = rop_processor_get_object(plogmap,
					logon_id, hsrc, &object_type);
	if (NULL == psrc_stream) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_STREAM != object_type) {
		return EC_NOT_SUPPORTED;
	}
	pdst_stream = rop_processor_get_object(plogmap,
					logon_id, hdst, &object_type);
	if (NULL == psrc_stream) {
		return EC_DST_NULL_OBJECT;
	}
	if (stream_object_get_open_flags(pdst_stream)
		== OPENSTREAM_FLAG_READONLY) {
		return EC_ACCESS_DENIED;	
	}
	if (0 == byte_count) {
		*pread_bytes = 0;
		*pwritten_bytes = 0;
		return EC_SUCCESS;
	}
	length = byte_count;
	if (FALSE == stream_object_copy(
		pdst_stream, psrc_stream, &length)) {
		return EC_ERROR;	
	}
	*pread_bytes = length;
	*pwritten_bytes = length;
	return EC_SUCCESS;
}

uint32_t rop_lockregionstream(uint64_t region_offset,
	uint64_t region_size, uint32_t lock_flags,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	 /* just like exchange 2010 or later */
	 return EC_NOT_IMPLEMENTED;
}

uint32_t rop_unlockregionstream(uint64_t region_offset,
	uint64_t region_size, uint32_t lock_flags,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	 /* just like exchange 2010 or later */
	return EC_NOT_IMPLEMENTED;
}

uint32_t rop_writeandcommitstream(
	const BINARY *pdata, uint16_t *pwritten_size,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	 /* just like exchange 2010 or later */
	return EC_NOT_IMPLEMENTED;
}

uint32_t rop_clonestream(void *plogmap,
	uint8_t logon_id, uint32_t hin, uint32_t *phout)
{
	 /* just like exchange 2010 or later */
	return EC_NOT_IMPLEMENTED;
}
