#ifndef _H_CONTEXTS_POOL_
#define _H_CONTEXTS_POOL_
#include "common_types.h"
#include "double_list.h"
#include <sys/time.h>


#define MAX_TURN_COUNTS     0x7FFFFFFF

#define CALCULATE_INTERVAL(a, b) \
    (((a).tv_usec >= (b).tv_usec) ? ((a).tv_sec - (b).tv_sec) : \
    ((a).tv_sec - (b).tv_sec - 1))

enum{
	CONTEXT_BEGIN = 0,
	CONTEXT_FREE = 0,	/* context is free */
	CONTEXT_IDLING,		/* context is waiting on self-defined condition */
	CONTEXT_POLLING,	/* context is waiting on epoll*/
	CONTEXT_SLEEPING,	/* context is need to sleep */
	CONTEXT_TURNING,	/* context is waiting to be served by pool threads */
	CONTEXT_TYPES,
	CONTEXT_CONSTRUCTING,	/* context is got from pool
								and wait to be construct */
	CONTEXT_SWITCHING		/* context is switching between sheduling
								(polling, idling to turning) queues */
};

/* enumeration for distinguishing parameters of contexts pool */
enum{
	MAX_CONTEXTS_NUM,
	CONTEXTS_PER_THR,
	CUR_VALID_CONTEXTS,
	CUR_SLEEPING_CONTEXTS,
	CUR_SCHEDUING_CONTEXTS
};


#define POLLING_READ						0x1
#define POLLING_WRITE						0x2


typedef struct _SCHEDULE_CONTEXT{
	DOUBLE_LIST_NODE node;
	int type;
	BOOL b_waiting;		/* is still in epoll queue */
	int polling_mask;
} SCHEDULE_CONTEXT;

#ifdef __cplusplus
extern "C" {
#endif

void contexts_pool_init(void *pcontexts, int context_num,
	int unit_offset, int (*get_socket)(void*),
	struct timeval (*get_timestamp)(void*),
	int contexts_per_thr, int timeout);

int contexts_pool_run();

int contexts_pool_stop();

void contexts_pool_free();

SCHEDULE_CONTEXT* contexts_pool_get_context(int type);

void contexts_pool_put_context(SCHEDULE_CONTEXT *pcontext, int type);

BOOL contexts_pool_wakeup_context(SCHEDULE_CONTEXT *pcontext, int type);

void contexts_pool_signal(SCHEDULE_CONTEXT *pcontext);

int contexts_pool_get_param(int type);

#ifdef __cplusplus
}
#endif

#endif
