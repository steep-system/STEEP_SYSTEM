#ifndef _H_ROP_EXT_
#define _H_ROP_EXT_
#include "ext_buffer.h"
#include "processor_types.h"


int rop_ext_pull_rop_buffer(EXT_PULL *pext, ROP_BUFFER *r);

int rop_ext_make_rpc_ext(const uint8_t *pbuff_in, uint32_t in_len,
	const ROP_BUFFER *prop_buff, uint8_t *pbuff_out, uint32_t *pout_len);

void rop_ext_set_rhe_flag_last(uint8_t *pdata, uint32_t last_offset);

int rop_ext_push_rop_response(EXT_PUSH *pext,
	uint8_t logon_id, ROP_RESPONSE *r);

int rop_ext_push_buffertoosmall_response(EXT_PUSH *pext,
	const BUFFERTOOSMALL_RESPONSE *r);

int rop_ext_push_notify_response(EXT_PUSH *pext,
	const NOTIFY_RESPONSE *r);

int rop_ext_push_pending_response(EXT_PUSH *pext,
	const PENDING_RESPONSE *r);

#endif /* _H_ROP_EXT_ */