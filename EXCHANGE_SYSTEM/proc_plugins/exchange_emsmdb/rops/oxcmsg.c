#include "rops.h"
#include "rop_util.h"
#include "common_util.h"
#include "proc_common.h"
#include "exmdb_client.h"
#include "logon_object.h"
#include "table_object.h"
#include "rop_processor.h"
#include "message_object.h"
#include "processor_types.h"
#include "emsmdb_interface.h"
#include "attachment_object.h"


uint32_t rop_openmessage(uint16_t cpid,
	uint64_t folder_id, uint8_t open_mode_flags,
	uint64_t message_id, uint8_t *phas_named_properties,
	TYPED_STRING *psubject_prefix, TYPED_STRING *pnormalized_subject,
	uint16_t *precipient_count, PROPTAG_ARRAY *precipient_columns,
	uint8_t *prow_count, OPENRECIPIENT_ROW **pprecipient_row,
	void *plogmap, uint8_t logon_id, uint32_t hin, uint32_t *phout)
{
	int i;
	BOOL b_del;
	int rop_num;
	BOOL b_exist;
	BOOL b_owner;
	void *pvalue;
	int object_type;
	uint8_t rcpt_num;
	TARRAY_SET rcpts;
	EMSMDB_INFO *pinfo;
	uint32_t tag_access;
	uint32_t permission;
	LOGON_OBJECT *plogon;
	DCERPC_INFO rpc_info;
	PROPTAG_ARRAY proptags;
	TPROPVAL_ARRAY propvals;
	PROPTAG_ARRAY *pcolumns;
	uint32_t proptag_buff[3];
	MESSAGE_OBJECT *pmessage;
	
	if (0x0FFF == cpid) {
		pinfo = emsmdb_interface_get_emsmdb_info();
		if (NULL == pinfo) {
			return EC_ERROR;
		}
		cpid = pinfo->cpid;
	}
	if (FALSE == common_util_verify_cpid(cpid)) {
		return EC_INVALID_CODEPAGE;
	}
	plogon = rop_processor_get_logon_object(plogmap, logon_id);
	if (NULL == plogon) {
		return EC_ERROR;
	}
	if (NULL == rop_processor_get_object(
		plogmap, logon_id, hin, &object_type)) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_LOGON != object_type &&
		OBJECT_TYPE_FOLDER != object_type) {
		return EC_NOT_SUPPORTED;
	}
	if (FALSE == exmdb_client_check_message(
		logon_object_get_dir(plogon), folder_id,
		message_id, &b_exist)) {
		return EC_ERROR;
	}
	if (FALSE == b_exist) {
		return EC_NOT_FOUND;
	}
	if (FALSE == exmdb_client_get_message_property(
		logon_object_get_dir(plogon), NULL, 0,
		message_id, PROP_TAG_FOLDERID, &pvalue) ||
		NULL == pvalue) {
		return EC_ERROR;
	}
	folder_id = *(uint64_t*)pvalue;
	if (FALSE == exmdb_client_check_message_deleted(
		logon_object_get_dir(plogon), message_id, &b_del)) {
		return EC_ERROR;	
	}
	if (TRUE == b_del && 0 == (open_mode_flags &
		OPEN_MODE_FLAG_OPENSOFTDELETE)) {
		return EC_NOT_FOUND;
	}
	
	tag_access = 0;
	rpc_info = get_rpc_info();
	if (LOGON_MODE_OWNER == logon_object_get_mode(plogon)) {
		tag_access = TAG_ACCESS_MODIFY|TAG_ACCESS_READ|TAG_ACCESS_DELETE;
		goto PERMISSION_CHECK;
	}
	if (FALSE == exmdb_client_check_folder_permission(
		logon_object_get_dir(plogon), folder_id,
		rpc_info.username, &permission)) {
		return EC_ERROR;	
	}
	if (0 == (permission & PERMISSION_READANY) &&
		0 == (permission & PERMISSION_FOLDERVISIBLE) &&
		0 == (permission & PERMISSION_FOLDEROWNER)) {
		return EC_ACCESS_DENIED;
	}
	if (permission & PERMISSION_FOLDEROWNER) {
		tag_access = TAG_ACCESS_MODIFY|TAG_ACCESS_READ|TAG_ACCESS_DELETE;
		goto PERMISSION_CHECK;
	}
	if (FALSE == exmdb_client_check_message_owner(
		logon_object_get_dir(plogon), message_id,
		rpc_info.username, &b_owner)) {
		return EC_ERROR;	
	}
	if (TRUE == b_owner || (permission & PERMISSION_READANY)) {
		tag_access |= TAG_ACCESS_READ;
	}
	if ((permission & PERMISSION_EDITANY) || (TRUE == b_owner &&
		(permission & PERMISSION_EDITOWNED))) {
		tag_access |= TAG_ACCESS_MODIFY;	
	}
	if ((permission & PERMISSION_DELETEANY) || (TRUE == b_owner &&
		(permission & PERMISSION_DELETEOWNED))) {
		tag_access |= TAG_ACCESS_DELETE;	
	}
	
