#include "attachment_object.h"
#include "fastupctx_object.h"
#include "emsmdb_interface.h"
#include "message_object.h"
#include "tpropval_array.h"
#include "folder_object.h"
#include "exmdb_client.h"
#include "common_util.h"
#include "tarray_set.h"
#include "rop_util.h"
#include <stdlib.h>


typedef struct _MARKER_NODE {
	DOUBLE_LIST_NODE node;
	uint32_t marker;
	union {
		void *pelement;
		uint32_t instance_id;
		uint64_t folder_id;
	} data;
} MARKER_NODE;


FASTUPCTX_OBJECT* fastupctx_object_create(
	LOGON_OBJECT *plogon, void *pobject, int root_element)
{
	FASTUPCTX_OBJECT *pctx;
	
	pctx = malloc(sizeof(FASTUPCTX_OBJECT));
	if (NULL == pctx) {
		return NULL;
	}
	pctx->pobject = pobject;
	pctx->b_ended = FALSE;
	pctx->root_element = root_element;
	pctx->pstream = ftstream_parser_create(plogon);
	if (NULL == pctx->pstream) {
		free(pctx);
		return NULL;
	}
	pctx->pproplist = NULL;
	pctx->pmsgctnt = NULL;
	switch (root_element) {
	case ROOT_ELEMENT_FOLDERCONTENT:
		pctx->pproplist = tpropval_array_init();
		if (NULL == pctx->pproplist) {
			ftstream_parser_free(pctx->pstream);
			free(pctx);
			return NULL;
		}
		break;
	case ROOT_ELEMENT_TOPFOLDER:
	case ROOT_ELEMENT_MESSAGECONTENT:
	case ROOT_ELEMENT_ATTACHMENTCONTENT:
	case ROOT_ELEMENT_MESSAGELIST:
		break;
	default:
		ftstream_parser_free(pctx->pstream);
		free(pctx);
		return NULL;
	}
	double_list_init(&pctx->marker_stack);
	return pctx;
}

void fastupctx_object_free(FASTUPCTX_OBJECT *pctx)
{
	DOUBLE_LIST_NODE *pnode;
	
	ftstream_parser_free(pctx->pstream);
	if (NULL != pctx->pproplist) {
		tpropval_array_free(pctx->pproplist);
	}
	if (NULL != pctx->pmsgctnt) {
		message_content_free(pctx->pmsgctnt);
	}
	while (pnode=double_list_get_from_head(
		&pctx->marker_stack)) {
		free(pnode->pdata);
	}
	double_list_free(&pctx->marker_stack);
	free(pctx);
}

static uint64_t fastupctx_object_get_last_folder(
	FASTUPCTX_OBJECT *pctx)
{
	DOUBLE_LIST_NODE *pnode;
	
	for (pnode=double_list_get_tail(&pctx->marker_stack); NULL!=pnode;
		pnode=double_list_get_before(&pctx->marker_stack, pnode)) {
		if (STARTSUBFLD == ((MARKER_NODE*)pnode->pdata)->marker) {
			return ((MARKER_NODE*)pnode->pdata)->data.folder_id;
		}
	}
	return folder_object_get_id(pctx->pobject);
}

static uint32_t fastupctx_object_get_last_attachment_instance(
	FASTUPCTX_OBJECT *pctx)
{
	DOUBLE_LIST_NODE *pnode;
	
	for (pnode=double_list_get_tail(&pctx->marker_stack); NULL!=pnode;
		pnode=double_list_get_before(&pctx->marker_stack, pnode)) {
		if (NEWATTACH == ((MARKER_NODE*)pnode->pdata)->marker) {
			return ((MARKER_NODE*)pnode->pdata)->data.instance_id;
		}
	}
	return attachment_object_get_instance_id(pctx->pobject);
}

static uint32_t fastupctx_object_get_last_message_instance(
	FASTUPCTX_OBJECT *pctx)
{
	DOUBLE_LIST_NODE *pnode;
	
	for (pnode=double_list_get_tail(&pctx->marker_stack); NULL!=pnode;
		pnode=double_list_get_before(&pctx->marker_stack, pnode)) {
		if (STARTEMBED == ((MARKER_NODE*)pnode->pdata)->marker) {
			return ((MARKER_NODE*)pnode->pdata)->data.instance_id;
		}
	}
	return message_object_get_instance_id(pctx->pobject);
}

static MESSAGE_CONTENT* fastupctx_object_get_last_message_element(
	FASTUPCTX_OBJECT *pctx)
{
	DOUBLE_LIST_NODE *pnode;
	
	for (pnode=double_list_get_tail(&pctx->marker_stack); NULL!=pnode;
		pnode=double_list_get_before(&pctx->marker_stack, pnode)) {
		if (STARTEMBED == ((MARKER_NODE*)pnode->pdata)->marker) {
			return ((MARKER_NODE*)pnode->pdata)->data.pelement;
		}
	}
	return pctx->pmsgctnt;
}

