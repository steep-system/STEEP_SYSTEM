#ifndef _H_TPROPVAL_ARRAY_
#define _H_TPROPVAL_ARRAY_
#include "mapi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

TPROPVAL_ARRAY* tpropval_array_init();

void tpropval_array_free(TPROPVAL_ARRAY *parray);

BOOL tpropval_array_init_internal(TPROPVAL_ARRAY *parray);

void tpropval_array_free_internal(TPROPVAL_ARRAY *parray);

BOOL tpropval_array_set_propval(TPROPVAL_ARRAY *parray,
	const TAGGED_PROPVAL *ppropval);

void tpropval_array_remove_propval(TPROPVAL_ARRAY *parray, uint32_t proptag);

void* tpropval_array_get_propval(TPROPVAL_ARRAY *parray, uint32_t proptag);

void tpropval_array_update(TPROPVAL_ARRAY *parray_dst,
	const TPROPVAL_ARRAY *parray);

TPROPVAL_ARRAY* tpropval_array_dup(TPROPVAL_ARRAY *parray);

#ifdef __cplusplus
}
#endif

#endif /* _H_TPROPVAL_ARRAY_ */