PERMISSION_CHECK:
	if (0 == (TAG_ACCESS_READ & tag_access)) {
		return EC_ACCESS_DENIED;
	}
	if (0 == (open_mode_flags & OPEN_MODE_FLAG_READWRITE) &&
		0 == (TAG_ACCESS_MODIFY & tag_access)) {
		if (open_mode_flags & OPEN_MODE_FLAG_BESTACCESS) {
			open_mode_flags &= ~OPEN_MODE_FLAG_BESTACCESS;
		} else {
			return EC_ACCESS_DENIED;
		}
	}
	
	pmessage = message_object_create(plogon, FALSE,
				cpid, message_id, &folder_id,
				tag_access, open_mode_flags, NULL);
	if (NULL == pmessage) {
		return EC_OUT_OF_MEMORY;
	}
	proptags.count = 3;
	proptags.pproptag = proptag_buff;
	proptag_buff[0] = PROP_TAG_HASNAMEDPROPERTIES;
	proptag_buff[1] = PROP_TAG_SUBJECTPREFIX;
	proptag_buff[2] = PROP_TAG_NORMALIZEDSUBJECT;
	if (FALSE == message_object_get_properties(
		pmessage, 0, &proptags, &propvals)) {
		message_object_free(pmessage);
		return EC_ERROR;	
	}
	pvalue = common_util_get_propvals(&propvals,
				PROP_TAG_HASNAMEDPROPERTIES);
	if (NULL == pvalue || 0 == *(uint8_t*)pvalue) {
		*phas_named_properties = 1;
	} else {
		*phas_named_properties = 0;
	}
	pvalue = common_util_get_propvals(&propvals,
						PROP_TAG_SUBJECTPREFIX);
	if (NULL == pvalue) {
		psubject_prefix->string_type = STRING_TYPE_EMPTY;
		psubject_prefix->pstring = NULL;
	} else {
		psubject_prefix->string_type = STRING_TYPE_UNICODE;
		psubject_prefix->pstring = pvalue;
	}
	pvalue = common_util_get_propvals(&propvals,
					PROP_TAG_NORMALIZEDSUBJECT);
	if (NULL == pvalue) {
		pnormalized_subject->string_type = STRING_TYPE_EMPTY;
		pnormalized_subject->pstring = NULL;
	} else {
		pnormalized_subject->string_type = STRING_TYPE_UNICODE;
		pnormalized_subject->pstring = pvalue;
	}
	if (FALSE == message_object_get_recipient_num(
		pmessage, precipient_count)) {
		message_object_free(pmessage);
		return EC_ERROR;
	}
	pcolumns = message_object_get_rcpt_columns(pmessage);
	*precipient_columns = *pcolumns;
	emsmdb_interface_get_rop_num(&rop_num);
	if (1 == rop_num) {
		rcpt_num = 0xFE;
	} else {
		rcpt_num = 5;
	}
	if (FALSE == message_object_read_recipients(
		pmessage, 0, rcpt_num, &rcpts)) {
		message_object_free(pmessage);
		return EC_ERROR;
	}
	*prow_count = rcpts.count;
	if (rcpts.count > 0) {
		*pprecipient_row = common_util_alloc(
			sizeof(OPENRECIPIENT_ROW)*rcpts.count);
		if (NULL == *pprecipient_row) {
			message_object_free(pmessage);
			return EC_OUT_OF_MEMORY;
		}
	}
	for (i=0; i<rcpts.count; i++) {
		if (FALSE == common_util_propvals_to_openrecipient(
			cpid, rcpts.pparray[i], pcolumns,
			(*pprecipient_row) + i)) {
			message_object_free(pmessage);
			return EC_OUT_OF_MEMORY;
		}
	}
	*phout = rop_processor_add_object_handle(plogmap,
		logon_id, hin, OBJECT_TYPE_MESSAGE, pmessage);
	if (*phout < 0) {
		message_object_free(pmessage);
		return EC_ERROR;
	}
	return EC_SUCCESS;
}

uint32_t rop_createmessage(uint16_t cpid,
	uint64_t folder_id, uint8_t associated_flag,
	uint64_t **ppmessage_id, void *plogmap,
	uint8_t logon_id, uint32_t hin, uint32_t *phout)
{
	BOOL b_fai;
	void *pvalue;
	int object_type;
	int64_t max_quota;
	EMSMDB_INFO *pinfo;
	uint32_t total_mail;
	uint64_t total_size;
	uint32_t tag_access;
	uint32_t permission;
	DCERPC_INFO rpc_info;
	LOGON_OBJECT *plogon;
	MESSAGE_OBJECT *pmessage;
	uint32_t proptag_buff[4];
	PROPTAG_ARRAY tmp_proptags;
	TPROPVAL_ARRAY tmp_propvals;
	
	if (0x0FFF == cpid) {
		pinfo = emsmdb_interface_get_emsmdb_info();
		if (NULL == pinfo) {
			return EC_ERROR;
		}
		cpid = pinfo->cpid;
	}
	if (FALSE == common_util_verify_cpid(cpid)) {
		return EC_INVALID_CODEPAGE;
	}
	plogon = rop_processor_get_logon_object(plogmap, logon_id);
	if (NULL == plogon) {
		return EC_ERROR;
	}
	if (NULL == rop_processor_get_object(
		plogmap, logon_id, hin, &object_type)) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_LOGON != object_type &&
		OBJECT_TYPE_FOLDER != object_type) {
		return EC_NOT_SUPPORTED;
	}
	rpc_info = get_rpc_info();
	if (LOGON_MODE_OWNER != logon_object_get_mode(plogon)) {
		if (FALSE == exmdb_client_check_folder_permission(
			logon_object_get_dir(plogon), folder_id,
			rpc_info.username, &permission)) {
			return EC_ERROR;	
		}
		if (0 == (permission & PERMISSION_FOLDEROWNER) &&
			0 == (permission & PERMISSION_CREATE)) {
			return EC_ACCESS_DENIED;
		}
		tag_access = TAG_ACCESS_MODIFY|TAG_ACCESS_READ;
		if ((permission & PERMISSION_DELETEOWNED) ||
			(permission & PERMISSION_DELETEANY)) {
			tag_access |= TAG_ACCESS_DELETE;
		}
	} else {
		tag_access = TAG_ACCESS_MODIFY|TAG_ACCESS_READ|TAG_ACCESS_DELETE;
	}
	tmp_proptags.count = 4;
	tmp_proptags.pproptag = proptag_buff;
	proptag_buff[0] = PROP_TAG_MESSAGESIZEEXTENDED;
	proptag_buff[1] = PROP_TAG_PROHIBITSENDQUOTA;
	proptag_buff[2] = PROP_TAG_ASSOCIATEDCONTENTCOUNT;
	proptag_buff[3] = PROP_TAG_CONTENTCOUNT;
	if (FALSE == logon_object_get_properties(
		plogon, &tmp_proptags, &tmp_propvals)) {
		return EC_ERROR;	
	}
	pvalue = common_util_get_propvals(&tmp_propvals,
						PROP_TAG_PROHIBITSENDQUOTA);
	if (NULL == pvalue) {
		max_quota = -1;
	} else {
		max_quota = *(uint32_t*)pvalue;
		max_quota *= 1024;
	}
	pvalue = common_util_get_propvals(&tmp_propvals,
					PROP_TAG_MESSAGESIZEEXTENDED);
	if (NULL == pvalue) {
		total_size = 0;
	} else {
		total_size = *(uint64_t*)pvalue;
	}
	if (max_quota > 0 && total_size > max_quota) {
		return EC_QUOTA_EXCEEDED;
	}
	total_mail = 0;
	pvalue = common_util_get_propvals(&tmp_propvals,
					PROP_TAG_ASSOCIATEDCONTENTCOUNT);
	if (NULL != pvalue) {
		total_mail += *(uint32_t*)pvalue;
	}
	pvalue = common_util_get_propvals(&tmp_propvals,
							PROP_TAG_CONTENTCOUNT);
	if (NULL != pvalue) {
		total_mail += *(uint32_t*)pvalue;
	}
	if (total_mail > common_util_get_param(
		COMMON_UTIL_MAX_MESSAGE)) {
		return EC_QUOTA_EXCEEDED;
	}
	*ppmessage_id = common_util_alloc(sizeof(uint64_t));
	if (NULL == *ppmessage_id) {
		return EC_OUT_OF_MEMORY;
	}
	if (FALSE == exmdb_client_allocate_message_id(
		logon_object_get_dir(plogon), folder_id,
		*ppmessage_id)) {
		return EC_ERROR;	
	}
	pmessage = message_object_create(plogon, TRUE, cpid,
				**ppmessage_id, &folder_id, tag_access,
				OPEN_MODE_FLAG_READWRITE, NULL);
	if (NULL == pmessage) {
		return EC_OUT_OF_MEMORY;
	}
	if (0 == associated_flag) {
		b_fai = FALSE;
	} else {
		b_fai = TRUE;
	}
	if (FALSE == message_object_init_message(pmessage, b_fai, cpid)) {
		message_object_free(pmessage);
		return EC_ERROR;
	}
	*phout = rop_processor_add_object_handle(plogmap,
		logon_id, hin, OBJECT_TYPE_MESSAGE, pmessage);
	if (*phout < 0) {
		message_object_free(pmessage);
		return EC_ERROR;
	}
	return EC_SUCCESS;
}

