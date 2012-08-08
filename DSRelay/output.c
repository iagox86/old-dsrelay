#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "output.h"

/* These functions ensure that text is actually flushed to the screen. This is required, for 
 * instance, for using this over netcat. */

void fprintf_f(FILE *handle, char *msg, ...)
{
   va_list argList;
   va_start(argList, msg);

   vfprintf(handle, msg, argList);

   va_end(argList);

   fflush(stdout);
}

void printf_f(char *msg, ...)
{
   va_list argList;
   va_start(argList, msg);

   vprintf(msg, argList);

   va_end(argList);

   fflush(stdout);
}