static BOOL fastupctx_object_create_folder(
	FASTUPCTX_OBJECT *pctx, uint64_t parent_id,
	TPROPVAL_ARRAY *pproplist, uint64_t *pfolder_id)
{
	XID tmp_xid;
	BOOL b_exist;
	BINARY *pbin;
	BINARY *pbin1;
	uint64_t tmp_id;
	BINARY *pentryid;
	uint32_t tmp_type;
	EMSMDB_INFO *pinfo;
	uint64_t change_num;
	uint32_t permission;
	DCERPC_INFO rpc_info;
	TAGGED_PROPVAL propval;
	PERMISSION_DATA permission_row;
	TAGGED_PROPVAL propval_buff[10];
	
	tpropval_array_remove_propval(pproplist, PROP_TAG_ACCESS);
	tpropval_array_remove_propval(pproplist, PROP_TAG_ACCESSLEVEL);
	tpropval_array_remove_propval(pproplist, PROP_TAG_ADDRESSBOOKENTRYID);
	tpropval_array_remove_propval(pproplist, PROP_TAG_ASSOCIATEDCONTENTCOUNT);
	tpropval_array_remove_propval(pproplist, PROP_TAG_ATTRIBUTEREADONLY);
	tpropval_array_remove_propval(pproplist, PROP_TAG_CONTENTCOUNT);
	tpropval_array_remove_propval(pproplist, PROP_TAG_CONTENTUNREADCOUNT);
	tpropval_array_remove_propval(pproplist, PROP_TAG_DELETEDCOUNTTOTAL);
	tpropval_array_remove_propval(pproplist, PROP_TAG_DELETEDFOLDERTOTAL);
	tpropval_array_remove_propval(pproplist, PROP_TAG_ARTICLENUMBERNEXT);
	tpropval_array_remove_propval(pproplist, PROP_TAG_INTERNETARTICLENUMBER);
	tpropval_array_remove_propval(pproplist, PROP_TAG_DISPLAYTYPE);
	tpropval_array_remove_propval(pproplist, PROP_TAG_DELETEDON);
	tpropval_array_remove_propval(pproplist, PROP_TAG_ENTRYID);
	tpropval_array_remove_propval(pproplist, PROP_TAG_FOLDERCHILDCOUNT);
	tpropval_array_remove_propval(pproplist, PROP_TAG_FOLDERFLAGS);
	tpropval_array_remove_propval(pproplist, PROP_TAG_FOLDERID);
	tpropval_array_remove_propval(pproplist, PROP_TAG_FOLDERTYPE);
	tpropval_array_remove_propval(pproplist, PROP_TAG_HASRULES);
	tpropval_array_remove_propval(pproplist, PROP_TAG_HIERARCHYCHANGENUMBER);
	tpropval_array_remove_propval(pproplist, PROP_TAG_LOCALCOMMITTIME);
	tpropval_array_remove_propval(pproplist, PROP_TAG_LOCALCOMMITTIMEMAX);
	tpropval_array_remove_propval(pproplist, PROP_TAG_MESSAGESIZE);
	tpropval_array_remove_propval(pproplist, PROP_TAG_MESSAGESIZEEXTENDED);
	tpropval_array_remove_propval(pproplist, PROP_TAG_NATIVEBODY);
	tpropval_array_remove_propval(pproplist, PROP_TAG_OBJECTTYPE);
	tpropval_array_remove_propval(pproplist, PROP_TAG_PARENTENTRYID);
	tpropval_array_remove_propval(pproplist, PROP_TAG_RECORDKEY);
	tpropval_array_remove_propval(pproplist, PROP_TAG_SEARCHKEY);
	tpropval_array_remove_propval(pproplist, PROP_TAG_STOREENTRYID);
	tpropval_array_remove_propval(pproplist, PROP_TAG_STORERECORDKEY);
	tpropval_array_remove_propval(pproplist, PROP_TAG_SOURCEKEY);
	tpropval_array_remove_propval(pproplist, PROP_TAG_PARENTSOURCEKEY);
	if (NULL == tpropval_array_get_propval(
		pproplist, PROP_TAG_DISPLAYNAME)) {
		return FALSE;
	}
	propval.proptag = PROP_TAG_FOLDERTYPE;
	propval.pvalue = &tmp_type;
	tmp_type = FOLDER_TYPE_GENERIC;
	if (FALSE == tpropval_array_set_propval(
		pproplist, &propval)) {
		return FALSE;
	}
	propval.proptag = PROP_TAG_PARENTFOLDERID;
	propval.pvalue = &parent_id;
	if (FALSE == tpropval_array_set_propval(
		pproplist, &propval)) {
		return FALSE;
	}
	if (FALSE == exmdb_client_allocate_cn(
		logon_object_get_dir(pctx->pstream->plogon),
		&change_num)) {
		return FALSE;
	}
	propval.proptag = PROP_TAG_CHANGENUMBER;
	propval.pvalue = &change_num;
	if (FALSE == tpropval_array_set_propval(
		pproplist, &propval)) {
		return FALSE;
	}
	if (TRUE == logon_object_check_private(
		pctx->pstream->plogon)) {
		tmp_xid.guid = rop_util_make_user_guid(
						logon_object_get_account_id(
						pctx->pstream->plogon));
	} else {
		tmp_xid.guid = rop_util_make_domain_guid(
					logon_object_get_account_id(
					pctx->pstream->plogon));
	}
	rop_util_get_gc_array(change_num, tmp_xid.local_id);
	pbin = common_util_xid_to_binary(22, &tmp_xid);
	if (NULL == pbin) {
		return FALSE;
	}
	propval.proptag = PROP_TAG_CHANGEKEY;
	propval.pvalue = pbin;
	if (FALSE == tpropval_array_set_propval(
		pproplist, &propval)) {
		return FALSE;
	}
	pbin1 = tpropval_array_get_propval(pproplist,
				PROP_TAG_PREDECESSORCHANGELIST);
	propval.proptag = PROP_TAG_PREDECESSORCHANGELIST;
	propval.pvalue = common_util_pcl_append(pbin1, pbin);
	if (NULL == propval.pvalue) {
		return FALSE;
	}
	if (FALSE == tpropval_array_set_propval(
		pproplist, &propval)) {
		return FALSE;
	}
	pinfo = emsmdb_interface_get_emsmdb_info();
	if (FALSE == exmdb_client_create_folder_by_properties(
		logon_object_get_dir(pctx->pstream->plogon),
		pinfo->cpid, pproplist, pfolder_id) ||
		0 == *pfolder_id) {
		return FALSE;
	}
	if (LOGON_MODE_OWNER != logon_object_get_mode(
		pctx->pstream->plogon)) {
		rpc_info = get_rpc_info();
		pentryid = common_util_username_to_addressbook_entryid(
											rpc_info.username);
		if (NULL != pentryid) {
			tmp_id = 1;
			permission = PERMISSION_FOLDEROWNER|PERMISSION_READANY|
						PERMISSION_FOLDERVISIBLE|PERMISSION_CREATE|
						PERMISSION_EDITANY|PERMISSION_DELETEANY|
						PERMISSION_CREATESUBFOLDER;
			permission_row.flags = PERMISSION_DATA_FLAG_ADD_ROW;
			permission_row.propvals.count = 3;
			permission_row.propvals.ppropval = propval_buff;
			propval_buff[0].proptag = PROP_TAG_ENTRYID;
			propval_buff[0].pvalue = pentryid;
			propval_buff[1].proptag = PROP_TAG_MEMBERID;
			propval_buff[1].pvalue = &tmp_id;
			propval_buff[2].proptag = PROP_TAG_MEMBERRIGHTS;
			propval_buff[2].pvalue = &permission;
			exmdb_client_update_folder_permission(
				logon_object_get_dir(pctx->pstream->plogon),
				*pfolder_id, FALSE, 1, &permission_row);
		}
	}
	return TRUE;
}