uint32_t rop_savechangesmessage(uint8_t save_flags,
	uint64_t *pmessage_id, void *plogmap, uint8_t logon_id,
	uint32_t hresponse, uint32_t hin)
{
	void *pvalue;
	BOOL b_touched;
	int object_type;
	uint8_t open_flags;
	uint32_t tag_access;
	uint32_t tmp_proptag;
	PROPTAG_ARRAY proptags;
	TPROPVAL_ARRAY propvals;
	MESSAGE_OBJECT *pmessage;
	
	save_flags &= SAVE_FLAG_KEEPOPENREADONLY |
					SAVE_FLAG_KEEPOPENREADWRITE |
					SAVE_FLAG_FORCESAVE;
	pmessage = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pmessage) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_MESSAGE != object_type) {
		return EC_NOT_SUPPORTED;
	}
	tag_access = message_object_get_tag_access(pmessage);
	if (0 == (TAG_ACCESS_MODIFY & tag_access)) {
		return EC_ACCESS_DENIED;
	}
	open_flags = message_object_get_open_flags(pmessage);
	if (0 == (open_flags & OPEN_MODE_FLAG_READWRITE) &&
		SAVE_FLAG_FORCESAVE != save_flags) {
		return EC_ACCESS_DENIED;
	}
	if (SAVE_FLAG_FORCESAVE != save_flags) {
		if (FALSE == message_object_check_orignal_touched(
			pmessage, &b_touched)) {
			return EC_ERROR;	
		}
		if (TRUE == b_touched) {
			return EC_OBJECT_MODIFIED;
		}
	}
	proptags.count = 1;
	proptags.pproptag = &tmp_proptag;
	tmp_proptag = PROP_TAG_MID;
	if (FALSE == message_object_get_properties(
		pmessage, 0, &proptags, &propvals)) {
		return EC_ERROR;
	}
	pvalue = common_util_get_propvals(&propvals, PROP_TAG_MID);
	if (NULL == pvalue) {
		return EC_ERROR;
	}
	*pmessage_id = *(uint64_t*)pvalue;
	if (FALSE == message_object_save(pmessage)) {
		return EC_ERROR;
	}
	switch (save_flags) {
	case SAVE_FLAG_KEEPOPENREADWRITE:
	case SAVE_FLAG_FORCESAVE:
		open_flags = OPEN_MODE_FLAG_READWRITE;
		message_object_set_open_flags(pmessage, open_flags);
		break;
	}
	return EC_SUCCESS;
}

uint32_t rop_removeallrecipients(uint32_t reserved,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	int object_type;
	MESSAGE_OBJECT *pmessage;
	
	pmessage = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pmessage) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_MESSAGE != object_type) {
		return EC_NOT_SUPPORTED;
	}
	message_object_empty_rcpts(pmessage);
	return EC_SUCCESS;
}

uint32_t rop_modifyrecipients(const PROPTAG_ARRAY *pproptags,
	uint16_t count, const MODIFYRECIPIENT_ROW *prow,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	int i;
	int object_type;
	EMSMDB_INFO *pinfo;
	TARRAY_SET tmp_set;
	MESSAGE_OBJECT *pmessage;
	TPROPVAL_ARRAY *ppropvals;
	
	if (pproptags->count >= 0x7FEF || count >= 0x7FEF) {
		return EC_INVALID_PARAMETER;
	}
	for (i=0; i<pproptags->count; i++) {
		switch (pproptags->pproptag[i]) {
		case PROP_TAG_ADDRESSTYPE:
		case PROP_TAG_DISPLAYNAME:
		case PROP_TAG_EMAILADDRESS:
		case PROP_TAG_ENTRYID:
		case PROP_TAG_INSTANCEKEY:
		case PROP_TAG_RECIPIENTTYPE:
		case PROP_TAG_SEARCHKEY:
		case PROP_TAG_SENDRICHINFO:
		case PROP_TAG_TRANSMITTABLEDISPLAYNAME:
			return EC_INVALID_PARAMETER;
		}
	}
	pmessage = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pmessage) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_MESSAGE != object_type) {
		return EC_NOT_SUPPORTED;
	}
	pinfo = emsmdb_interface_get_emsmdb_info();
	if (NULL == pinfo) {
		return EC_ERROR;
	}
	tmp_set.count = count;
	tmp_set.pparray = common_util_alloc(sizeof(TPROPVAL_ARRAY*)*count);
	if (NULL == tmp_set.pparray) {
		return EC_OUT_OF_MEMORY;
	}
	for (i=0; i<count; i++) {
		ppropvals = common_util_alloc(sizeof(TPROPVAL_ARRAY));
		if (NULL == ppropvals) {
			return EC_OUT_OF_MEMORY;
		}
		if (NULL == prow[i].precipient_row) {
			ppropvals->count = 1;
			ppropvals->ppropval = common_util_alloc(sizeof(TAGGED_PROPVAL));
			if (NULL == ppropvals->ppropval) {
				return EC_OUT_OF_MEMORY;
			}
			ppropvals->ppropval->proptag = PROP_TAG_ROWID;
			ppropvals->ppropval->pvalue = (void*)&prow[i].row_id;
		} else {
			if (FALSE == common_util_modifyrecipient_to_propvals(
				pinfo->cpid, prow + i, pproptags, ppropvals)) {
				return EC_OUT_OF_MEMORY;	
			}
		}
		tmp_set.pparray[i] = ppropvals;
	}
	if (FALSE == message_object_set_rcpts(pmessage, &tmp_set)) {
		return EC_ERROR;
	}
	return EC_SUCCESS;
}

