#include "common_types.h"
#include "endian_macro.h"
#include "ndr.h"
#include <stdlib.h>
#include <string.h>

#define NDR_BE(pndr) ((pndr->flags & NDR_FLAG_BIGENDIAN) != 0)

#define NDR_SVAL(pndr, ofs) (NDR_BE(pndr)?RSVAL(pndr->data,ofs):SVAL(pndr->data,ofs))
#define NDR_IVAL(pndr, ofs) (NDR_BE(pndr)?RIVAL(pndr->data,ofs):IVAL(pndr->data,ofs))
#define NDR_IVALS(pndr, ofs) (NDR_BE(pndr)?RIVALS(pndr->data,ofs):IVALS(pndr->data,ofs))
#define NDR_SSVAL(pndr, ofs, v) NDR_BE(pndr)?RSSVAL(pndr->data,ofs,v):SSVAL(pndr->data,ofs,v)
#define NDR_SIVAL(pndr, ofs, v) NDR_BE(pndr)?RSIVAL(pndr->data,ofs,v):SIVAL(pndr->data,ofs,v)
#define NDR_SIVALS(pndr, ofs, v) NDR_BE(pndr)?RSIVALS(pndr->data,ofs,v):SIVALS(pndr->data,ofs,v)


typedef struct _PTR_NODE {
	DOUBLE_LIST_NODE node;
	const void *pointer;
	uint32_t ptr_count;
} PTR_NODE;


int ndr_pull_advance(NDR_PULL *pndr, uint32_t size)
{
	pndr->offset += size;
	if (pndr->offset > pndr->data_size) {
		return NDR_ERR_BUFSIZE;
	}
	return NDR_ERR_SUCCESS;
}

void ndr_set_flags(uint32_t *pflags, uint32_t new_flags)
{
	if (new_flags & NDR_ALIGN_FLAGS) {
		/* Ensure we only have the passed-in
		   align flag set in the new_flags,
		   remove any old align flag. */
		(*pflags) &= ~NDR_ALIGN_FLAGS;
	}
	if (new_flags & NDR_FLAG_NO_RELATIVE_REVERSE) {
		(*pflags) &= ~NDR_FLAG_RELATIVE_REVERSE;
	}
	(*pflags) |= new_flags;
}

uint32_t ndr_pull_get_ptrcnt(NDR_PULL *pndr)
{
	return pndr->ptr_count;
}

static size_t ndr_align_size(uint32_t offset, size_t n)
{
	if (0 == (offset & (n - 1))) {
		return 0;
	}
	return n - (offset & (n - 1));
}


void ndr_pull_init(NDR_PULL *pndr, uint8_t *pdata,
	uint32_t data_size, uint32_t flags)
{
	pndr->data = pdata;
	pndr->data_size = data_size;
	pndr->offset = 0;
	pndr->flags = flags;
	pndr->ptr_count = 0;
}

void ndr_pull_destroy(NDR_PULL *pndr)
{
	pndr->data = NULL;
	pndr->data_size = 0;
	pndr->offset = 0;
	pndr->flags = 0;
}

static BOOL ndr_pull_check_padding(NDR_PULL *pndr, size_t n)
{
	int i;
	size_t ofs2;

	ofs2 = (pndr->offset + (n - 1)) & ~(n - 1);
	
	for (i=pndr->offset; i<ofs2; i++) {
		if (pndr->data[i] != 0) {
			return FALSE;
		}
	}
	return TRUE;
}

int ndr_pull_align(NDR_PULL *pndr, size_t size)
{
	if (5 == size) {
		if (pndr->flags & NDR_FLAG_NDR64) {
			size = 8;
		} else {
			size = 4;
		}
	} else if (3 == size) {
		if (pndr->flags & NDR_FLAG_NDR64) {
			size = 4;
		} else {
			size = 2;
		}
	}
	
	if (0 == (pndr->flags & NDR_FLAG_NOALIGN)) {
		if (pndr->flags & NDR_FLAG_PAD_CHECK) {
			if (FALSE == ndr_pull_check_padding(pndr, size)) {
				return NDR_ERR_PADDING;
			}
		}
		pndr->offset = (pndr->offset + (size - 1)) & ~(size - 1);
	}
	if (pndr->offset > pndr->data_size) {
		return NDR_ERR_BUFSIZE;
	}
	return NDR_ERR_SUCCESS;
}