static BOOL fastupctx_object_empty_folder(
	FASTUPCTX_OBJECT *pctx, uint64_t folder_id,
	BOOL b_normal, BOOL b_fai, BOOL b_sub)
{
	BOOL b_partial;
	EMSMDB_INFO *pinfo;
	const char *username;
	DCERPC_INFO rpc_info;
	
	if (LOGON_MODE_OWNER == logon_object_get_mode(
		pctx->pstream->plogon)) {
		username = NULL;	
	} else {
		rpc_info = get_rpc_info();
		username = rpc_info.username;
	}
	pinfo = emsmdb_interface_get_emsmdb_info();
	if (FALSE == exmdb_client_empty_folder(logon_object_get_dir(
		pctx->pstream->plogon), pinfo->cpid, username, folder_id,
		TRUE, b_normal, b_fai, b_sub, &b_partial)) {
		return FALSE;	
	}
	if (TRUE == b_partial) {
		return FALSE;
	}
	return TRUE;
}

static BOOL fastupctx_object_write_message(
	FASTUPCTX_OBJECT *pctx, uint64_t folder_id)
{
	XID tmp_xid;
	BINARY *pbin;
	BINARY *pbin1;
	BOOL b_result;
	EMSMDB_INFO *pinfo;
	uint64_t change_num;
	TAGGED_PROPVAL propval;
	TPROPVAL_ARRAY *pproplist;
	
	pproplist = message_content_get_proplist(pctx->pmsgctnt);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_CONVERSATIONID);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_DISPLAYTO);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_DISPLAYTO_STRING8);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_DISPLAYCC);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_DISPLAYCC_STRING8);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_DISPLAYBCC);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_DISPLAYBCC_STRING8);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_MID);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_MESSAGESIZE);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_MESSAGESIZEEXTENDED);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_HASNAMEDPROPERTIES);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_HASATTACHMENTS);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_ENTRYID);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_FOLDERID);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_OBJECTTYPE);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_PARENTENTRYID);
	tpropval_array_remove_propval(
		pproplist, PROP_TAG_STORERECORDKEY);
	if (FALSE == exmdb_client_allocate_cn(
		logon_object_get_dir(pctx->pstream->plogon),
		&change_num)) {
		return FALSE;
	}
	propval.proptag = PROP_TAG_CHANGENUMBER;
	propval.pvalue = &change_num;
	if (FALSE == tpropval_array_set_propval(
		pproplist, &propval)) {
		return FALSE;
	}
	if (TRUE == logon_object_check_private(
		pctx->pstream->plogon)) {
		tmp_xid.guid = rop_util_make_user_guid(
						logon_object_get_account_id(
						pctx->pstream->plogon));
	} else {
		tmp_xid.guid = rop_util_make_domain_guid(
						logon_object_get_account_id(
						pctx->pstream->plogon));
	}
	rop_util_get_gc_array(change_num, tmp_xid.local_id);
	pbin = common_util_xid_to_binary(22, &tmp_xid);
	if (NULL == pbin) {
		return FALSE;
	}
	propval.proptag = PROP_TAG_CHANGEKEY;
	propval.pvalue = pbin;
	if (FALSE == tpropval_array_set_propval(
		pproplist, &propval)) {
		return FALSE;
	}
	pbin1 = tpropval_array_get_propval(pproplist,
				PROP_TAG_PREDECESSORCHANGELIST);
	propval.proptag = PROP_TAG_PREDECESSORCHANGELIST;
	propval.pvalue = common_util_pcl_append(pbin1, pbin);
	if (NULL == propval.pvalue) {
		return FALSE;
	}
	if (FALSE == tpropval_array_set_propval(
		pproplist, &propval)) {
		return FALSE;
	}
	pinfo = emsmdb_interface_get_emsmdb_info();
	if (FALSE == exmdb_client_write_message(
		logon_object_get_dir(pctx->pstream->plogon),
		logon_object_get_account(pctx->pstream->plogon),
		pinfo->cpid, folder_id, pctx->pmsgctnt,
		&b_result) || FALSE == b_result) {
		return FALSE;
	}
	return TRUE;
}

