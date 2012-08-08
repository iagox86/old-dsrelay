#ifndef __TYPES_H__
#define __TYPES_H__

#include <sys/types.h>
#include <winsock2.h>

typedef signed char 	int8_t;
typedef unsigned char 	uint8_t;
typedef signed short 	int16_t;
typedef unsigned short 	uint16_t;
typedef signed int 	int32_t;
typedef unsigned int 	uint32_t;
typedef signed long long 	int64_t;
typedef unsigned long long 	uint64_t;

#define HOST_LENGTH 256

typedef enum
{
	SOURCE_CONNECT,
	SOURCE_LISTEN
} SOURCE;

typedef struct
{
	char host[HOST_LENGTH];
	u_short port;

	SOCKET s;
	BOOL connected;

	SOURCE source;
} socket_list;

#endif
