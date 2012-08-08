#ifndef __OPTIONS_H__
#define __OPTIONS_H__

#include "types.h"

extern u_short option_listen_port;
extern u_int   option_wait;
extern int     option_terminate_at;
extern BOOL    option_terminate_on_close;
extern BOOL    option_terminate_on_error;

extern int     option_verbose;
extern u_int   option_data;
extern BOOL    option_sanitize_data;
extern BOOL    option_reconnect_on_close;

#endif