static BOOL fastupctx_object_record_marker(
	FASTUPCTX_OBJECT *pctx, uint32_t marker)
{
	BOOL b_result;
	uint32_t tmp_id;
	uint32_t tmp_num;
	uint8_t tmp_byte;
	TARRAY_SET *prcpts;
	uint64_t parent_id;
	uint64_t folder_id;
	EMSMDB_INFO *pinfo;
	uint32_t instance_id;
	TARRAY_SET tmp_rcpts;
	MARKER_NODE *pmarker;
	uint32_t last_marker;
	TPROPVAL_ARRAY *prcpt;
	PROPTAG_ARRAY proptags;
	TAGGED_PROPVAL propval;
	TPROPVAL_ARRAY propvals;
	DOUBLE_LIST_NODE *pnode;
	DOUBLE_LIST_NODE *pnode1;
	uint32_t proptag_buff[2];
	MESSAGE_CONTENT *pmsgctnt;
	TPROPVAL_ARRAY *pproplist;
	PROBLEM_ARRAY tmp_problems;
	ATTACHMENT_LIST *pattachments;
	ATTACHMENT_CONTENT *pattachment;
	
	pnode = double_list_get_tail(&pctx->marker_stack);
	if (NULL == pnode) {
		last_marker = 0;
	} else {
		last_marker = ((MARKER_NODE*)pnode->pdata)->marker;
	}
	switch (last_marker) {
	case STARTSUBFLD:
		if (NULL == pctx->pproplist) {
			break;
		}
		if (0 == pctx->pproplist->count) {
			return FALSE;
		}
		pnode1 = double_list_get_before(&pctx->marker_stack, pnode);
		if (NULL == pnode1) {
			parent_id = folder_object_get_id(pctx->pobject);
		} else {
			parent_id = ((MARKER_NODE*)pnode1->pdata)->data.folder_id;
		}
		if (FALSE == fastupctx_object_create_folder(pctx,
			parent_id, pctx->pproplist, &folder_id)) {
			return FALSE;
		}
		tpropval_array_free(pctx->pproplist);
		pctx->pproplist = NULL;
		((MARKER_NODE*)pnode->pdata)->data.folder_id = folder_id;
		break;
	case STARTTOPFLD:
		if (NULL == pctx->pproplist) {
			break;
		}
		if (pctx->pproplist->count > 0) {
			if (FALSE == folder_object_set_properties(
				pctx->pobject, pctx->pproplist, &tmp_problems)) {
				return FALSE;
			}
		}
		tpropval_array_free(pctx->pproplist);
		pctx->pproplist = NULL;
		break;
	case 0:
		if (ROOT_ELEMENT_FOLDERCONTENT == pctx->root_element) {
			if (NULL == pctx->pproplist) {
				break;
			}
			if (pctx->pproplist->count > 0) {
				if (FALSE == folder_object_set_properties(
					pctx->pobject, pctx->pproplist, &tmp_problems)) {
					return FALSE;
				}
			}
			tpropval_array_free(pctx->pproplist);
			pctx->pproplist = NULL;
		}
		break;
	}
	switch (marker) {
	case STARTTOPFLD:
		if (ROOT_ELEMENT_TOPFOLDER !=
			pctx->root_element || 0 != last_marker) {
			return FALSE;	
		}
		if (NULL != pctx->pproplist) {
			return FALSE;
		}
		pctx->pproplist = tpropval_array_init();
		if (NULL == pctx->pproplist) {
			return FALSE;
		}
		pmarker = malloc(sizeof(MARKER_NODE));
		if (NULL == pmarker) {
			return FALSE;
		}
		pmarker->node.pdata = pmarker;
		pmarker->marker = marker;
		break;
	case STARTSUBFLD:
		if (ROOT_ELEMENT_TOPFOLDER != pctx->root_element &&
			ROOT_ELEMENT_FOLDERCONTENT != pctx->root_element) {
			return FALSE;	
		}
		if (NULL != pctx->pproplist) {
			return FALSE;
		}
		pctx->pproplist = tpropval_array_init();
		if (NULL == pctx->pproplist) {
			return FALSE;
		}
		pmarker = malloc(sizeof(MARKER_NODE));
		if (NULL == pmarker) {
			return FALSE;
		}
		pmarker->node.pdata = pmarker;
		pmarker->marker = marker;
		break;
	case ENDFOLDER:
		if (STARTTOPFLD != last_marker &&
			STARTSUBFLD != last_marker) {
			return FALSE;	
		}
		double_list_remove(&pctx->marker_stack, pnode);
		free(pnode->pdata);
		if (STARTTOPFLD == last_marker) {
			/* mark fast stream ended */
			pctx->b_ended = TRUE;
		}
		return TRUE;
	case STARTFAIMSG:
	case STARTMESSAGE:
		switch (pctx->root_element) {
		case ROOT_ELEMENT_MESSAGELIST:
			if (0 != last_marker) {
				return FALSE;
			}
			break;
		case ROOT_ELEMENT_TOPFOLDER:
			if (STARTTOPFLD != last_marker &&
				STARTSUBFLD != last_marker) {
				return FALSE;
			}
			break;
		case ROOT_ELEMENT_FOLDERCONTENT:
			break;
		default:
			return FALSE;
		}
		if (NULL != pctx->pmsgctnt) {
			return FALSE;
		}
		pctx->pmsgctnt = message_content_init();
		if (NULL == pctx->pmsgctnt) {
			return FALSE;
		}
		prcpts = tarray_set_init();
		if (NULL == prcpts) {
			return FALSE;
		}
		message_content_set_rcpts_internal(pctx->pmsgctnt, prcpts);
		pattachments = attachment_list_init();
		if (NULL == pattachments) {
			return FALSE;
		}
		message_content_set_attachments_internal(
					pctx->pmsgctnt, pattachments);
		pproplist = message_content_get_proplist(pctx->pmsgctnt);
		propval.proptag = PROP_TAG_ASSOCIATED;
		propval.pvalue = &tmp_byte;
		if (STARTFAIMSG == marker) {
			tmp_byte = 1;
		} else {
			tmp_byte = 0;
		}
		if (FALSE == tpropval_array_set_propval(
			pproplist, &propval)) {
			return FALSE;	
		}
		pmarker = malloc(sizeof(MARKER_NODE));
		if (NULL == pmarker) {
			return FALSE;
		}
		pmarker->node.pdata = pmarker;
		pmarker->marker = marker;
		pmarker->data.pelement = pctx->pmsgctnt;
		break;
	case ENDMESSAGE:
		if (STARTMESSAGE != last_marker &&
			STARTFAIMSG != last_marker) {
			return FALSE;	
		}
		if (NULL == pctx->pmsgctnt || pctx->pmsgctnt !=
			((MARKER_NODE*)pnode->pdata)->data.pelement) {
			return FALSE;	
		}
		double_list_remove(&pctx->marker_stack, pnode);
		free(pnode->pdata);
		folder_id = fastupctx_object_get_last_folder(pctx);
		if (FALSE == fastupctx_object_write_message(pctx, folder_id)) {
			return FALSE;
		}
		message_content_free(pctx->pmsgctnt);
		pctx->pmsgctnt = NULL;
		return TRUE;
	case STARTRECIP:
		switch (pctx->root_element) {
		case ROOT_ELEMENT_MESSAGECONTENT:
			switch (last_marker) {
			case 0:
			case STARTEMBED:
				break;
			default:
				return FALSE;
			}
			break;
		case ROOT_ELEMENT_ATTACHMENTCONTENT:
			if (STARTEMBED != last_marker) {
				return FALSE;
			}
			break;
		default:
			if (STARTMESSAGE != last_marker &&
				STARTFAIMSG != last_marker &&
				STARTEMBED != last_marker) {
				return FALSE;	
			}
			break;
		}
		if (ROOT_ELEMENT_MESSAGECONTENT == pctx->root_element ||
			ROOT_ELEMENT_ATTACHMENTCONTENT == pctx->root_element) {
			if (NULL != pctx->pproplist) {
				return FALSE;
			}
			pctx->pproplist = tpropval_array_init();
			if (NULL == pctx->pproplist) {
				return FALSE;
			}
		} else {
			prcpt = tpropval_array_init();
			if (NULL == prcpt) {
				return FALSE;
			}
			pmsgctnt = ((MARKER_NODE*)pnode->pdata)->data.pelement;
			if (FALSE == tarray_set_append_internal(
				pmsgctnt->children.prcpts, prcpt)) {
				tpropval_array_free(prcpt);
				return FALSE;
			}
		}
		pmarker = malloc(sizeof(MARKER_NODE));
		if (NULL == pmarker) {
			return FALSE;
		}
		pmarker->node.pdata = pmarker;
		pmarker->marker = marker;
		if (ROOT_ELEMENT_MESSAGECONTENT == pctx->root_element ||
			ROOT_ELEMENT_ATTACHMENTCONTENT == pctx->root_element) {
			pmarker->data.instance_id =
				fastupctx_object_get_last_message_instance(pctx);
		} else {
			pmarker->data.pelement = prcpt;
		}
		break;
	case ENDTORECIP:
		if (STARTRECIP != last_marker) {
			return FALSE;
		}
		if (ROOT_ELEMENT_MESSAGECONTENT == pctx->root_element ||
			ROOT_ELEMENT_ATTACHMENTCONTENT == pctx->root_element) {
			tmp_rcpts.count = 1;
			tmp_rcpts.pparray = &pctx->pproplist;
			pinfo = emsmdb_interface_get_emsmdb_info();
			if (FALSE == exmdb_client_update_message_instance_rcpts(
				logon_object_get_dir(pctx->pstream->plogon),
				((MARKER_NODE*)pnode->pdata)->data.instance_id,
				&tmp_rcpts)) {
				return FALSE;
			}
			tpropval_array_free(pctx->pproplist);
			pctx->pproplist = NULL;
		}
		double_list_remove(&pctx->marker_stack, pnode);
		free(pnode->pdata);
		return TRUE;
	case NEWATTACH:
		switch (pctx->root_element) {
		case ROOT_ELEMENT_MESSAGECONTENT:
			switch (last_marker) {
			case 0:
			case STARTEMBED:
				break;
			default:
				return FALSE;
			}
			break;
		case ROOT_ELEMENT_ATTACHMENTCONTENT:
			if (STARTEMBED != last_marker) {
				return FALSE;
			}
			break;
		default:
			if (STARTMESSAGE != last_marker &&
				STARTFAIMSG != last_marker &&
				STARTEMBED != last_marker) {
				return FALSE;	
			}
			break;
		}
		if (ROOT_ELEMENT_MESSAGECONTENT == pctx->root_element ||
			ROOT_ELEMENT_ATTACHMENTCONTENT == pctx->root_element) {
			instance_id = fastupctx_object_get_last_message_instance(pctx);
			if (FALSE == exmdb_client_create_attachment_instance(
				logon_object_get_dir(pctx->pstream->plogon),
				instance_id, &tmp_id, &tmp_num) || 0 == tmp_id) {
				return FALSE;
			}
		} else {
			pattachment = attachment_content_init();
			if (NULL == pattachment) {
				return FALSE;
			}
			pmsgctnt = ((MARKER_NODE*)pnode->pdata)->data.pelement;
			if (FALSE == attachment_list_append_internal(
				pmsgctnt->children.pattachments, pattachment)) {
				attachment_content_free(pattachment);
				return FALSE;
			}
		}
		pmarker = malloc(sizeof(MARKER_NODE));
		if (NULL == pmarker) {
			return FALSE;
		}
		pmarker->node.pdata = pmarker;
		pmarker->marker = marker;
		if (ROOT_ELEMENT_MESSAGECONTENT == pctx->root_element ||
			ROOT_ELEMENT_ATTACHMENTCONTENT == pctx->root_element) {
			pmarker->data.instance_id = tmp_id;
		} else {
			pmarker->data.pelement = pattachment;
		}
		break;
	case ENDATTACH:
		if (NEWATTACH != last_marker) {
			return FALSE;
		}
		if (ROOT_ELEMENT_MESSAGECONTENT == pctx->root_element ||
			ROOT_ELEMENT_ATTACHMENTCONTENT == pctx->root_element) {
			if (FALSE == exmdb_client_flush_instance(
				logon_object_get_dir(pctx->pstream->plogon),
				((MARKER_NODE*)pnode->pdata)->data.instance_id,
				NULL, &b_result) || FALSE == b_result) {
				return FALSE;	
			}
			if (FALSE == exmdb_client_unload_instance(
				logon_object_get_dir(pctx->pstream->plogon),
				((MARKER_NODE*)pnode->pdata)->data.instance_id)) {
				return FALSE;	
			}
		}
		double_list_remove(&pctx->marker_stack, pnode);
		free(pnode->pdata);
		return TRUE;
	case STARTEMBED:
		if (ROOT_ELEMENT_MESSAGECONTENT == pctx->root_element ||
			ROOT_ELEMENT_ATTACHMENTCONTENT == pctx->root_element) {
			if (ROOT_ELEMENT_MESSAGECONTENT == pctx->root_element) {
				if (NEWATTACH != last_marker) {
					return FALSE;
				}
			} else {
				if (0 != last_marker && NEWATTACH != last_marker) {
					return FALSE;
				}
			}
			instance_id = fastupctx_object_get_last_attachment_instance(pctx);
			if (FALSE == exmdb_client_load_embedded_instance(
				logon_object_get_dir(pctx->pstream->plogon),
				FALSE, instance_id, &tmp_id)) {
				return FALSE;
			}
			if (0 == tmp_id) {
				if (FALSE == exmdb_client_load_embedded_instance(
					logon_object_get_dir(pctx->pstream->plogon),
					TRUE, instance_id, &tmp_id) || 0 == tmp_id) {
					return FALSE;
				}
			} else {
				if (FALSE == exmdb_client_clear_message_instance(
					logon_object_get_dir(pctx->pstream->plogon),
					instance_id)) {
					return FALSE;	
				}
			}
		} else {
			if (NEWATTACH != last_marker) {
				return FALSE;
			}
			pmsgctnt = message_content_init();
			if (NULL == pmsgctnt) {
				return FALSE;
			}
			prcpts = tarray_set_init();
			if (NULL == prcpts) {
				message_content_free(pmsgctnt);
				return FALSE;
			}
			message_content_set_rcpts_internal(pmsgctnt, prcpts);
			pattachments = attachment_list_init();
			if (NULL == pattachments) {
				message_content_free(pmsgctnt);
				return FALSE;
			}
			message_content_set_attachments_internal(
							pmsgctnt, pattachments);
			attachment_content_set_embeded_internal(
				((MARKER_NODE*)pnode->pdata)->data.pelement, pmsgctnt);
		}
		pmarker = malloc(sizeof(MARKER_NODE));
		if (NULL == pmarker) {
			return FALSE;
		}
		pmarker->node.pdata = pmarker;
		pmarker->marker = marker;
		if (ROOT_ELEMENT_MESSAGECONTENT == pctx->root_element ||
			ROOT_ELEMENT_ATTACHMENTCONTENT == pctx->root_element) {
			pmarker->data.instance_id = tmp_id;
		} else {
			pmarker->data.pelement = pmsgctnt;
		}
		break;
	case ENDEMBED:
		if (STARTEMBED != last_marker) {
			return FALSE;	
		}
		if (ROOT_ELEMENT_MESSAGECONTENT == pctx->root_element ||
			ROOT_ELEMENT_ATTACHMENTCONTENT == pctx->root_element) {
			if (FALSE == exmdb_client_flush_instance(
				logon_object_get_dir(pctx->pstream->plogon),
				((MARKER_NODE*)pnode->pdata)->data.instance_id,
				NULL, &b_result) || FALSE == b_result) {
				return FALSE;	
			}
			if (FALSE == exmdb_client_unload_instance(
				logon_object_get_dir(pctx->pstream->plogon),
				((MARKER_NODE*)pnode->pdata)->data.instance_id)) {
				return FALSE;	
			}
		}
		double_list_remove(&pctx->marker_stack, pnode);
		free(pnode->pdata);
		return TRUE;
	case FXERRORINFO:
		/* not support this feature */
		return FALSE;
	default:
		return FALSE;
	}
	double_list_append_as_tail(&pctx->marker_stack, &pmarker->node);
	return TRUE;
}

