#ifndef _H_FIFO_
#define _H_FIFO_
#include "lib_buffer.h"
#include "single_list.h"

#define EXTRA_FIFOITEM_SIZE sizeof(SINGLE_LIST)

typedef struct _FIFO {
    LIB_BUFFER* mbuf_pool;
    SINGLE_LIST mlist;
    size_t      data_size;
    size_t      cur_size;
    size_t      max_size;
} FIFO;


#ifdef __cplusplus
extern "C" {
#endif

LIB_BUFFER* fifo_allocator_init(size_t data_size, size_t max_size, BOOL thread_safe);

void fifo_allocator_free(LIB_BUFFER* pallocator);

void fifo_init(FIFO* pfifo, LIB_BUFFER* pbuf_pool, 
    size_t data_size, size_t max_size);

void fifo_free(FIFO* pfifo);

BOOL fifo_enqueue(FIFO* pfifo, void* pdata);

void* fifo_get_front(FIFO* pfifo);

void* fifo_remove_front(FIFO* pfifo);

void fifo_dequeue(FIFO* pfifo);

BOOL fifo_is_empty(FIFO* pfifo);

#ifdef __cplusplus
}
#endif

#endif /*_H_FIFO_ */

