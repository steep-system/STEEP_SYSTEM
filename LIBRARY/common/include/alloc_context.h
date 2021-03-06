#ifndef _H_ALLOC_CONTEXT_
#define _H_ALLOC_CONTEXT_
#include "double_list.h"


typedef struct _ALLOC_CONTEXT {
	DOUBLE_LIST list;
	int offset;
	size_t total;
} ALLOC_CONTEXT;

void alloc_context_init(ALLOC_CONTEXT *pcontext);

void* alloc_context_alloc(ALLOC_CONTEXT *pcontext, size_t size);

void alloc_context_free(ALLOC_CONTEXT *pcontext);

size_t alloc_context_get_total(ALLOC_CONTEXT *pcontext);

#endif /* _H_ALLOC_CONTEXT_ */