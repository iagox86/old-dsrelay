#ifndef __DSR_WINSOCK_H__
#define __DSR_WINSOCK_H__

#include "types.h"

extern BOOL dsr_has_error;
extern BOOL dsr_has_lost_connection;

void dsr_initialize();

SOCKET dsr_listen(u_short port);
SOCKET dsr_connect(char *host, u_short port);
void dsr_close(SOCKET s);
int dsr_send(SOCKET s, char *data, int length);
int dsr_recv(SOCKET s, char *buffer, int max_length);
BOOL dsr_select(SOCKET listener, socket_list *list, u_int *count);

void dsr_winsock_fatal();
void dsr_winsock_error();

#endif