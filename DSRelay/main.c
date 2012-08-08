/** Dead Simple Relay
 * By Ron Bowes
 *
 * This program is a very simple relay that will either listen on one port and connect on
 * another, or connect to two hosts on two ports. In either case, it will send all data
 * from one side to the other and back. 
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>

#include "dsr_winsock.h"
#include "getopt.h"
#include "options.h"
#include "output.h"
#include "types.h"

#define MAX_SOCKETS 256

BOOL check_connection();
int get_active_count();
void usage(char *program);
void split_param(char *in, char **host, u_short *port);
void fatal();
BOOL is_numeric(char *str);
BOOL is_valid_port(char *str);
void connect_all();
void reconnect_all();

void add_socket(SOCKET s, char *host, u_short port, BOOL connected, SOURCE source);

u_int count = 0;
u_int accepted_count = 0;
BOOL started_yet = FALSE;

SOCKET listener = 0;
socket_list sockets[MAX_SOCKETS];

int main(int argc, char *argv[])
{
	BOOL listen = FALSE;
	char c;

	opterr = 0;

	/* This big loop and switch() parse the command-line parameters. */
	while ((c = getopt (argc, argv, "l:wW:vdst:Ter")) != -1)
	{
		switch (c)
        {
        case 'l':
			if(is_valid_port(optarg))
			{
				option_listen_port = atoi(optarg);
			}
			else
			{
				fprintf_f(stderr, "Invalid port.\n\n");
				usage(argv[0]);
			}
             break;

		case 'w':
			option_wait++;
			break;

		case 'W':
			if(is_numeric(optarg))
			{
				option_wait = atoi(optarg);
			}
			else
			{
				fprintf_f(stderr, "Wait count must be numeric.\n\n");
				usage(argv[0]);
			}
			break;
		
		case 'v':
			option_verbose++;
			break;

		case 'd':
			option_data++;
			break;

		case 's':
			option_sanitize_data++;
			break;

		case 't':
			if(is_numeric(optarg))
			{
				option_terminate_at = atoi(optarg);
			}
			else
			{
				fprintf_f(stderr, "Terminate count must be numeric.\n\n");
				usage(argv[0]);
			}
			break;

		case 'T':
			option_terminate_on_close = TRUE;
			break;

		case 'e':
			option_terminate_on_error = TRUE;
			break;

		case 'r':
			option_reconnect_on_close = TRUE;
			break;

        case '?':
            fprintf_f (stderr, "Unknown option `-%c'.\n\n", optopt);
			usage(argv[0]);

        default:
             return 1;
        }
	}

	if(optind >= argc && option_listen_port == 0)
	{
		fprintf_f(stderr, "Nothing to do.\n\n");
		usage(argv[0]);
	}

	if(option_terminate_on_close && option_terminate_at)
	{
		fprintf_f(stderr, "Options -t and -T conflict.\n\n");
		usage(argv[0]);
	}

	if(option_terminate_on_close && option_reconnect_on_close)
	{
		fprintf_f(stderr, "Options -T and -r conflict.\n\n");
		usage(argv[0]);
	}

	if(optind >= argc)
	{
		if(option_verbose >= 1)
			printf_f("Entering listen-only mode.\n");
	}

	for(; optind < argc; optind++)
	{
		char *host;
		u_short port;

		split_param(argv[optind], &host, &port);

		if(host == NULL || port == 0)
		{
			fprintf_f(stderr, "Invalid argument: %s\n\n", argv[optind]);
			usage(argv[0]);
		}

		add_socket(0, host, port, FALSE, SOURCE_CONNECT);
	}

	dsr_initialize();

	if(option_wait == 0)
	{
		if(option_verbose >= 1)
			printf_f("Connecting to hosts\n");
		connect_all();
	}
	else
	{
		if(option_verbose >= 1)
			printf_f("Waiting for %d incoming connection%s before connecting\n", option_wait, option_wait == 1 ? "" : "s");
	}

	if(option_listen_port)
	{
		if(option_verbose >= 1)
			printf_f("Listening on port %u\n", option_listen_port);
		listener = dsr_listen(option_listen_port);
	}

	while(check_connection(get_active_count()))
	{
		struct sockaddr_in client_address;
		int client_length = sizeof(client_address);

		BOOL new_connection = dsr_select(listener, sockets, &count);

		if(new_connection)
		{
			SOCKET new_socket = accept(listener, (struct sockaddr *) &client_address, &client_length);
			char *client_host = inet_ntoa(client_address.sin_addr);
			u_short client_port = client_address.sin_port;

			add_socket(new_socket, client_host, client_port, TRUE, SOURCE_LISTEN);
			accepted_count++;
			
			if(option_verbose >= 1)
				printf_f("Connection accepted from %s:%u\n", client_host, client_port);

			if(accepted_count < option_wait)
			{
				if(option_verbose >= 1)
				{
					int remaining = option_wait - accepted_count;
					printf_f("Waiting for %d more connection%s\n", remaining, remaining == 1 ? "" : "s");
				}
			}

			if(accepted_count == option_wait)
			{
				if(option_verbose >= 1)
					printf_f("Inbound threshold reached, connecting out\n");
				connect_all();
			}
		}

		if(dsr_has_lost_connection)
		{
			reconnect_all();
			dsr_has_lost_connection = FALSE;
		}
	}

	WSACleanup();
	exit(0);
}

