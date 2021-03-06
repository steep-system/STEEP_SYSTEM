#ifndef _H_NET_FAILURE_
#define _H_NET_FAILURE_

enum{
	NET_FAILURE_OK,
	NET_FAILURE_TEMP,
	NET_FAILURE_PERMANENT,
	NET_FAILURE_NOUSER,
	NET_FAILURE_TURN_ALARM,
	NET_FAILURE_STATISTIC_TIMES,
	NET_FAILURE_STATISTIC_INTERVAL,
	NET_FAILURE_ALARM_INTERVAL
};

void net_failure_init(int times, int interval, int alarm_interval);

int net_failure_run();

int net_failure_stop();

void net_failure_free();

void net_failure_statistic(int OK_num, int temp_fail, int permanent_fail,
	int nouser_num);

int net_failure_get_param(int param);

void net_failure_set_param(int param, int val);

#endif /* _H_NET_FAILURE_ */