int ndr_pull_union_align(NDR_PULL *pndr, size_t size)
{
	/* MS-RPCE section 2.2.5.3.4.4 */
	if (pndr->flags & NDR_FLAG_NDR64) {
		return ndr_pull_align(pndr, size);
	}
	return NDR_ERR_SUCCESS;
}

int ndr_pull_trailer_align(NDR_PULL *pndr, size_t size)
{
	/* MS-RPCE section 2.2.5.3.4.1 */
	if (pndr->flags & NDR_FLAG_NDR64) {
		return ndr_pull_align(pndr, size);
	}
	return NDR_ERR_SUCCESS;
}


int ndr_pull_string(NDR_PULL *pndr, char *buff, uint32_t inbytes)
{
	if (0 == inbytes) {
		buff[0] = '\0';
		return NDR_ERR_SUCCESS;
	}

	if (pndr->data_size < inbytes ||
		pndr->offset + inbytes > pndr->data_size) {
		return NDR_ERR_BUFSIZE;
	}
	
	memcpy(buff, pndr->data + pndr->offset, inbytes);
	buff[inbytes] = '\0';
	
	return ndr_pull_advance(pndr, inbytes);
}

int ndr_pull_int8(NDR_PULL *pndr, int8_t *v)
{
	if (pndr->data_size < 1 || pndr->offset + 1 > pndr->data_size) {
		return NDR_ERR_BUFSIZE;
	}
	*v = (int8_t)CVAL(pndr->data, pndr->offset);
	pndr->offset += 1;
	return NDR_ERR_SUCCESS;
}

int ndr_pull_uint8(NDR_PULL *pndr, uint8_t *v)
{
	if (pndr->data_size < 1 || pndr->offset + 1 > pndr->data_size) {
		return NDR_ERR_BUFSIZE;
	}
	*v = CVAL(pndr->data, pndr->offset);
	pndr->offset += 1;
	return NDR_ERR_SUCCESS;
}

int ndr_pull_int16(NDR_PULL *pndr, int16_t *v)
{
	int status;
	
	status = ndr_pull_align(pndr, 2);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	if (pndr->data_size < 2 || pndr->offset + 2 > pndr->data_size) {
		return NDR_ERR_BUFSIZE;
	}
	*v = (int16_t)NDR_SVAL(pndr, pndr->offset);
	pndr->offset += 2;
	return NDR_ERR_SUCCESS;
}

int ndr_pull_uint16(NDR_PULL *pndr, uint16_t *v)
{
	int status;
	
	status = ndr_pull_align(pndr, 2);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	if (pndr->data_size < 2 || pndr->offset + 2 > pndr->data_size) {
		return NDR_ERR_BUFSIZE;
	}
	*v = NDR_SVAL(pndr, pndr->offset);
	pndr->offset += 2;
	return NDR_ERR_SUCCESS;
}

int ndr_pull_int32(NDR_PULL *pndr, int32_t *v)
{
	int status;
	
	status = ndr_pull_align(pndr, 4);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	
	if (pndr->data_size < 4 || pndr->offset + 4 > pndr->data_size) {
		return NDR_ERR_BUFSIZE;
	}
	*v = NDR_IVALS(pndr, pndr->offset);
	pndr->offset += 4;
	return NDR_ERR_SUCCESS;
}

int ndr_pull_uint32(NDR_PULL *pndr, uint32_t *v)
{
	int status;
	
	status = ndr_pull_align(pndr, 4);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	if (pndr->data_size < 4 || pndr->offset + 4 > pndr->data_size) {
		return NDR_ERR_BUFSIZE;
	}
	*v = NDR_IVAL(pndr, pndr->offset);
	pndr->offset += 4;
	return NDR_ERR_SUCCESS;
}