BOOL check_connection()
{
	BOOL result = TRUE;
	int active_count = get_active_count();

	if(option_terminate_on_close && dsr_has_lost_connection)
	{
		result = FALSE;
		fprintf_f(stderr, "A connection was lost, terminating.\n");
	}

	if(option_terminate_on_error && dsr_has_error)
	{
		result = FALSE;
		fprintf_f(stderr, "A connection error occurred, terminating.\n");
	}

	if(started_yet && option_terminate_at >= 0 && active_count <= option_terminate_at)
	{
		result = FALSE;
		fprintf_f(stderr, "Connection count fell to %d, terminating.\n", get_active_count());
	}

	return result;
}

int get_active_count()
{
	/* Count how many connections we have left. */
	int active_count = 0;
	u_int i;

	for(i = 0; i < count; i++)
	{
		if(sockets[i].connected)
			active_count++;
	}

	return active_count;
}

void usage(char *program)
{
	/* Ruler (for console width): */
	/*        12345678901234567890123456789012345678901234567890123456789012345678901234567890 */
	printf_f("Usage: %s [options] [<host:port> [<host:port>[<host:port>[...]]]]\n", program);
	printf_f("\n");
	printf_f("Options\n");
	printf_f("-l <port>      Listen for incoming connections\n");
	printf_f("-w             Wait for an incoming connection before making outbound\n");
	printf_f("               connections (must be in listen mode). Use multiple 'w's to\n");
	printf_f("               wait for multiple incoming connections (-ww, -www, -www, ...)\n");
	printf_f("-W <N>         As -w, but wait for N incoming connections\n");
	printf_f("-v             Be verbose (print notifications for connects/disconnects)\n");
	printf_f("-vv            Be very verbose (print notifications for packets)\n");
	printf_f("-d             Show raw data\n");
	printf_f("-dd            Show raw data with some context\n");
	printf_f("-s             Sanitize the raw data (replace non-printable characters,\n");
	printf_f("               including newlines)\n");
	printf_f("-t <N>         Terminate when there are <=N active connections (default 0)\n");
	printf_f("               Note: only happens after waiting (-w) threshold is reached\n");
	printf_f("-T             Terminates when any connection closes\n");
	printf_f("-e             Terminate on any winsock error (eg, failed connection)\n");
	printf_f("-r             Restarts each outbound connection when any connection ends\n");

/*	printf_f("-c <host:port> Give 'control' to the specified host:port combinations\n");
	printf_f("               (rather than relaying packets, they are processed).\n");*/
	printf_f("\n");
	printf_f("Either -l or multiple outgoing connections must be given.\n");
	printf_f("\n");
	printf_f("Example 1, to create a relay between localhost and Google, watching data:\n");
	printf_f("c:\\> %s -vv -dd -eT -w -l 80 www.google.ca:80\n", program);
	printf_f("\n");
	printf_f("Example 2, to create an outbound-only tunnel to Google, watching data:\n");
	printf_f("c:\\> %s -eT localhost:4444 www.google.ca:80\n", program);
	printf_f("\n");
	printf_f("Example 3, to create a tunnel to a locally-running VNC server, with a monitor\n");
	printf_f("           (listens on 5901 (vnc:1), relays data to 5900 (vnc:0), and copies it\n");
	printf_f("           to 4444 (presumably a netcat listener)\n");
	printf_f("c:\\> %s -w -e -T -l 5901 localhost:5900 localhost:4444\n", program);
	printf_f("\n");
	printf_f("Example 4, to forward a Hydra attack against a FTP server\n");
	printf_f("           (here, we use a second connection (probably a netcat client) to\n");
	printf_f("           the connection. Every time Hydra reconnects, the connection resets,\n");
	printf_f("           but when the other disconnects, it falls below the threshold of 1\n");
	printf_f("           connection and the session terminates.\n");
	printf_f("           Note: Hydra must be set to one connection (-t1) for this to work.\n");
	printf_f("c:\\> %s -ww -t1 -r -dd -vv -l 21 192.168.2.15:21\n", program);
	printf_f("\n");

	fatal();
}

