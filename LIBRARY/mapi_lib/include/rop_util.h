#ifndef _H_ROP_UTIL_
#define _H_ROP_UTIL_
#include "mapi_types.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

uint16_t rop_util_get_replid(uint64_t eid);

uint64_t rop_util_get_gc_value(uint64_t eid);

void rop_util_get_gc_array(uint64_t eid, uint8_t gc[6]);

void rop_util_value_to_gc(uint64_t value, uint8_t gc[6]);

uint64_t rop_util_gc_to_value(uint8_t gc[6]);

uint64_t rop_util_make_eid(uint16_t replid, const uint8_t gc[6]);

uint64_t rop_util_make_eid_ex(uint16_t replid, uint64_t value);

GUID rop_util_make_user_guid(int user_id);

GUID rop_util_make_domain_guid(int domain_id);

int rop_util_make_user_id(GUID guid);

int rop_util_make_domain_id(GUID guid);

uint64_t rop_util_unix_to_nttime(time_t unix_time);

time_t rop_util_nttime_to_unix(uint64_t nt_time);

uint64_t rop_util_current_nttime();

GUID rop_util_binary_to_guid(const BINARY *pbin);

void rop_util_guid_to_binary(GUID guid, BINARY *pbin);

BOOL rop_util_get_common_pset(int pset_type, GUID *pguid);

BOOL rop_util_get_provider_uid(int provider_type, uint8_t *pflat_guid);

void rop_util_free_binary(BINARY *pbin);

#ifdef __cplusplus
}
#endif

#endif /* _H_ROP_UTIL_ */