int ndr_pull_int64(NDR_PULL *pndr, int64_t *v)
{
	int status;
	
	status = ndr_pull_align(pndr, 8);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	if (pndr->data_size < 8 || pndr->offset + 8 > pndr->data_size) {
		return NDR_ERR_BUFSIZE;
	}
	if (NDR_BE(pndr)) {
		*v = ((int64_t)NDR_IVAL(pndr, pndr->offset)) << 32;
		*v |= NDR_IVAL(pndr, pndr->offset + 4);
	} else {
		*v = NDR_IVAL(pndr, pndr->offset);
		*v |= (int64_t)(NDR_IVAL(pndr, pndr->offset + 4)) << 32;
	}
	pndr->offset += 8;
	return NDR_ERR_SUCCESS;
}

int ndr_pull_uint64(NDR_PULL *pndr, uint64_t *v)
{
	int status;
	
	status = ndr_pull_align(pndr, 8);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	if (pndr->data_size < 8 || pndr->offset + 8 > pndr->data_size) {
		return NDR_ERR_BUFSIZE;
	}
	if (NDR_BE(pndr)) {
		*v = ((uint64_t)NDR_IVAL(pndr, pndr->offset)) << 32;
		*v |= NDR_IVAL(pndr, pndr->offset+4);
	} else {
		*v = NDR_IVAL(pndr, pndr->offset);
		*v |= (uint64_t)(NDR_IVAL(pndr, pndr->offset+4)) << 32;
	}
	pndr->offset += 8;
	return NDR_ERR_SUCCESS;
}

int ndr_pull_long(NDR_PULL *pndr, int32_t *v)
{
	int status;
	int64_t v64;
	
	if (pndr->flags & NDR_FLAG_NDR64) {
		status = ndr_pull_int64(pndr, &v64);
		if (NDR_ERR_SUCCESS != status) {
			return status;
		}
		*v = (int32_t)v64;
		if (v64 != *v) {
			return NDR_ERR_NDR64;
		}
		return NDR_ERR_SUCCESS;
	}
	return ndr_pull_int32(pndr, v);
}

int ndr_pull_ulong(NDR_PULL *pndr, uint32_t *v)
{
	int status;
	uint64_t v64;
	
	if (pndr->flags & NDR_FLAG_NDR64) {
		status = ndr_pull_uint64(pndr, &v64);
		if (NDR_ERR_SUCCESS != status) {
			return status;
		}
		*v = (uint32_t)v64;
		if (v64 != *v) {
			return NDR_ERR_NDR64;
		}
		return NDR_ERR_SUCCESS;
	}
	return ndr_pull_uint32(pndr, v);
}

static int ndr_pull_bytes(NDR_PULL *pndr, uint8_t *data, uint32_t n)
{
	
	if (pndr->data_size < n || pndr->offset + n > pndr->data_size) {
		return NDR_ERR_BUFSIZE;
	}
	
	memcpy(data, pndr->data + pndr->offset, n);
	pndr->offset += n;
	return NDR_ERR_SUCCESS;
}

int ndr_pull_array_uint8(NDR_PULL *pndr, uint8_t *data, uint32_t n)
{
	return ndr_pull_bytes(pndr, data, n);
}

int ndr_pull_guid(NDR_PULL *pndr, GUID *r)
{
	int status;
	
	status = ndr_pull_align(pndr, 4);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_uint32(pndr, &r->time_low);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_uint16(pndr, &r->time_mid);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_uint16(pndr, &r->time_hi_and_version);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_array_uint8(pndr, r->clock_seq, 2);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_array_uint8(pndr, r->node, 6);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_trailer_align(pndr, 4);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	return NDR_ERR_SUCCESS;
}


int ndr_pull_syntax_id(NDR_PULL *pndr, SYNTAX_ID *r)
{
	int status;
	
	status = ndr_pull_align(pndr, 4);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_guid(pndr, &r->uuid);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_uint32(pndr, &r->version);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_trailer_align(pndr, 4);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	return NDR_ERR_SUCCESS;
}

