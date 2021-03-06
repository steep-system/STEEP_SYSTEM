#include "common_types.h"
#include "double_list.h"
#include "contexts_pool.h"
#include "lib_buffer.h"
#include "threads_pool.h"
#include "util.h"
#include <stdio.h>
#include <unistd.h>

#define MAX_TIMES_NOT_SERVED			100

#define MAX_NOT_EMPTY_TIMES				10

#define THREAD_STACK_SIZE           	16 * 1024 * 1024

typedef struct _THR_DATA {
	DOUBLE_LIST_NODE node;
	BOOL notify_stop;
	pthread_t id;
} THR_DATA;

static pthread_t g_scan_id;
static BOOL g_notify_stop= TRUE;
static int g_threads_pool_min_num;
static int g_threads_pool_max_num;
static int g_threads_pool_cur_thr_num;
static LIB_BUFFER* g_threads_data_buff;
static DOUBLE_LIST g_threads_data_list;
static THREADS_EVENT_PROC g_threads_event_proc;
static pthread_mutex_t g_threads_pool_data_lock;
static pthread_cond_t g_threads_pool_waken_cond;
static pthread_mutex_t g_threads_pool_cond_mutex;

static void* thread_work_func(void* pparam);

static void* scan_work_func(void *pparam);

static int (*threads_pool_process_func)(SCHEDULE_CONTEXT*);


void threads_pool_init(int init_pool_num,
	int (*process_func)(SCHEDULE_CONTEXT*))
{
	int contexts_max_num;
	int contexts_per_thr;
	
	g_threads_pool_min_num = init_pool_num;
	threads_pool_process_func = process_func;
	/*  CAUTION !!! threads pool should be initialized
		after that contexts pool has been initialized */
	contexts_max_num = contexts_pool_get_param(MAX_CONTEXTS_NUM);
	contexts_per_thr = contexts_pool_get_param(CONTEXTS_PER_THR);
	g_threads_pool_max_num = (contexts_max_num +
		contexts_per_thr - 1)/ contexts_per_thr; 
	if (g_threads_pool_min_num > g_threads_pool_max_num) {
		g_threads_pool_min_num = g_threads_pool_max_num;
	}
	g_threads_pool_cur_thr_num = 0;
	g_threads_data_buff = NULL;
	g_threads_event_proc = NULL;
	pthread_mutex_init(&g_threads_pool_data_lock, NULL);
	pthread_cond_init(&g_threads_pool_waken_cond, NULL);
	pthread_mutex_init(&g_threads_pool_cond_mutex, NULL);
	double_list_init(&g_threads_data_list);
}

int threads_pool_run()
{
	int i;
	THR_DATA *pdata;
	int created_thr_num;
	pthread_attr_t attr;
	
	/* g_threads_data_buff is protected by g_threads_pool_data_lock */
	g_threads_data_buff = lib_buffer_init(sizeof(THR_DATA), 
							g_threads_pool_max_num, FALSE);
	if (NULL == g_threads_data_buff) {
		printf("[threads_pool]: fail to allocate memory for threads pool\n");
		return -1;
	}
	/* list is also protected by g_threads_pool_data_lock */
	g_notify_stop = FALSE;
	if (0 != pthread_create(&g_scan_id, NULL, scan_work_func, NULL)) {
		printf("[threads_pool]: fail to create scan thread\n");
		lib_buffer_free(g_threads_data_buff);
		g_threads_data_buff = NULL;
		return -2;
	}
	pthread_attr_init(&attr);
	created_thr_num = 0;
	for (i=0; i<g_threads_pool_min_num; i++) {
		pdata = (THR_DATA*)lib_buffer_get(g_threads_data_buff);
		pdata->node.pdata = pdata;
		pdata->id = (pthread_t)-1;
		pdata->notify_stop = FALSE;
		pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
		if (0 != pthread_create(&pdata->id, &attr,
			thread_work_func, (void*)pdata)) {
			printf("[threads_pool]: fail to create a pool thread\n");
		} else {
			created_thr_num ++;
			double_list_append_as_tail(&g_threads_data_list, &pdata->node);
		}
	}
	pthread_attr_destroy(&attr);
	g_threads_pool_cur_thr_num = created_thr_num;
	return 0;
}