uint32_t rop_readrecipients(uint32_t row_id,
	uint16_t reserved, uint8_t *pcount, EXT_PUSH *pext,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	int i;
	int object_type;
	TARRAY_SET tmp_set;
	uint32_t last_offset;
	MESSAGE_OBJECT *pmessage;
	READRECIPIENT_ROW tmp_row;
	
	pmessage = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pmessage) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_MESSAGE != object_type) {
		return EC_NOT_SUPPORTED;
	}
	if (FALSE == message_object_read_recipients(
		pmessage, row_id, 0xFE, &tmp_set)) {
		return EC_ERROR;	
	}
	if (0 == tmp_set.count) {
		return EC_NOT_FOUND;
	}
	for (i=0; i<tmp_set.count; i++) {
		if (FALSE == common_util_propvals_to_readrecipient(
			message_object_get_cpid(pmessage), tmp_set.pparray[i],
			message_object_get_rcpt_columns(pmessage), &tmp_row)) {
			return EC_OUT_OF_MEMORY;
		}
		last_offset = pext->offset;
		if (EXT_ERR_SUCCESS != ext_buffer_push_readrecipient_row(
			pext, message_object_get_rcpt_columns(pmessage), &tmp_row)) {
			pext->offset = last_offset;
			break;
		}
	}
	if (0 == i) {
		return EC_BUFFER_TOO_SMALL;
	}
	*pcount = i;
	return EC_SUCCESS;
}

uint32_t rop_reloadcachedinformation(uint16_t reserved,
	uint8_t *phas_named_properties, TYPED_STRING *psubject_prefix,
	TYPED_STRING *pnormalized_subject, uint16_t *precipient_count,
	PROPTAG_ARRAY *precipient_columns, uint8_t *prow_count,
	OPENRECIPIENT_ROW **pprecipient_row, void *plogmap,
	uint8_t logon_id, uint32_t hin)
{
	int i;
	void *pvalue;
	int object_type;
	TARRAY_SET rcpts;
	PROPTAG_ARRAY proptags;
	TPROPVAL_ARRAY propvals;
	PROPTAG_ARRAY *pcolumns;
	uint32_t proptag_buff[3];
	MESSAGE_OBJECT *pmessage;
	
	pmessage = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pmessage) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_MESSAGE != object_type) {
		return EC_NOT_SUPPORTED;
	}
	proptags.count = 3;
	proptags.pproptag = proptag_buff;
	proptag_buff[0] = PROP_TAG_HASNAMEDPROPERTIES;
	proptag_buff[1] = PROP_TAG_SUBJECTPREFIX;
	proptag_buff[2] = PROP_TAG_NORMALIZEDSUBJECT;
	if (FALSE == message_object_get_properties(
		pmessage, 0, &proptags, &propvals)) {
		message_object_free(pmessage);
		return EC_ERROR;	
	}
	pvalue = common_util_get_propvals(&propvals,
				PROP_TAG_HASNAMEDPROPERTIES);
	if (NULL == pvalue || 0 == *(uint8_t*)pvalue) {
		*phas_named_properties = 1;
	} else {
		*phas_named_properties = 0;
	}
	pvalue = common_util_get_propvals(&propvals,
						PROP_TAG_SUBJECTPREFIX);
	if (NULL == pvalue) {
		psubject_prefix->string_type = STRING_TYPE_EMPTY;
		psubject_prefix->pstring = NULL;
	} else {
		psubject_prefix->string_type = STRING_TYPE_UNICODE;
		psubject_prefix->pstring = pvalue;
	}
	pvalue = common_util_get_propvals(&propvals,
					PROP_TAG_NORMALIZEDSUBJECT);
	if (NULL == pvalue) {
		pnormalized_subject->string_type = STRING_TYPE_EMPTY;
		pnormalized_subject->pstring = NULL;
	} else {
		pnormalized_subject->string_type = STRING_TYPE_UNICODE;
		pnormalized_subject->pstring = pvalue;
	}
	if (FALSE == message_object_get_recipient_num(
		pmessage, precipient_count)) {
		return EC_ERROR;
	}
	pcolumns = message_object_get_rcpt_columns(pmessage);
	*precipient_columns = *pcolumns;
	if (FALSE == message_object_read_recipients(
		pmessage, 0, 0xFE, &rcpts)) {
		return EC_ERROR;
	}
	*prow_count = rcpts.count;
	*pprecipient_row = common_util_alloc(
		sizeof(OPENRECIPIENT_ROW)*rcpts.count);
	if (NULL == *pprecipient_row) {
		return EC_OUT_OF_MEMORY;
	}
	for (i=0; i<rcpts.count; i++) {
		if (FALSE == common_util_propvals_to_openrecipient(
			message_object_get_cpid(pmessage), rcpts.pparray[i],
			pcolumns, *pprecipient_row + i)) {
			return EC_OUT_OF_MEMORY;
		}
	}
	return EC_SUCCESS;
}

