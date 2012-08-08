#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>

#include "dsr_winsock.h"
#include "options.h"
#include "output.h"

#define BUFFER_LENGTH 1024

BOOL dsr_has_error           = FALSE;
BOOL dsr_has_lost_connection = FALSE;

void dsr_initialize()
{
	int WSAResult;
	WSADATA blah;


	WSAResult = WSAStartup(MAKEWORD(1, 0), &blah);

	if(WSAResult)
	{
		fprintf_f(stderr, "Couldn't initialize Winsock: ");

		switch(WSAResult)
		{
			case WSASYSNOTREADY:     fprintf_f(stderr, "The underlying network subsystem is not ready for network communication.\n\n"); break;
			case WSAVERNOTSUPPORTED: fprintf_f(stderr, "The version of Windows Sockets support requested is not provided by this particular Windows Sockets implementation.\n\n"); break;
			case WSAEINPROGRESS:     fprintf_f(stderr, "A blocking Windows Sockets 1.1 operation is in progress.\n\n"); break;
			case WSAEPROCLIM:        fprintf_f(stderr, "A limit on the number of tasks supported by the Windows Sockets implementation has been reached.\n\n"); break;
			case WSAEFAULT:          fprintf_f(stderr, "The lpWSAData parameter is not a valid pointer.\n\n"); break; 
			default:                 fprintf_f(stderr, "Unknown error.\n\n");
		}

		exit(1);
	}
}