static BOOL fastupctx_object_del_props(
	FASTUPCTX_OBJECT *pctx, uint32_t marker)
{
	int instance_id;
	uint32_t last_marker;
	DOUBLE_LIST_NODE *pnode;
	MESSAGE_CONTENT *pmsgctnt;
	
	pnode = double_list_get_tail(&pctx->marker_stack);
	if (NULL == pnode) {
		last_marker = 0;
	} else {
		last_marker = ((MARKER_NODE*)pnode->pdata)->marker;
	}
	switch (marker) {
	case PROP_TAG_MESSAGERECIPIENTS:
		switch (pctx->root_element) {
		case ROOT_ELEMENT_MESSAGECONTENT:
			switch (last_marker) {
			case 0:
				instance_id =
					fastupctx_object_get_last_message_instance(pctx);
				if (FALSE == exmdb_client_empty_message_instance_rcpts(
					logon_object_get_dir(pctx->pstream->plogon),
					instance_id)) {
					return FALSE;	
				}
			case STARTEMBED:
				break;
			default:
				return FALSE;
			}
			break;
		case ROOT_ELEMENT_ATTACHMENTCONTENT:
			if (STARTEMBED != last_marker) {
				return FALSE;
			}
			break;
		default:
			if (STARTMESSAGE != last_marker &&
				STARTFAIMSG != last_marker &&
				STARTEMBED != last_marker) {
				return FALSE;	
			}
			pmsgctnt = ((MARKER_NODE*)pnode->pdata)->data.pelement;
			if (0 != pmsgctnt->children.prcpts->count) {
				return FALSE;
			}
			break;
		}
		break;
	case PROP_TAG_MESSAGEATTACHMENTS:
		switch (pctx->root_element) {
		case ROOT_ELEMENT_MESSAGECONTENT:
			switch (last_marker) {
			case 0:
				if (FALSE == exmdb_client_empty_message_instance_attachments(
					logon_object_get_dir(pctx->pstream->plogon),
					fastupctx_object_get_last_message_instance(pctx))) {
					return FALSE;
				}
				break;
			case STARTEMBED:
				break;
			default:
				return FALSE;
			}
			break;
		case ROOT_ELEMENT_ATTACHMENTCONTENT:
			if (STARTEMBED != last_marker) {
				return FALSE;
			}
			break;
		default:
			if (STARTMESSAGE != last_marker &&
				STARTFAIMSG != last_marker &&
				STARTEMBED != last_marker) {
				return FALSE;	
			}
			pmsgctnt = ((MARKER_NODE*)pnode->pdata)->data.pelement;
			if (0 != pmsgctnt->children.pattachments->count) {
				return FALSE;
			}
			break;
		}
		break;
	case PROP_TAG_CONTAINERCONTENTS:
		if (ROOT_ELEMENT_FOLDERCONTENT != pctx->root_element ||
			(STARTSUBFLD != last_marker && 0 != last_marker)) {
			return FALSE;	
		}
		if (0 == last_marker) {
			if (FALSE == fastupctx_object_empty_folder(
				pctx, folder_object_get_id(pctx->pobject),
				TRUE, FALSE, FALSE)) {
				return FALSE;	
			}
		}
		break;
	case PROP_TAG_FOLDERASSOCIATEDCONTENTS:
		if (ROOT_ELEMENT_FOLDERCONTENT != pctx->root_element ||
			(STARTSUBFLD != last_marker && 0 != last_marker)) {
			return FALSE;	
		}
		if (0 == last_marker) {
			if (FALSE == fastupctx_object_empty_folder(
				pctx, folder_object_get_id(pctx->pobject),
				FALSE, TRUE, FALSE)) {
				return FALSE;	
			}
		}
		break;
	case PROP_TAG_CONTAINERHIERARCHY:
		if (ROOT_ELEMENT_FOLDERCONTENT != pctx->root_element ||
			(STARTSUBFLD != last_marker && 0 != last_marker)) {
			return FALSE;	
		}
		if (0 == last_marker) {
			if (FALSE == fastupctx_object_empty_folder(
				pctx, folder_object_get_id(pctx->pobject),
				FALSE, FALSE, TRUE)) {
				return FALSE;	
			}
		}
		break;
	}
	return TRUE;
}