uint32_t rop_setmessagestatus(uint64_t message_id,
	uint32_t message_status, uint32_t status_mask,
	uint32_t *pmessage_status, void *plogmap,
	uint8_t logon_id, uint32_t hin)
{
	void *pvalue;
	uint32_t result;
	int object_type;
	uint32_t new_status;
	LOGON_OBJECT *plogon;
	TAGGED_PROPVAL propval;
	PROPERTY_ROW message_row;
	uint32_t original_status;
	FLAGGED_PROPVAL *ppropval;
	
	plogon = rop_processor_get_logon_object(plogmap, logon_id);
	if (NULL == plogon) {
		return EC_ERROR;
	}
	if (NULL == rop_processor_get_object(
		plogmap, logon_id, hin, &object_type)) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_FOLDER != object_type) {
		return EC_NOT_SUPPORTED;
	}
	/* we do not check permission because it's maybe
		not an important property for the message.
		also, we don't know the message location */
	if (FALSE == exmdb_client_get_message_property(
		logon_object_get_dir(plogon), NULL, 0,
		message_id, PROP_TAG_MESSAGESTATUS, &pvalue)) {
		return EC_ERROR;
	}
	if (NULL == pvalue) {
		return EC_NOT_FOUND;
	}
	original_status = *(uint32_t*)pvalue;
	new_status = message_status & status_mask;
	if (new_status & MESSAGE_STATUS_IN_CONFLICT) {
		return EC_ACCESS_DENIED;
	}
	new_status |= original_status & ~(status_mask & ~new_status);
	*pmessage_status = new_status;
	propval.proptag = PROP_TAG_MESSAGESTATUS;
	propval.pvalue = &new_status;
	if (FALSE == exmdb_client_set_message_property(
		logon_object_get_dir(plogon), NULL, 0, message_id,
		&propval, &result)) {
		return EC_ERROR;
	}
	return result;
}

uint32_t rop_getmessagestatus(uint64_t message_id,
	uint32_t *pmessage_status, void *plogmap,
	uint8_t logon_id, uint32_t hin)
{
	void *pvalue;
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
	if (OBJECT_TYPE_FOLDER != object_type) {
		return EC_NOT_SUPPORTED;
	}
	if (FALSE == exmdb_client_get_message_property(
		logon_object_get_dir(plogon), NULL, 0,
		message_id, PROP_TAG_MESSAGESTATUS, &pvalue)) {
		return EC_ERROR;
	}
	if (NULL == pvalue) {
		return EC_NOT_FOUND;
	}
	*pmessage_status = *(uint32_t*)pvalue;
	return EC_SUCCESS;
}

static BOOL oxcmsg_setreadflag(LOGON_OBJECT *plogon,
	uint64_t message_id, uint8_t read_flag)
{
	void *pvalue;
	BOOL b_notify;
	BOOL b_changed;
	uint64_t read_cn;
	uint8_t tmp_byte;
	EMSMDB_INFO *pinfo;
	DCERPC_INFO rpc_info;
	const char *username;
	PROBLEM_ARRAY problems;
	MESSAGE_CONTENT *pbrief;
	TPROPVAL_ARRAY propvals;
	static uint8_t fake_false = 0;
	TAGGED_PROPVAL propval_buff[2];
	
	rpc_info = get_rpc_info();
	pinfo = emsmdb_interface_get_emsmdb_info();
	if (TRUE == logon_object_check_private(plogon)) {
		username = NULL;
	} else {
		username = rpc_info.username;
	}
	b_notify = FALSE;
	b_changed = FALSE;
	switch (read_flag) {
	case MSG_READ_FLAG_DEFAULT:
	case MSG_READ_FLAG_SUPPRESS_RECEIPT:
		if (FALSE == exmdb_client_get_message_property(
			logon_object_get_dir(plogon), username, 0,
			message_id, PROP_TAG_READ, &pvalue)) {
			return FALSE;	
		}
		if (NULL == pvalue || 0 == *(uint8_t*)pvalue) {
			tmp_byte = 1;
			b_changed = TRUE;
			if (MSG_READ_FLAG_DEFAULT == read_flag) {
				if (FALSE == exmdb_client_get_message_property(
					logon_object_get_dir(plogon), username, 0,
					message_id, PROP_TAG_READRECEIPTREQUESTED,
					&pvalue)) {
					return FALSE;
				}
				if (NULL != pvalue && 0 != *(uint8_t*)pvalue) {
					b_notify = TRUE;
				}
			}
		}
		break;
	case MSG_READ_FLAG_CLEAR_READ_FLAG:
		if (FALSE == exmdb_client_get_message_property(
			logon_object_get_dir(plogon), username, 0,
			message_id, PROP_TAG_READ, &pvalue)) {
			return FALSE;	
		}
		if (NULL != pvalue && 0 != *(uint8_t*)pvalue) {
			tmp_byte = 0;
			b_changed = TRUE;
		}
		break;
	case MSG_READ_FLAG_GENERATE_RECEIPT_ONLY:
		if (FALSE == exmdb_client_get_message_property(
			logon_object_get_dir(plogon), username, 0,
			message_id, PROP_TAG_READRECEIPTREQUESTED,
			&pvalue)) {
			return FALSE;
		}
		if (NULL != pvalue && 0 != *(uint8_t*)pvalue) {
			b_notify = TRUE;
		}
		break;
	case MSG_READ_FLAG_CLEAR_NOTIFY_READ:
	case MSG_READ_FLAG_CLEAR_NOTIFY_UNREAD:
	case MSG_READ_FLAG_CLEAR_NOTIFY_READ |
		MSG_READ_FLAG_CLEAR_NOTIFY_UNREAD:
		if ((read_flag & MSG_READ_FLAG_CLEAR_NOTIFY_READ) &&
			TRUE == exmdb_client_get_message_property(
			logon_object_get_dir(plogon), username, 0,
			message_id, PROP_TAG_READRECEIPTREQUESTED, &pvalue)
			&& NULL != pvalue && 0 != *(uint8_t*)pvalue) {
			if (FALSE == exmdb_client_remove_message_property(
				logon_object_get_dir(plogon), pinfo->cpid,
				message_id, PROP_TAG_READRECEIPTREQUESTED)) {
				return FALSE;	
			}
		}
		if ((read_flag & MSG_READ_FLAG_CLEAR_NOTIFY_UNREAD) &&
			TRUE == exmdb_client_get_message_property(
			logon_object_get_dir(plogon), username, 0,
			message_id, PROP_TAG_NONRECEIPTNOTIFICATIONREQUESTED,
			&pvalue) && NULL != pvalue && 0 != *(uint8_t*)pvalue) {
			if (FALSE == exmdb_client_remove_message_property(
				logon_object_get_dir(plogon), pinfo->cpid, message_id,
				PROP_TAG_NONRECEIPTNOTIFICATIONREQUESTED)) {
				return FALSE;	
			}
		}
		if (FALSE == exmdb_client_mark_modified(
			logon_object_get_dir(plogon), message_id)) {
			return FALSE;	
		}
		return TRUE;
	default:
		return TRUE;
	}
	if (TRUE == b_changed) {
		if (FALSE == exmdb_client_set_message_read_state(
			logon_object_get_dir(plogon), username,
			message_id, tmp_byte, &read_cn)) {
			return FALSE;
		}
	}
	if (TRUE == b_notify) {
		if (FALSE == exmdb_client_get_message_brief(
			logon_object_get_dir(plogon), pinfo->cpid,
			message_id, &pbrief)) {
			return FALSE;	
		}
		if (NULL != pbrief) {
			common_util_notify_receipt(
				logon_object_get_account(plogon),
				NOTIFY_RECEIPT_READ, pbrief);
		}
		propvals.count = 2;
		propvals.ppropval = propval_buff;
		propval_buff[0].proptag = PROP_TAG_READRECEIPTREQUESTED;
		propval_buff[0].pvalue = &fake_false;
		propval_buff[1].proptag = PROP_TAG_NONRECEIPTNOTIFICATIONREQUESTED;
		propval_buff[1].pvalue = &fake_false;
		exmdb_client_set_message_properties(
			logon_object_get_dir(plogon), username,
			0, message_id, &propvals, &problems);
	}
	return TRUE;
}

