#include "icsupctx_object.h"
#include "common_util.h"
#include "idset.h"
#include <stdlib.h>

ICSUPCTX_OBJECT* icsupctx_object_create(
	LOGON_OBJECT *plogon, FOLDER_OBJECT *pfolder,
	uint8_t sync_type)
{
	int state_type;
	ICSUPCTX_OBJECT *pctx;
	
	if (SYNC_TYPE_CONTENTS == sync_type) {
		state_type = ICS_STATE_CONTENTS_UP;
	} else {
		state_type = ICS_STATE_HIERARCHY_UP;
	}
	pctx = malloc(sizeof(ICSUPCTX_OBJECT));
	if (NULL == pctx) {
		return NULL;
	}
	pctx->pstate = ics_state_create(plogon, state_type);
	if (NULL == pctx->pstate) {
		free(pctx);
		return NULL;
	}
	pctx->plogon = plogon;
	pctx->pfolder = pfolder;
	pctx->state_property = 0;
	pctx->sync_type = sync_type;
	pctx->b_started = FALSE;
	return pctx;
}

void icsupctx_object_free(ICSUPCTX_OBJECT *pctx)
{
	if (0 != pctx->state_property) {
		mem_file_free(&pctx->f_state_stream);
	}
	free(pctx);
}

uint8_t icsupctx_object_get_sync_type(ICSUPCTX_OBJECT *pctx)
{
	return pctx->sync_type;
}

FOLDER_OBJECT* icsupctx_object_get_parent_object(
	ICSUPCTX_OBJECT *pctx)
{
	return pctx->pfolder;
}

BOOL icsupctx_object_begin_state_stream(ICSUPCTX_OBJECT *pctx,
	uint32_t state_property)
{
	if (TRUE == pctx->b_started) {
		return FALSE;
	}
	if (0 != pctx->state_property) {
		return FALSE;
	}
	switch (state_property) {
	case META_TAG_IDSETGIVEN:
	case META_TAG_IDSETGIVEN1:
	case META_TAG_CNSETSEEN:
		break;
	case META_TAG_CNSETSEENFAI:
	case META_TAG_CNSETREAD:
		if (SYNC_TYPE_CONTENTS != pctx->sync_type) {
			return FALSE;
		}
		break;
	default:
		return FALSE;
	}
	pctx->state_property = state_property;
	mem_file_init(&pctx->f_state_stream, common_util_get_allocator());
	return TRUE;
}

BOOL icsupctx_object_continue_state_stream(ICSUPCTX_OBJECT *pctx,
	const BINARY *pstream_data)
{
	if (TRUE == pctx->b_started) {
		return FALSE;
	}
	if (0 == pctx->state_property) {
		return FALSE;
	}
	if (META_TAG_IDSETGIVEN == pctx->state_property ||
		META_TAG_IDSETGIVEN1 == pctx->state_property) {
		return TRUE;
	}
	if (pstream_data->cb != mem_file_write(&pctx->f_state_stream,
		pstream_data->pb, pstream_data->cb)) {
		return FALSE;	
	}
	return TRUE;
}

BOOL icsupctx_object_end_state_stream(ICSUPCTX_OBJECT *pctx)
{
	IDSET *pset;
	int repl_type;
	BINARY tmp_bin;
	uint32_t state_property;
	
	if (TRUE == pctx->b_started) {
		return FALSE;
	}
	if (0 == pctx->state_property) {
		return FALSE;
	}
	if (META_TAG_IDSETGIVEN == pctx->state_property ||
		META_TAG_IDSETGIVEN1 == pctx->state_property) {
		pctx->state_property = 0;
		mem_file_free(&pctx->f_state_stream);
		return TRUE;
	}
	pset = idset_init(FALSE, REPL_TYPE_GUID);
	if (NULL == pset) {
		return FALSE;
	}
	tmp_bin.cb = mem_file_get_total_length(&pctx->f_state_stream);
	tmp_bin.pb = common_util_alloc(tmp_bin.cb);
	if (NULL == tmp_bin.pb) {
		idset_free(pset);
		return FALSE;
	}
	mem_file_read(&pctx->f_state_stream, tmp_bin.pb, tmp_bin.cb);
	mem_file_free(&pctx->f_state_stream);
	state_property = pctx->state_property;
	pctx->state_property = 0;
	if (FALSE == idset_deserialize(pset, &tmp_bin)) {
		idset_free(pset);
		return FALSE;
	}
	tmp_bin.cb = sizeof(void*);
	tmp_bin.pb = (void*)&pctx->plogon;
	if (FALSE == idset_register_mapping(pset,
		&tmp_bin, common_util_mapping_replica)) {
		idset_free(pset);
		return FALSE;
	}
	if (FALSE == idset_convert(pset)) {
		idset_free(pset);
		return FALSE;
	}
	if (FALSE == ics_state_append_idset(
		pctx->pstate, state_property, pset)) {
		idset_free(pset);
		return FALSE;
	}
	return TRUE;
}

ICS_STATE* icsupctx_object_get_state(ICSUPCTX_OBJECT *pctx)
{
	return pctx->pstate;
}

BOOL icsupctx_object_check_started(ICSUPCTX_OBJECT *pctx)
{
	return pctx->b_started;
}

void icsupctx_object_mark_started(ICSUPCTX_OBJECT *pctx)
{
	pctx->b_started = TRUE;
}