int ndr_pull_data_blob(NDR_PULL *pndr, DATA_BLOB *pblob)
{
	int status;
	uint32_t length;

	length = 0;
	if (pndr->flags & NDR_FLAG_REMAINING) {
		length = pndr->data_size - pndr->offset;
	} else if (pndr->flags & (NDR_ALIGN_FLAGS & ~NDR_FLAG_NOALIGN)) {
		if (pndr->flags & NDR_FLAG_ALIGN2) {
			length = ndr_align_size(pndr->offset, 2);
		} else if (pndr->flags & NDR_FLAG_ALIGN4) {
			length = ndr_align_size(pndr->offset, 4);
		} else if (pndr->flags & NDR_FLAG_ALIGN8) {
			length = ndr_align_size(pndr->offset, 8);
		}
		if (pndr->data_size - pndr->offset < length) {
			length = pndr->data_size - pndr->offset;
		}
	} else {
		status = ndr_pull_uint32(pndr, &length);
		if (NDR_ERR_SUCCESS != status) {
			return status;
		}
	}
	if (pndr->data_size < length ||
		pndr->offset + length > pndr->data_size) {
		return NDR_ERR_BUFSIZE;
	}
	pblob->data = malloc(length);
	if (NULL == pblob->data) {
		return NDR_ERR_ALLOC;
	}
	memcpy(pblob->data, pndr->data + pndr->offset, length);
	pblob->length = length;
	pndr->offset += length;
	return NDR_ERR_SUCCESS;
}

/* free memory internal of blob except of blob itself */
void ndr_free_data_blob(DATA_BLOB *pblob)
{
	if (NULL != pblob->data) {
		free(pblob->data);
		pblob->data = NULL;
	}
	pblob->length = 0;
}

int ndr_pull_check_string(NDR_PULL *pndr,
	uint32_t count, uint32_t element_size)
{
	int status;
	uint32_t i;
	uint32_t saved_offset;

	saved_offset = pndr->offset;
	status = ndr_pull_advance(pndr, (count - 1) * element_size);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	if (pndr->data_size < element_size ||
		pndr->offset + element_size > pndr->data_size) {
		return NDR_ERR_BUFSIZE;
	}
	for (i=0; i<element_size; i++) {
		if (0 != pndr->data[pndr->offset + i]) {
			pndr->offset = saved_offset;
			return NDR_ERR_ARRAY_SIZE;
		}
	}
	
	pndr->offset = saved_offset;
	return NDR_ERR_SUCCESS;
}

int ndr_pull_generic_ptr(NDR_PULL *pndr, uint32_t *v)
{
	int status;
	
	status = ndr_pull_ulong(pndr, v);
	if (*v != 0) {
		pndr->ptr_count ++;
	}
	return NDR_ERR_SUCCESS;
}

int ndr_pull_context_handle(NDR_PULL *pndr, CONTEXT_HANDLE *r)
{
	int status;
	
	status = ndr_pull_align(pndr, 4);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_uint32(pndr, &r->handle_type);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_guid(pndr, &r->guid);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_pull_trailer_align(pndr, 4);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	return NDR_ERR_SUCCESS;
}

/********************************** NDR PHSH *********************************/

void ndr_push_set_ptrcnt(NDR_PUSH *pndr, uint32_t ptr_count)
{
	pndr->ptr_count = ptr_count;
}

void ndr_push_init(NDR_PUSH *pndr, uint8_t *pdata,
	uint32_t alloc_size, uint32_t flags)
{
	pndr->data = pdata;
	pndr->alloc_size = alloc_size;
	pndr->flags = flags;
	pndr->offset = 0;
	pndr->ptr_count = 0;
	double_list_init(&pndr->full_ptr_list);
}

void ndr_push_destroy(NDR_PUSH *pndr)
{
	DOUBLE_LIST_NODE *pnode;
	
	while (pnode=double_list_get_from_head(&pndr->full_ptr_list)) {
		free(pnode->pdata);
	}
	double_list_free(&pndr->full_ptr_list);
	pndr->data = NULL;
	pndr->alloc_size = 0;
	pndr->flags = 0;
	pndr->offset = 0;
}

