#include <stdio.h>
#include <string.h>
#include <stdarg.h>

// buffer
static char buffer[isolink_max_error] = {0}, *bufferp = 0;

void _set_error(const char *format, ...) {
	va_list valist;
	va_start(valist, format);
	vsprintf(bufferp = buffer, format, valist);
}

const char* isolink_get_error() {
	const char *ret = bufferp;
	bufferp = 0;
	return ret;
}