/** This function is super simple, just split a <host>:<port> into its components. 
 * if the host isn't specified, NULL is returned. If the port isn't, it's set to 0. 
 * If no colon is present, it's assumed to be a host only. */
void split_param(char *in, char **host, u_short *port)
{
	char *colon = strchr(in, ':');
	char *port_start;
	int port_valid = TRUE;

	*host = NULL;
	*port = 0;

	if(colon)
	{
		*host = in;
		 port_start = colon + 1;
		 *colon = '\0';

		if(is_valid_port(port_start))
			*port = atoi(port_start);
	}
}

void fatal()
{
	exit(1);
}

BOOL is_numeric(char *str)
{
	size_t i;
	BOOL valid = TRUE;

	for(i = 0; i < strlen(str) && valid; i++)
	{
		if(!isdigit(str[i]))
			valid = FALSE;
	}

	return valid;
}

BOOL is_valid_port(char *str)
{
	BOOL valid = FALSE;

	if(is_numeric(str))
	{
		if(atoi(str) > 0 && atoi(str) < 65536)
			valid = TRUE;
	}

	return valid;
}

void add_socket(SOCKET s, char *host, u_short port, BOOL connected, SOURCE source)
{
	if(count >= MAX_SOCKETS)
	{
		fprintf_f(stderr, "Too many sockets! (max is %d)\n\n", MAX_SOCKETS);
		exit(1);
	}

	sockets[count].port      = port;
	sockets[count].s         = s; 
	sockets[count].connected = connected;
	sockets[count].source    = source;
	strncpy_s(sockets[count].host, HOST_LENGTH - 1, host, HOST_LENGTH - 1);

	count++;
}

void connect_all()
{
	u_int i;

	started_yet = TRUE;

	for(i = 0; i < count; i++)
	{
		if(sockets[i].source != SOURCE_LISTEN && !sockets[i].connected)
		{
			SOCKET s = dsr_connect(sockets[i].host, sockets[i].port);

			if(s != SOCKET_ERROR)
			{
				sockets[i].s         = s;
				sockets[i].connected = TRUE;
			}
		}
	}
}

void reconnect_all()
{
	u_int i;

	if(option_verbose >= 1)
		printf_f("Reconnecting to all hosts.\n");

	for(i = 0; i < count; i++)
	{
		if(sockets[i].source == SOURCE_CONNECT)
		{
			dsr_close(sockets[i].s);
		}
	}

	for(i = 0; i < count; i++)
	{
		if(sockets[i].source == SOURCE_CONNECT)
		{
			SOCKET new_socket = dsr_connect(sockets[i].host, sockets[i].port);
			sockets[i].s = new_socket;
		}
	}
}