static BOOL ndr_push_check_overflow(NDR_PUSH *pndr, uint32_t extra_size)
{
	uint32_t size;
	
	size = extra_size + pndr->offset;
	if (size > pndr->alloc_size) {
		/* overflow */
		return FALSE;
	}
	/* not overflow */
	return TRUE;
}

static int ndr_push_bytes(NDR_PUSH *pndr, const uint8_t *pdata, uint32_t n)
{
	if (FALSE == ndr_push_check_overflow(pndr, n)) {
		return NDR_ERR_BUFSIZE;
	}
	memcpy(pndr->data + pndr->offset, pdata, n);
	pndr->offset += n;
	return NDR_ERR_SUCCESS;
}

int ndr_push_int8(NDR_PUSH *pndr, int8_t v)
{
	int status;
	
	if (FALSE == ndr_push_check_overflow(pndr, 1)) {
		return NDR_ERR_BUFSIZE;
	}
	SCVAL(pndr->data, pndr->offset, (uint8_t)v);
	pndr->offset += 1;
	return NDR_ERR_SUCCESS;
}

int ndr_push_uint8(NDR_PUSH *pndr, uint8_t v)
{
	int status;
	
	if (FALSE == ndr_push_check_overflow(pndr, 1)) {
		return NDR_ERR_BUFSIZE;
	}
	SCVAL(pndr->data, pndr->offset, v);
	pndr->offset += 1;
	return NDR_ERR_SUCCESS;
}

int ndr_push_align(NDR_PUSH *pndr, size_t size)
{
	int status;
	uint32_t pad;
	
	if (size == 5) {
		if (pndr->flags & NDR_FLAG_NDR64) {
			size = 8;
		} else {
			size = 4;
		}
	} else if (size == 3) {
		if (pndr->flags & NDR_FLAG_NDR64) {
			size = 4;
		} else {
			size = 2;
		}
	}
	if (0 == (pndr->flags & NDR_FLAG_NOALIGN)) {
		pad = ((pndr->offset + (size - 1)) & ~(size - 1)) - pndr->offset;
		while (pad--) {
			status = ndr_push_uint8(pndr, 0);
			if (NDR_ERR_SUCCESS != status) {
				return status;
			}
		}
	}
	return NDR_ERR_SUCCESS;
}

int ndr_push_union_align(NDR_PUSH *pndr, size_t size)
{
	if (pndr->flags & NDR_FLAG_NDR64) {
		return ndr_push_align(pndr, size);
	}
	return NDR_ERR_SUCCESS;
}

int ndr_push_trailer_align(NDR_PUSH *pndr, size_t size)
{
	if (pndr->flags & NDR_FLAG_NDR64) {
		return ndr_push_align(pndr, size);
	}
	return NDR_ERR_SUCCESS;
}

int ndr_push_int16(NDR_PUSH *pndr, int16_t v)
{
	int status;
	
	status = ndr_push_align(pndr, 2);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	if (FALSE == ndr_push_check_overflow(pndr, 2)) {
		return NDR_ERR_BUFSIZE;
	}
	NDR_SSVAL(pndr, pndr->offset, (uint16_t)v);
	pndr->offset += 2;
	return NDR_ERR_SUCCESS;
}

int ndr_push_uint16(NDR_PUSH *pndr, uint16_t v)
{
	int status;
	
	status = ndr_push_align(pndr, 2);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	if (FALSE == ndr_push_check_overflow(pndr, 2)) {
		return NDR_ERR_BUFSIZE;
	}
	NDR_SSVAL(pndr, pndr->offset, v);
	pndr->offset += 2;
	return NDR_ERR_SUCCESS;
}

int ndr_push_int32(NDR_PUSH *pndr, int32_t v)
{
	int status;
	
	status = ndr_push_align(pndr, 4);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	if (FALSE == ndr_push_check_overflow(pndr, 4)) {
		return NDR_ERR_BUFSIZE;
	}
	NDR_SIVALS(pndr, pndr->offset, v);
	pndr->offset += 4;
	return NDR_ERR_SUCCESS;
}

