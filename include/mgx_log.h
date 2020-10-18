#if !defined(__MGX_LOG_H__)
#define __MGX_LOG_H__

#include <string>

#define DEFAULT_LOG_LEVEL 6
#define DEFAULT_DEBUG_MODE 0
#define DEFAULT_LOG_PATH  "./log/"

#define MGX_LOG_STDERR 0
#define MGX_LOG_EMERG  1
#define MGX_LOG_ALERT  2
#define MGX_LOG_CRIT   3
#define MGX_LOG_ERR    4
#define MGX_LOG_WARN   5
#define MGX_LOG_NOTICE 6
#define MGX_LOG_INFO   7
#define MGX_LOG_DEBUG  8

typedef struct {
    int log_level;
    int fd = -1;
    bool debug_mode;
} mgx_log_t;

void mgx_log_stderr(const char *fmt, ...);
void mgx_log_init();
void mgx_log(int level, const char *fmt, ...);
void mgx_log(int level, const std::string &str);
void mgx_log_free();

#endif  // __MGX_LOG_H__