int threads_pool_stop()
{
	THR_DATA *pthr;
	pthread_t thr_id;
	DOUBLE_LIST_NODE *pnode;
	BOOL b_should_exit = FALSE;
	
	g_notify_stop = TRUE;
	pthread_join(g_scan_id, NULL);
	while (TRUE) {
		/* get a thread from list */
		pthread_mutex_lock(&g_threads_pool_data_lock);
		pnode = double_list_get_head(&g_threads_data_list);
		if (1 == double_list_get_nodes_num(&g_threads_data_list)) {
			b_should_exit = TRUE;
		}
		pthread_mutex_unlock(&g_threads_pool_data_lock);
		pthr = (THR_DATA*)pnode->pdata;
		thr_id = pthr->id;
		/* notify this thread to exit */
		pthr->notify_stop = TRUE;
		/* wake up all thread waiting on the event */
		pthread_cond_broadcast(&g_threads_pool_waken_cond);
		pthread_join(thr_id, NULL);
		if (TRUE == b_should_exit) {
			break;
		}
	}
	lib_buffer_free(g_threads_data_buff);
	return 0;
}

void threads_pool_free()
{
	g_threads_data_buff = NULL;
	g_threads_pool_min_num = 0;
	g_threads_pool_max_num = 0;
	g_threads_pool_cur_thr_num = 0;
	g_threads_event_proc = NULL;
	pthread_mutex_destroy(&g_threads_pool_data_lock);
	pthread_cond_destroy(&g_threads_pool_waken_cond);
	pthread_mutex_destroy(&g_threads_pool_cond_mutex);
}

int threads_pool_get_param(int type)
{
	switch(type) {
	case THREADS_POOL_MIN_NUM:
		return g_threads_pool_min_num;
	case THREADS_POOL_MAX_NUM:
		return g_threads_pool_max_num;
	case THREADS_POOL_CUR_THR_NUM:
		return g_threads_pool_cur_thr_num;
	default:
		return -1;
	}
}

static void* thread_work_func(void* pparam)
{
	THR_DATA *pdata;
	struct timespec ts;
	int cannot_served_times;
	int max_contexts_per_thr;
	int contexts_per_threads;
	SCHEDULE_CONTEXT *pcontext;
	
	
	pdata = (THR_DATA*)pparam;
	max_contexts_per_thr = contexts_pool_get_param(CONTEXTS_PER_THR);
	contexts_per_threads = max_contexts_per_thr / 4;
	if (NULL!= g_threads_event_proc) {
		g_threads_event_proc(THREAD_CREATE);
	}   
	
	cannot_served_times = 0;
	while (TRUE != pdata->notify_stop) {
		pcontext = contexts_pool_get_context(CONTEXT_TURNING);
		if (NULL == pcontext) {
			if (MAX_TIMES_NOT_SERVED == cannot_served_times) {
				pthread_mutex_lock(&g_threads_pool_data_lock);
				if (g_threads_pool_cur_thr_num > g_threads_pool_min_num
					&& g_threads_pool_cur_thr_num * contexts_per_threads
					> contexts_pool_get_param(CUR_VALID_CONTEXTS)) {
					double_list_remove(&g_threads_data_list, &pdata->node);
					lib_buffer_put(g_threads_data_buff, pdata);
					g_threads_pool_cur_thr_num --;
					pthread_mutex_unlock(&g_threads_pool_data_lock);
					if (NULL != g_threads_event_proc) {
						g_threads_event_proc(THREAD_DESTROY);
					}
					pthread_detach(pthread_self());
					pthread_exit(0);
				}
				pthread_mutex_unlock(&g_threads_pool_data_lock);
			} else {
				cannot_served_times ++;
			}
			/* wait context */
			clock_gettime(CLOCK_REALTIME, &ts);
			ts.tv_sec ++;
			pthread_mutex_lock(&g_threads_pool_cond_mutex);
			pthread_cond_timedwait(&g_threads_pool_waken_cond, 
							&g_threads_pool_cond_mutex, &ts);
			pthread_mutex_unlock(&g_threads_pool_cond_mutex);
			continue;
		}
		cannot_served_times = 0;
		switch (threads_pool_process_func(pcontext)) {
		case PROCESS_CONTINUE:
			contexts_pool_put_context(pcontext, CONTEXT_TURNING);
			break;
		case PROCESS_IDLE:
			contexts_pool_put_context(pcontext, CONTEXT_IDLING);
			break;
		case PROCESS_POLLING_NONE:
			pcontext->polling_mask = 0;
			contexts_pool_put_context(pcontext, CONTEXT_POLLING);
			break;
		case PROCESS_POLLING_RDONLY:
			pcontext->polling_mask = POLLING_READ;
			contexts_pool_put_context(pcontext, CONTEXT_POLLING);
			break;
		case PROCESS_POLLING_WRONLY:
			pcontext->polling_mask = POLLING_WRITE;
			contexts_pool_put_context(pcontext, CONTEXT_POLLING);
			break;
		case PROCESS_POLLING_RDWR:
			pcontext->polling_mask = POLLING_READ | POLLING_WRITE;
			contexts_pool_put_context(pcontext, CONTEXT_POLLING);
			break;
		case PROCESS_SLEEPING:
			contexts_pool_put_context(pcontext, CONTEXT_SLEEPING);
			break;
		case PROCESS_CLOSE:
			contexts_pool_put_context(pcontext, CONTEXT_FREE);
			break;
		}
	}
	
	pthread_mutex_lock(&g_threads_pool_data_lock);
	double_list_remove(&g_threads_data_list, &pdata->node);
	lib_buffer_put(g_threads_data_buff, pdata);
	g_threads_pool_cur_thr_num --;
	pthread_mutex_unlock(&g_threads_pool_data_lock);
	if (NULL != g_threads_event_proc) {
		g_threads_event_proc(THREAD_DESTROY);
	}
	pthread_exit(0);
	return NULL;
}