uint32_t rop_setreadflags(uint8_t want_asynchronous,
	uint8_t read_flags,	const LONGLONG_ARRAY *pmessage_ids,
	uint8_t *ppartial_completion, void *plogmap,
	uint8_t logon_id, uint32_t hin)
{
	int i;
	BOOL b_partial;
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
	if (OBJECT_TYPE_FOLDER != object_type) {
		return EC_NOT_SUPPORTED;
	}
	b_partial = FALSE;
	
	for (i=0; i<pmessage_ids->count; i++) {
		if (FALSE == oxcmsg_setreadflag(plogon,
			pmessage_ids->pll[i], read_flags)) {
			b_partial = TRUE;	
		}
	}
	if (TRUE == b_partial) {
		*ppartial_completion = 1;
	} else {
		*ppartial_completion = 0;
	}
	return EC_SUCCESS;
}

uint32_t rop_setmessagereadflag(uint8_t read_flags,
	const LONG_TERM_ID *pclient_data, uint8_t *pread_change,
	void *plogmap, uint8_t logon_id, uint32_t hresponse, uint32_t hin)
{
	BOOL b_changed;
	int object_type;
	LOGON_OBJECT *plogon;
	MESSAGE_OBJECT *pmessage;
	
	plogon = rop_processor_get_logon_object(plogmap, logon_id);
	if (NULL == plogon) {
		return EC_ERROR;
	}
	if (NULL == rop_processor_get_object(
		plogmap, logon_id, hresponse, &object_type)) {
		return EC_NULL_OBJECT;
	}
	pmessage = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pmessage) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_MESSAGE != object_type) {
		return EC_NOT_SUPPORTED;
	}
	if (FALSE == message_object_set_readflag(
		pmessage, read_flags, &b_changed)) {
		return EC_ERROR;	
	}
	if (FALSE == b_changed) {
		*pread_change = 0;
	} else {
		*pread_change = 1;
	}
	return EC_SUCCESS;
}

uint32_t rop_openattachment(uint8_t flags, uint32_t attachment_id,
	void *plogmap, uint8_t logon_id, uint32_t hin, uint32_t *phout)
{
	int object_type;
	uint32_t tag_access;
	LOGON_OBJECT *plogon;
	MESSAGE_OBJECT *pmessage;
	ATTACHMENT_OBJECT *pattachment;
	
	plogon = rop_processor_get_logon_object(plogmap, logon_id);
	if (NULL == plogon) {
		return EC_ERROR;
	}
	pmessage = rop_processor_get_object(
		plogmap, logon_id, hin, &object_type);
	if (NULL == pmessage) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_MESSAGE != object_type) {
		return EC_NOT_SUPPORTED;
	}
	if (flags & OPEN_MODE_FLAG_READWRITE) {
		tag_access = message_object_get_tag_access(pmessage);
		if (0 == (tag_access & TAG_ACCESS_MODIFY)) {
			if (flags & OPEN_MODE_FLAG_BESTACCESS) {
				flags &= ~OPEN_MODE_FLAG_BESTACCESS;
			} else {
				return EC_ACCESS_DENIED;
			}
		}
	}
	pattachment = attachment_object_create(
			pmessage, attachment_id, flags);
	if (NULL == pattachment) {
		return EC_ERROR;
	}
	if (0 == attachment_object_get_instance_id(pattachment)) {
		attachment_object_free(pattachment);
		return EC_NOT_FOUND;
	}
	*phout = rop_processor_add_object_handle(plogmap, logon_id,
					hin, OBJECT_TYPE_ATTACHMENT, pattachment);
	if (*phout < 0) {
		attachment_object_free(pattachment);
		return EC_ERROR;
	}
	return EC_SUCCESS;
}

