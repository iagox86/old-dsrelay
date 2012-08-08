#include "options.h"

u_short option_listen_port        = 0;
u_int   option_wait               = 0;
int     option_terminate_at       = 0;

int     option_verbose            = FALSE;
u_int   option_data               = 0;
BOOL    option_sanitize_data      = FALSE;
BOOL    option_terminate_on_close = FALSE;
BOOL    option_terminate_on_error = FALSE;
BOOL    option_reconnect_on_close = FALSE;