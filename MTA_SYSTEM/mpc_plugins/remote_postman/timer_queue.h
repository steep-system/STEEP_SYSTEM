#ifndef _H_TIMER_QUEUE_
#define _H_TIMER_QUEUE_
#include "hook_common.h"
#include "bounce_producer.h"
#include <time.h>

enum{
	TIMER_QUEUE_UNTRIED = -1,
	TIMER_QUEUE_FRESH,
	TIMER_QUEUE_RETRYING,
	TIMER_QUEUE_FINAL,
	TIMER_QUEUE_NUM,
	TIMER_QUEUE_SCAN_INTERVAL,
	TIMER_QUEUE_THREADS_MAX,
	TIMER_QUEUE_THREADS_NUM
};

void timer_queue_init(const char *path, int max_thr, int scan_interval,
	int fresh_interval, int retrying_interval, int final_interval);

int timer_queue_run();

int timer_queue_stop();

void timer_queue_free();

BOOL timer_queue_put(MESSAGE_CONTEXT *pcontext, time_t original_time,
	BOOL is_untried);

int timer_queue_get_param(int param);

void timer_queue_set_param(int param, int val);

#endif /* _H_TIMER_QUEUE_ */