uint32_t rop_createattachment(uint32_t *pattachment_id,
	void *plogmap, uint8_t logon_id, uint32_t hin, uint32_t *phout)
{
	int object_type;
	uint32_t tag_access;
	LOGON_OBJECT *plogon;
	MESSAGE_OBJECT *pmessage;
	ATTACHMENT_OBJECT *pattachment;
	
	plogon = rop_processor_get_logon_object(plogmap, logon_id);
	if (NULL == plogon) {
		return EC_ERROR;
	}
	pmessage = rop_processor_get_object(
		plogmap, logon_id, hin, &object_type);
	if (NULL == pmessage) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_MESSAGE != object_type) {
		return EC_NOT_SUPPORTED;
	}
	tag_access = message_object_get_tag_access(pmessage);
	if (0 == (tag_access & TAG_ACCESS_MODIFY)) {
		return EC_ACCESS_DENIED;
	}
	pattachment = attachment_object_create(pmessage,
		ATTACHMENT_NUM_INVALID, OPEN_MODE_FLAG_READWRITE);
	if (NULL == pattachment) {
		return EC_ERROR;
	}
	*pattachment_id = attachment_object_get_attachment_num(pattachment);
	if (ATTACHMENT_NUM_INVALID == *pattachment_id) {
		attachment_object_free(pattachment);
		return EC_ATTACHMENT_EXCEEDED;
	}
	if (FALSE == attachment_object_init_attachment(pattachment)) {
		attachment_object_free(pattachment);
		return EC_ERROR;
	}
	*phout = rop_processor_add_object_handle(plogmap, logon_id,
					hin, OBJECT_TYPE_ATTACHMENT, pattachment);
	if (*phout < 0) {
		attachment_object_free(pattachment);
		return EC_ERROR;
	}
	return EC_SUCCESS;
}

uint32_t rop_deleteattachment(uint32_t attachment_id,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	int object_type;
	uint32_t tag_access;
	MESSAGE_OBJECT *pmessage;
	
	pmessage = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pmessage) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_MESSAGE != object_type) {
		return EC_NOT_SUPPORTED;
	}
	tag_access = message_object_get_tag_access(pmessage);
	if (0 == (TAG_ACCESS_MODIFY & tag_access)) {
		return EC_ACCESS_DENIED;
	}
	if (FALSE == message_object_delele_attachment(
		pmessage, attachment_id)) {
		return EC_ERROR;
	}
	return EC_SUCCESS;
}

uint32_t rop_savechangesattachment(uint8_t save_flags,
	void *plogmap, uint8_t logon_id, uint32_t hresponse, uint32_t hin)
{
	int object_type;
	uint8_t open_flags;
	uint32_t tag_access;
	ATTACHMENT_OBJECT *pattachment;
	
	save_flags &= SAVE_FLAG_KEEPOPENREADONLY |
					SAVE_FLAG_KEEPOPENREADWRITE |
					SAVE_FLAG_FORCESAVE;
	if (NULL == rop_processor_get_object(plogmap,
		logon_id, hresponse, &object_type)) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_MESSAGE != object_type) {
		return EC_NOT_SUPPORTED;
	}
	pattachment = rop_processor_get_object(plogmap,
					logon_id, hin, &object_type);
	if (NULL == pattachment) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_ATTACHMENT != object_type) {
		return EC_NOT_SUPPORTED;
	}
	tag_access = attachment_object_get_tag_access(pattachment);
	if (0 == (TAG_ACCESS_MODIFY & tag_access)) {
		return EC_ACCESS_DENIED;
	}
	open_flags = attachment_object_get_open_flags(pattachment);
	if (0 == (open_flags & OPEN_MODE_FLAG_READWRITE) &&
		SAVE_FLAG_FORCESAVE != save_flags) {
		return EC_ACCESS_DENIED;
	}
	if (FALSE == attachment_object_save(pattachment)) {
		return EC_ERROR;
	}
	switch (save_flags) {
	case SAVE_FLAG_KEEPOPENREADWRITE:
	case SAVE_FLAG_FORCESAVE:
		open_flags = OPEN_MODE_FLAG_READWRITE;
		attachment_object_set_open_flags(pattachment, open_flags);
		break;
	}
	return EC_SUCCESS;
}