SOCKET dsr_listen(u_short port)
{
	struct sockaddr_in serv_addr;
	SOCKET s;

	/* Get the server address */
	memset((char *) &serv_addr, '\0', sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(port);

	/* Create a socket */
	s = socket(AF_INET, SOCK_STREAM, 0);

	/* Bind the socket */
	if(bind(s, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
	{
		fprintf_f(stderr, "Error binding socket: ");
		dsr_winsock_fatal();
	}

	/* Switch the socket to listen mode */
	listen(s, 20);

	return s;
}

SOCKET dsr_connect(char *host, u_short port)
{
	SOCKET s;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	/* Create the socket */
	s = socket(AF_INET, SOCK_STREAM, 0);

	if(s < 0)
	{
		fprintf_f(stderr, "Error connecting to %s:%u: ", host, port);
		dsr_winsock_error();
	}
	else
	{
		/* Look up the host */
		server = gethostbyname(host);
		if(server == NULL)
		{
			fprintf_f(stderr, "Couldn't find host %s: ", host);
			dsr_winsock_error();
			s = SOCKET_ERROR;
		}
		else
		{
			/* Set up the server address */
			memset(&serv_addr, '\0', sizeof(serv_addr));
			serv_addr.sin_family = AF_INET;
			memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
			serv_addr.sin_port = htons(port);

			/* Connect */
			if(connect(s, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR)
			{
				fprintf_f(stderr, "Couldn't connect to host %s:%u: ", host, port);
				dsr_winsock_error();
				s = SOCKET_ERROR;
			}
			else
			{
				if(option_verbose >= 1)
					printf_f("Connected to %s:%u\n", host, port);
			}
		}
	}

	return s;
}

int dsr_send(SOCKET s, char *data, int length)
{
	return send(s, data, length, 0);
}

int dsr_recv(SOCKET s, char *buffer, int max_length)
{
	return recv(s, buffer, max_length, 0);
}

/* Returns true if there is a connection waiting on listener. */
BOOL dsr_select(SOCKET listener, socket_list *list, u_int *count)
{
	fd_set select_set;
	int select_return;
	SOCKET new_socket = 0;
	BOOL connection_waiting = FALSE;
	
	u_int i;

	/* Clear the current socket set */
	FD_ZERO(&select_set);

	/* Add the listener */
	if(listener > 0)
		FD_SET(listener, &select_set);

	/* Add all connected sockets */
	for(i = 0; i < *count; i++)
	{
		if(list[i].connected)
		{
			FD_SET(list[i].s, &select_set);
		}
	}

	/* Perform the select with no timeout (will potentially run forever) */
	select_return = select(0, &select_set, (fd_set*)NULL, (fd_set*)NULL, NULL);

	if(select_return == -1)
	{
		fprintf_f(stderr, "Select failed: ");
		dsr_winsock_fatal();
	}
	else
	{
		for(i = 0; i < *count; i++)
		{
			if(list[i].connected && FD_ISSET(list[i].s, &select_set))
			{
				char buffer[BUFFER_LENGTH];
				int length = dsr_recv(list[i].s, buffer, BUFFER_LENGTH);
				u_int j;

				if(length <= 0)
				{
					dsr_close(list[i].s);
					list[i].s = 0;
					list[i].connected = FALSE;

					fprintf_f(stderr, "Lost connection to %s:%u: \n", list[i].host, list[i].port);
					dsr_has_lost_connection = TRUE;
				}
				else
				{
					if(option_data >= 2 || option_verbose >= 2)
						printf_f("Distributing %d bytes from %s:%u\n", length, list[i].host, list[i].port);

					if(option_data >= 2)
						printf_f("---------------------------------\n");

					if(option_data >= 1)
					{
						int i;
						for(i = 0; i < length; i++)
						{
							if(option_sanitize_data && (buffer[i] < 0x20 || buffer[i] > 0x7F))
							{
								putchar('.');
							}
							else
							{
								putchar(buffer[i]);
							}
						}
					}

					if(option_data >= 2)
						printf_f("\n---------------------------------\n");

					for(j = 0; j < *count; j++)
					{
						if(j != i)
						{
							dsr_send(list[j].s, buffer, length);
						}
					}
				}
			}
		}

		if(listener > 0 && FD_ISSET(listener, &select_set))
			connection_waiting = TRUE;
	}

	return connection_waiting;
}

void dsr_close(SOCKET s)
{
	closesocket(s);
}

void dsr_winsock_fatal()
{
	dsr_winsock_error();
	fprintf_f(stderr, "\n");

	WSACleanup();
	exit(1);
}

void dsr_winsock_error()
{
	dsr_has_error = TRUE;

	switch(WSAGetLastError())
	{
		case WSAEINTR:           fprintf_f(stderr, "Interrupted function call\n"); break; 
		case WSAEACCES:          fprintf_f(stderr, "Permission denied\n"); break; 
		case WSAEFAULT:          fprintf_f(stderr, "Bad address\n"); break; 
		case WSAEINVAL:          fprintf_f(stderr, "Invalid argument\n"); break; 
		case WSAEMFILE:          fprintf_f(stderr, "Too many open files\n"); break; 
		case WSAEWOULDBLOCK:     fprintf_f(stderr, "Resource temporarily unavailable\n"); break; 
		case WSAEINPROGRESS:     fprintf_f(stderr, "Operation now in progress\n"); break; 
		case WSAEALREADY:        fprintf_f(stderr, "Operation already in progress\n"); break; 
		case WSAENOTSOCK:        fprintf_f(stderr, "Socket operation on nonsocket\n"); break; 
		case WSAEDESTADDRREQ:    fprintf_f(stderr, "Destination address required\n"); break; 
		case WSAEMSGSIZE:        fprintf_f(stderr, "Message too long\n"); break; 
		case WSAEPROTOTYPE:      fprintf_f(stderr, "Protocol wrong for socket\n"); break; 
		case WSAENOPROTOOPT:     fprintf_f(stderr, "Bad protocol option\n"); break; 
		case WSAEPROTONOSUPPORT: fprintf_f(stderr, "Protocol not supported\n"); break; 
		case WSAESOCKTNOSUPPORT: fprintf_f(stderr, "Socket type not supported\n"); break; 
		/* What's the difference between NOTSUPP and NOSUPPORT? :) */
		case WSAEOPNOTSUPP:      fprintf_f(stderr, "Operation not supported\n"); break; 
		case WSAEPFNOSUPPORT:    fprintf_f(stderr, "Protocol family not supported\n"); break; 
		case WSAEAFNOSUPPORT:    fprintf_f(stderr, "Address family not supported by protocol family\n"); break; 
		case WSAEADDRINUSE:      fprintf_f(stderr, "Address already in use\n"); break; 
		case WSAEADDRNOTAVAIL:   fprintf_f(stderr, "Cannot assign requested address\n"); break; 
		case WSAENETDOWN:        fprintf_f(stderr, "Network is down\n"); break; 
		case WSAENETUNREACH:     fprintf_f(stderr, "Network is unreachable\n"); break; 
		case WSAENETRESET:       fprintf_f(stderr, "Network dropped connection on reset\n"); break;
		case WSAECONNABORTED:    fprintf_f(stderr, "Software caused connection abort\n"); break; 
		case WSAECONNRESET:      fprintf_f(stderr, "Connection reset by peer\n"); break; 
		case WSAENOBUFS:         fprintf_f(stderr, "No buffer space avaialable\n"); break; 
		case WSAEISCONN:         fprintf_f(stderr, "Socket is already connected\n"); break; 
		case WSAENOTCONN:        fprintf_f(stderr, "Socket is not connected\n"); break; 
		case WSAESHUTDOWN:       fprintf_f(stderr, "Cannot send after socket shutdown\n"); break; 
		case WSAETIMEDOUT:       fprintf_f(stderr, "Connection timed out\n"); break; 
		case WSAECONNREFUSED:    fprintf_f(stderr, "Connection refused\n"); break; 
		case WSAEHOSTDOWN:       fprintf_f(stderr, "Host is down\n"); break; 
		case WSAEHOSTUNREACH:    fprintf_f(stderr, "No route to host\n"); break; 
		case WSAEPROCLIM:        fprintf_f(stderr, "Too many processes\n"); break; 
		/* Yes, some of these start with WSA, not WSAE. No, I don't know why. */
		case WSASYSNOTREADY:     fprintf_f(stderr, "Network subsystem is unavailable\n"); break; 
		case WSAVERNOTSUPPORTED: fprintf_f(stderr, "Winsock.dll out of range\n"); break; 
		case WSANOTINITIALISED:  fprintf_f(stderr, "Successful WSAStartup not yet performed\n"); break; 
		case WSAEDISCON:         fprintf_f(stderr, "Graceful shutdown no in progress\n"); break; 
		case WSATYPE_NOT_FOUND:  fprintf_f(stderr, "Class type not found\n"); break; 
		case WSAHOST_NOT_FOUND:  fprintf_f(stderr, "Host not found\n"); break; 
		case WSATRY_AGAIN:       fprintf_f(stderr, "Nonauthoritative host not found\n"); break; 
		case WSANO_RECOVERY:     fprintf_f(stderr, "This is a nonrecoverable error\n"); break; 
		case WSANO_DATA:         fprintf_f(stderr, "Valid name, no data record of requested type\n"); break;
		default:                 fprintf_f(stderr, "Unknown error: %d (0x%02x)\n", WSAGetLastError(), WSAGetLastError());
	}
}