static BOOL fastupctx_object_record_propval(
	FASTUPCTX_OBJECT *pctx, const TAGGED_PROPVAL *ppropval)
{
	BOOL b_result;
	EMSMDB_INFO *pinfo;
	uint32_t instance_id;
	uint32_t last_marker;
	MARKER_NODE *pmarker;
	DOUBLE_LIST_NODE *pnode;
	MESSAGE_CONTENT *pmsgctnt;
	
	switch (ppropval->proptag) {
	case META_TAG_FXDELPROP:
		switch (*(uint32_t*)ppropval->pvalue) {
		case PROP_TAG_MESSAGERECIPIENTS:
		case PROP_TAG_MESSAGEATTACHMENTS:
		case PROP_TAG_CONTAINERCONTENTS:
		case PROP_TAG_FOLDERASSOCIATEDCONTENTS:
		case PROP_TAG_CONTAINERHIERARCHY:
			return fastupctx_object_del_props(pctx,
					*(uint32_t*)ppropval->pvalue);
		default:
			return FALSE;
		}
	case META_TAG_DNPREFIX:
	case META_TAG_ECWARNING:
		return TRUE;
	case META_TAG_NEWFXFOLDER:
	case META_TAG_INCRSYNCGROUPID:
	case META_TAG_INCREMENTALSYNCMESSAGEPARTIAL:
	case META_TAG_IDSETGIVEN:
	case META_TAG_IDSETGIVEN1:
	case META_TAG_CNSETSEEN:
	case META_TAG_CNSETSEENFAI:
	case META_TAG_CNSETREAD:
	case META_TAG_IDSETDELETED:
	case META_TAG_IDSETNOLONGERINSCOPE:
	case META_TAG_IDSETEXPIRED:
	case META_TAG_IDSETREAD:
	case META_TAG_IDSETUNREAD:
		return FALSE;
	}
	pnode = double_list_get_tail(&pctx->marker_stack);
	if (NULL != pnode) {
		last_marker = ((MARKER_NODE*)pnode->pdata)->marker;
	} else {
		last_marker = 0;
	}
	if (PROPVAL_TYPE_OBJECT == (ppropval->proptag & 0xFFFF)) {
		if (NEWATTACH == last_marker || (0 == last_marker &&
			ROOT_ELEMENT_ATTACHMENTCONTENT == pctx->root_element)) {
			if (PROP_TAG_ATTACHDATAOBJECT != ppropval->proptag) {
				return FALSE;
			}
		} else {
			return FALSE;
		}
	}
	switch (last_marker) {
	case 0:
		switch (pctx->root_element) {
		case ROOT_ELEMENT_FOLDERCONTENT:
			return tpropval_array_set_propval(
					pctx->pproplist, ppropval);
		case ROOT_ELEMENT_MESSAGECONTENT:
			pinfo = emsmdb_interface_get_emsmdb_info();
			return exmdb_client_set_instance_property(
					logon_object_get_dir(pctx->pstream->plogon),
					message_object_get_instance_id(pctx->pobject),
					ppropval, &b_result);
		case ROOT_ELEMENT_ATTACHMENTCONTENT:
			pinfo = emsmdb_interface_get_emsmdb_info();
			return exmdb_client_set_instance_property(
					logon_object_get_dir(pctx->pstream->plogon),
					attachment_object_get_instance_id(pctx->pobject),
					ppropval, &b_result);
		case ROOT_ELEMENT_MESSAGELIST:
		case ROOT_ELEMENT_TOPFOLDER:
			return FALSE;
		}
		return FALSE;
	case STARTTOPFLD:
	case STARTSUBFLD:
		return tpropval_array_set_propval(
					pctx->pproplist, ppropval);
	case STARTMESSAGE:
	case STARTFAIMSG:
		return tpropval_array_set_propval(((MARKER_NODE*)
				pnode->pdata)->data.pelement, ppropval);
	case STARTEMBED:
	case NEWATTACH:
		if (ROOT_ELEMENT_ATTACHMENTCONTENT == pctx->root_element ||
			ROOT_ELEMENT_MESSAGECONTENT == pctx->root_element) {
			pinfo = emsmdb_interface_get_emsmdb_info();
			return exmdb_client_set_instance_property(
					logon_object_get_dir(pctx->pstream->plogon),
					((MARKER_NODE*)pnode->pdata)->data.instance_id,
					ppropval, &b_result);
		} else {
			return tpropval_array_set_propval(((MARKER_NODE*)
					pnode->pdata)->data.pelement, ppropval);
		}
	case STARTRECIP:
		if (ROOT_ELEMENT_ATTACHMENTCONTENT == pctx->root_element ||
			ROOT_ELEMENT_MESSAGECONTENT == pctx->root_element) {
			return tpropval_array_set_propval(pctx->pproplist, ppropval);
		} else {
			return tpropval_array_set_propval(((MARKER_NODE*)
					pnode->pdata)->data.pelement, ppropval);
		}
	default:
		return FALSE;
	}
}

BOOL fastupctx_object_write_buffer(FASTUPCTX_OBJECT *pctx,
	const BINARY *ptransfer_data)
{
	/* check if the fast stream is marked as ended */
	if (TRUE == pctx->b_ended) {
		return FALSE;
	}
	if (FALSE == ftstream_parser_write_buffer(
		pctx->pstream, ptransfer_data)) {
		return FALSE;
	}
	return ftstream_parser_process(pctx->pstream,
			(RECORD_MARKER)fastupctx_object_record_marker,
			(RECORD_PROPVAL)fastupctx_object_record_propval, pctx);
}