void threads_pool_wakeup_thread()
{
	if (TRUE == g_notify_stop) {
		return;
	}
	pthread_cond_signal(&g_threads_pool_waken_cond);
}

void threads_pool_wakeup_all_threads()
{
	if (TRUE == g_notify_stop) {
		return;
	}
	pthread_cond_broadcast(&g_threads_pool_waken_cond);
}

static void* scan_work_func(void *pparam)
{
	THR_DATA *pdata;
	int not_empty_times;
	pthread_attr_t attr;
	
	not_empty_times = 0;
	while (FALSE == g_notify_stop) {
		sleep(1);
		if (contexts_pool_get_param(CUR_SCHEDUING_CONTEXTS) > 1) {
			not_empty_times ++;
			if (not_empty_times < MAX_NOT_EMPTY_TIMES) {
				continue;
			}
			pthread_mutex_lock(&g_threads_pool_data_lock);
			if (g_threads_pool_cur_thr_num >= g_threads_pool_max_num) {
				pthread_mutex_unlock(&g_threads_pool_data_lock);
				continue;
			}
			pdata = (THR_DATA*)lib_buffer_get(g_threads_data_buff);
			if (NULL != pdata) {
				pdata->node.pdata = pdata;
				pdata->id = (pthread_t)-1;
				pdata->notify_stop = FALSE;
				pthread_attr_init(&attr);
				pthread_attr_setstacksize(&attr, THREAD_STACK_SIZE);
				if (0 != pthread_create(&pdata->id, &attr,
					thread_work_func, (void*)pdata)){
					debug_info("[threads_pool]: fail "
						"to increase a pool thread\n");
					lib_buffer_put(g_threads_data_buff, pdata);
				} else {
					double_list_append_as_tail(
						&g_threads_data_list, &pdata->node);
					g_threads_pool_cur_thr_num ++;
				}
				pthread_attr_destroy(&attr);
			} else {
				debug_info("[threads_pool]: fatal error,"
					" threads pool memory conflicts!\n");
			}
			pthread_mutex_unlock(&g_threads_pool_data_lock);
		}
		not_empty_times = 0;
	}
	pthread_exit(0);
}

THREADS_EVENT_PROC threads_pool_register_event_proc(THREADS_EVENT_PROC proc)
{
	THREADS_EVENT_PROC temp_proc;

	temp_proc = g_threads_event_proc;
	g_threads_event_proc = proc;
	return temp_proc;
}
