#include "gf_log.h"

#include <stdarg.h>
#include <stdio.h>

void gf_log_impl(int level, const char *file, int line, const char *fmt, ...)
{
    static const char *const names[] = { "DEBUG", "INFO", "WARN", "ERROR" };
    va_list ap;

    if (level < 0 || level > GF_LOG_ERROR) level = GF_LOG_ERROR;
    fprintf(stderr, "[%s] %s:%d: ", names[level], file, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}