int ndr_push_uint32(NDR_PUSH *pndr, uint32_t v)
{
	int status;
	
	status = ndr_push_align(pndr, 4);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	if (FALSE == ndr_push_check_overflow(pndr, 4)) {
		return NDR_ERR_BUFSIZE;
	}
	NDR_SIVAL(pndr, pndr->offset, v);
	pndr->offset += 4;
	return NDR_ERR_SUCCESS;
}

int ndr_push_int64(NDR_PUSH *pndr, int64_t v)
{
	int status;
	
	status = ndr_push_align(pndr, 8);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	if (FALSE == ndr_push_check_overflow(pndr, 8)) {
		return NDR_ERR_BUFSIZE;
	}
	if (NDR_BE(pndr)) {
		NDR_SIVAL(pndr, pndr->offset, (v>>32));
		NDR_SIVAL(pndr, pndr->offset+4, (v & 0xFFFFFFFF));
	} else {
		NDR_SIVAL(pndr, pndr->offset, (v & 0xFFFFFFFF));
		NDR_SIVAL(pndr, pndr->offset+4, (v>>32));
	}
	pndr->offset += 8;
	return NDR_ERR_SUCCESS;
}

int ndr_push_uint64(NDR_PUSH *pndr, uint64_t v)
{
	int status;
	
	status = ndr_push_align(pndr, 8);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	if (FALSE == ndr_push_check_overflow(pndr, 8)) {
		return NDR_ERR_BUFSIZE;
	}
	if (NDR_BE(pndr)) {
		NDR_SIVAL(pndr, pndr->offset, (v>>32));
		NDR_SIVAL(pndr, pndr->offset+4, (v & 0xFFFFFFFF));
	} else {
		NDR_SIVAL(pndr, pndr->offset, (v & 0xFFFFFFFF));
		NDR_SIVAL(pndr, pndr->offset+4, (v>>32));
	}
	pndr->offset += 8;
	return NDR_ERR_SUCCESS;
}

int ndr_push_long(NDR_PUSH *pndr, int32_t v)
{
	if (pndr->flags & NDR_FLAG_NDR64) {
		return ndr_push_int64(pndr, v);
	} else {
		return ndr_push_int32(pndr, v);
	}
}

int ndr_push_ulong(NDR_PUSH *pndr, uint32_t v)
{
	if (pndr->flags & NDR_FLAG_NDR64) {
		return ndr_push_uint64(pndr, v);
	} else {
		return ndr_push_uint32(pndr, v);
	}
}

int ndr_push_array_uint8(NDR_PUSH *pndr, const uint8_t *data, uint32_t n)
{
	return ndr_push_bytes(pndr, data, n);
}

/*
 * Push a DATA_BLOB onto the wire.
 * 1) When called with NDR_FLAG_ALIGN* alignment flags set, push padding
 *    bytes _only_. The length is determined by the alignment required and the
 *    current ndr offset.
 * 2) When called with the NDR_FLAG_REMAINING flag, push the byte array to
 *    the ndr buffer.
 * 3) Otherwise, push a uint32 length _and_ a corresponding byte array to the
 *    ndr buffer.
 */
int ndr_push_data_blob(NDR_PUSH *pndr, DATA_BLOB blob)
{
	int status;
	int length;
	char buff[8];
	
	if (pndr->flags & NDR_FLAG_REMAINING) {
		/* nothing to do */
	} else if (pndr->flags & (NDR_ALIGN_FLAGS & ~NDR_FLAG_NOALIGN)) {
		if (pndr->flags & NDR_FLAG_ALIGN2) {
			length = ndr_align_size(pndr->offset, 2);
		} else if (pndr->flags & NDR_FLAG_ALIGN4) {
			length = ndr_align_size(pndr->offset, 4);
		} else if (pndr->flags & NDR_FLAG_ALIGN8) {
			length = ndr_align_size(pndr->offset, 8);
		}
		memset(buff, 0, length);
		status = ndr_push_bytes(pndr, buff, length);
		return status;
	} else {
		status = ndr_push_uint32(pndr, blob.length);
		if (NDR_ERR_SUCCESS != status) {
			return status;
		}
	}
	status = ndr_push_bytes(pndr, blob.data, blob.length);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	return NDR_ERR_SUCCESS;
}

