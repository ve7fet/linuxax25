#include <stdarg.h>
#include <syslog.h>

#include "ax25ipd.h"

void LOGLn(int level, const char *format, ...)
{
	va_list va;

	va_start(va, format);

	if (loglevel >= level)
		vsyslog(LOG_DAEMON | LOG_WARNING, format, va);

	va_end(va);
}
