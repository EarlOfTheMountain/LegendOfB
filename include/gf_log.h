#ifndef GF_LOG_H
#define GF_LOG_H

/* Minimal leveled logging to stderr. */
enum { GF_LOG_DEBUG = 0, GF_LOG_INFO, GF_LOG_WARN, GF_LOG_ERROR };

void gf_log_impl(int level, const char *file, int line, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

#ifdef GF_DEBUG
#define GF_LOGD(...) gf_log_impl(GF_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#else
#define GF_LOGD(...) ((void)0)
#endif
#define GF_LOGI(...) gf_log_impl(GF_LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define GF_LOGW(...) gf_log_impl(GF_LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define GF_LOGE(...) gf_log_impl(GF_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#endif