int ndr_push_string(NDR_PUSH *pndr, const char *var, uint32_t required)
{	
	if (FALSE == ndr_push_check_overflow(pndr, required)) {
		return NDR_ERR_BUFSIZE;
	}
	
	memcpy(pndr->data + pndr->offset, var, required);
	pndr->offset += required;
	return NDR_ERR_SUCCESS;
}

int ndr_push_guid(NDR_PUSH *pndr, const GUID *r)
{
	int status;
	
	status = ndr_push_align(pndr, 4);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_uint32(pndr, r->time_low);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_uint16(pndr, r->time_mid);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_uint16(pndr, r->time_hi_and_version);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_array_uint8(pndr, r->clock_seq, 2);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_array_uint8(pndr, r->node, 6);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	return ndr_push_trailer_align(pndr, 4);
}

int ndr_push_syntax_id(NDR_PUSH *pndr, const SYNTAX_ID *r)
{
	int status;
	
	status = ndr_push_align(pndr, 4);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_guid(pndr, &r->uuid);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_uint32(pndr, r->version);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_trailer_align(pndr, 4);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	return NDR_ERR_SUCCESS;
}

int ndr_push_zero(NDR_PUSH *pndr, uint32_t n)
{
	if (FALSE == ndr_push_check_overflow(pndr, n)) {
		return NDR_ERR_BUFSIZE;
	}
	memset(pndr->data + pndr->offset, 0, n);
	pndr->offset += n;
	return NDR_ERR_SUCCESS;
}

int ndr_push_unique_ptr(NDR_PUSH *pndr, const void *p)
{
	uint32_t ptr;
	
	ptr = 0;
	if (NULL != p) {
		ptr = pndr->ptr_count * 4;
		ptr |= 0x00020000;
		pndr->ptr_count++;
	}
	return ndr_push_ulong(pndr, ptr);
}

static uint32_t ndr_push_check_pointer(NDR_PUSH *pndr, const void *p)
{
	PTR_NODE *ptr_node;
	DOUBLE_LIST_NODE *pnode;
	
	for (pnode=double_list_get_head(&pndr->full_ptr_list); NULL!=pnode;
		pnode=double_list_get_after(&pndr->full_ptr_list, pnode)) {
		ptr_node = (PTR_NODE*)pnode->pdata;
		if (p == ptr_node->pointer) {
			return ptr_node->ptr_count;
		}
	}
	return 0;
}

int ndr_push_full_ptr(NDR_PUSH *pndr, const void *p)
{
	uint32_t ptr_count;
	PTR_NODE *ptr_node;
	
	ptr_count = 0;
	if (NULL != p) {
		/* Check if the pointer already exists and has an id */
		ptr_count = ndr_push_check_pointer(pndr, p);
		if (0 == ptr_count) {
			pndr->ptr_count++;
			ptr_count = pndr->ptr_count;
			ptr_node = malloc(sizeof(PTR_NODE));
			if (NULL == ptr_node) {
				return NDR_ERR_ALLOC;
			}
			ptr_node->node.pdata = ptr_node;
			ptr_node->ptr_count = ptr_count;
			ptr_node->pointer = p;
			double_list_append_as_tail(&pndr->full_ptr_list, &ptr_node->node);
		}
	}
	return ndr_push_ulong(pndr, ptr_count);
}

int ndr_push_context_handle(NDR_PUSH *pndr, const CONTEXT_HANDLE *r)
{
	int status;
	
	status = ndr_push_align(pndr, 4);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_uint32(pndr, r->handle_type);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_guid(pndr, &r->guid);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	status = ndr_push_trailer_align(pndr, 4);
	if (NDR_ERR_SUCCESS != status) {
		return status;
	}
	return NDR_ERR_SUCCESS;
}
