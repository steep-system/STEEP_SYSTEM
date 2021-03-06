#ifndef _H_PROPTAG_ARRAY_
#define _H_PROPTAG_ARRAY_
#include "mapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

BOOL proptag_array_init_internal(PROPTAG_ARRAY *pproptags);

PROPTAG_ARRAY* proptag_array_init();

void proptag_array_free_internal(PROPTAG_ARRAY *pproptags);

void proptag_array_free(PROPTAG_ARRAY *pproptags);

void proptag_array_clear(PROPTAG_ARRAY *pproptags);

BOOL proptag_array_append(PROPTAG_ARRAY *pproptags, uint32_t proptag);

void proptag_array_remove(PROPTAG_ARRAY *pproptags, uint32_t proptag);

BOOL proptag_array_check(const PROPTAG_ARRAY *pproptags, uint32_t proptag);

PROPTAG_ARRAY* proptag_array_dup(const PROPTAG_ARRAY *pproptags);

BOOL proptag_array_dup_internal(const PROPTAG_ARRAY *pproptags,
	PROPTAG_ARRAY *pproptags_dst);

#ifdef __cplusplus
}
#endif

#endif /* _H_PROPTAG_ARRAY_ */