uint32_t rop_openembeddedmessage(uint16_t cpid,
	uint8_t open_embedded_flags, uint8_t *preserved,
	uint64_t *pmessage_id, uint8_t *phas_named_properties,
	TYPED_STRING *psubject_prefix, TYPED_STRING *pnormalized_subject,
	uint16_t *precipient_count, PROPTAG_ARRAY *precipient_columns,
	uint8_t *prow_count, OPENRECIPIENT_ROW **pprecipient_row,
	void *plogmap, uint8_t logon_id, uint32_t hin, uint32_t *phout)
{
	int i;
	void *pvalue;
	int object_type;
	TARRAY_SET rcpts;
	EMSMDB_INFO *pinfo;
	uint32_t tag_access;
	LOGON_OBJECT *plogon;
	PROPTAG_ARRAY proptags;
	TPROPVAL_ARRAY propvals;
	PROPTAG_ARRAY *pcolumns;
	uint32_t proptag_buff[4];
	MESSAGE_OBJECT *pmessage;
	ATTACHMENT_OBJECT *pattachment;
	
	*preserved = 0;
	if (0x0FFF == cpid) {
		pinfo = emsmdb_interface_get_emsmdb_info();
		if (NULL == pinfo) {
			return EC_ERROR;
		}
		cpid = pinfo->cpid;
	}
	if (FALSE == common_util_verify_cpid(cpid)) {
		return EC_INVALID_CODEPAGE;
	}
	plogon = rop_processor_get_logon_object(plogmap, logon_id);
	if (NULL == plogon) {
		return EC_ERROR;
	}
	pattachment = rop_processor_get_object(plogmap,
					logon_id, hin, &object_type);
	if (NULL == pattachment) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_ATTACHMENT != object_type) {
		return EC_NOT_SUPPORTED;
	}
	tag_access = attachment_object_get_tag_access(pattachment);
	if (0 == (tag_access & TAG_ACCESS_MODIFY) &&
		((OPEN_EMBEDDED_FLAG_READWRITE & open_embedded_flags))) {
		return EC_ACCESS_DENIED;
	}	
	pmessage = message_object_create(plogon, FALSE,
				cpid, 0, pattachment, tag_access,
				open_embedded_flags, NULL);
	if (NULL == pmessage) {
		return EC_ERROR;
	}
	if (0 == message_object_get_instance_id(pmessage)) {
		if (0 == (OPEN_EMBEDDED_FLAG_CREATE & open_embedded_flags)) {
			message_object_free(pmessage);
			return EC_NOT_FOUND;
		}
		message_object_free(pmessage);
		if (0 == (tag_access & TAG_ACCESS_MODIFY)) {
			return EC_ACCESS_DENIED;	
		}
		pmessage = message_object_create(plogon, TRUE,
					cpid, 0, pattachment, tag_access,
					OPEN_MODE_FLAG_READWRITE, NULL);
		if (NULL == pmessage) {
			return EC_ERROR;
		}
		if (FALSE == message_object_init_message(
			pmessage, FALSE, cpid)) {
			message_object_free(pmessage);
			return EC_ERROR;
		}
		proptags.count = 1;
		proptags.pproptag = proptag_buff;
		proptag_buff[0] = PROP_TAG_MID;
		if (FALSE == message_object_get_properties(
			pmessage, 0, &proptags, &propvals)) {
			message_object_free(pmessage);
			return EC_ERROR;	
		}
		pvalue = common_util_get_propvals(&propvals, PROP_TAG_MID);
		if (NULL == pvalue) {
			message_object_free(pmessage);
			return EC_ERROR;
		}
		*pmessage_id = *(uint64_t*)pvalue;
		*phout = rop_processor_add_object_handle(plogmap,
			logon_id, hin, OBJECT_TYPE_MESSAGE, pmessage);
		if (*phout < 0) {
			message_object_free(pmessage);
			return EC_ERROR;
		}
		*phas_named_properties = 0;
		psubject_prefix->string_type = STRING_TYPE_EMPTY;
		psubject_prefix->pstring = NULL;
		pnormalized_subject->string_type = STRING_TYPE_EMPTY;
		pnormalized_subject->pstring = NULL;
		precipient_columns->count = 0;
		precipient_columns->pproptag = NULL;
		*precipient_count = 0;
		*prow_count = 0;
		*pprecipient_row = NULL;
		return EC_SUCCESS;
	}
	proptags.count = 4;
	proptags.pproptag = proptag_buff;
	proptag_buff[0] = PROP_TAG_MID;
	proptag_buff[1] = PROP_TAG_HASNAMEDPROPERTIES;
	proptag_buff[2] = PROP_TAG_SUBJECTPREFIX;
	proptag_buff[3] = PROP_TAG_NORMALIZEDSUBJECT;
	if (FALSE == message_object_get_properties(
		pmessage, 0, &proptags, &propvals)) {
		message_object_free(pmessage);
		return EC_ERROR;
	}
	pvalue = common_util_get_propvals(&propvals, PROP_TAG_MID);
	if (NULL == pvalue) {
		message_object_free(pmessage);
		return EC_ERROR;
	}
	*pmessage_id = *(uint64_t*)pvalue;
	pvalue = common_util_get_propvals(&propvals,
				PROP_TAG_HASNAMEDPROPERTIES);
	if (NULL == pvalue || 0 == *(uint8_t*)pvalue) {
		*phas_named_properties = 1;
	} else {
		*phas_named_properties = 0;
	}
	pvalue = common_util_get_propvals(&propvals,
						PROP_TAG_SUBJECTPREFIX);
	if (NULL == pvalue) {
		psubject_prefix->string_type = STRING_TYPE_EMPTY;
		psubject_prefix->pstring = NULL;
	} else {
		psubject_prefix->string_type = STRING_TYPE_UNICODE;
		psubject_prefix->pstring = pvalue;
	}
	pvalue = common_util_get_propvals(&propvals,
					PROP_TAG_NORMALIZEDSUBJECT);
	if (NULL == pvalue) {
		pnormalized_subject->string_type = STRING_TYPE_EMPTY;
		pnormalized_subject->pstring = NULL;
	} else {
		pnormalized_subject->string_type = STRING_TYPE_UNICODE;
		pnormalized_subject->pstring = pvalue;
	}
	if (FALSE == message_object_get_recipient_num(
		pmessage, precipient_count)) {
		message_object_free(pmessage);
		return EC_ERROR;
	}
	pcolumns = message_object_get_rcpt_columns(pmessage);
	*precipient_columns = *pcolumns;
	if (FALSE == message_object_read_recipients(
		pmessage, 0, 0xFE, &rcpts)) {
		message_object_free(pmessage);
		return EC_ERROR;
	}
	*prow_count = rcpts.count;
	*pprecipient_row = common_util_alloc(
		sizeof(OPENRECIPIENT_ROW)*rcpts.count);
	if (NULL == *pprecipient_row) {
		message_object_free(pmessage);
		return EC_OUT_OF_MEMORY;
	}
	for (i=0; i<rcpts.count; i++) {
		if (FALSE == common_util_propvals_to_openrecipient(
			message_object_get_cpid(pmessage), rcpts.pparray[i],
			pcolumns, *pprecipient_row + i)) {
			message_object_free(pmessage);
			return EC_OUT_OF_MEMORY;
		}
	}
	*phout = rop_processor_add_object_handle(plogmap,
		logon_id, hin, OBJECT_TYPE_MESSAGE, pmessage);
	if (*phout < 0) {
		message_object_free(pmessage);
		return EC_ERROR;
	}
	return EC_SUCCESS;
}

uint32_t rop_getattachmenttable(uint8_t table_flags,
	void *plogmap, uint8_t logon_id, uint32_t hin, uint32_t *phout)
{
	int object_type;
	TABLE_OBJECT *ptable;
	LOGON_OBJECT *plogon;
	MESSAGE_OBJECT *pmessage;
	
	plogon = rop_processor_get_logon_object(plogmap, logon_id);
	if (NULL == plogon) {
		return EC_ERROR;
	}
	pmessage = rop_processor_get_object(plogmap,
				logon_id, hin, &object_type);
	if (NULL == pmessage) {
		return EC_NULL_OBJECT;
	}
	if (OBJECT_TYPE_MESSAGE != object_type) {
		return EC_NOT_SUPPORTED;
	}
	ptable = table_object_create(plogon, pmessage, table_flags,
							ROP_ID_GETATTACHMENTTABLE, logon_id);
	if (NULL == ptable) {
		return EC_OUT_OF_MEMORY;
	}
	*phout = rop_processor_add_object_handle(plogmap,
			logon_id, hin, OBJECT_TYPE_TABLE, ptable);
	if (*phout < 0) {
		table_object_free(ptable);
		return EC_ERROR;
	}
	table_object_set_handle(ptable, *phout);
	return EC_SUCCESS;
}

uint32_t rop_getvalidattachments(LONG_ARRAY *pattachment_ids,
	void *plogmap, uint8_t logon_id, uint32_t hin)
{
	/* just like exchange 2010 or later,
		we do not implement this rop */
	return EC_NOT_IMPLEMENTED;
}
