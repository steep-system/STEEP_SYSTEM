#ifndef _H_RELOAD_CONTROL_
#define _H_RELOAD_CONTROL_

void reload_control_init(const char *path);

int reload_control_run();

void reload_control_notify();

int reload_control_stop();

void reload_control_free();

#endif
