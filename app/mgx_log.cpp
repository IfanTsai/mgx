#include "mgx_log.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cstdarg>
#include <cstdio>
#include <string>
#include "mgx_conf.h"
#include "mgx_printf.h"
#include "mgx_string.h"

const static int MGX_MAX_ERR_STR_SIZE = 10240;
const static int MGX_MAX_LOG_STR_SIZE = MGX_MAX_ERR_STR_SIZE;
const static char *STD_ERR_TAG = "mgx: ";
static mgx_log_t g_mgx_log;
static const char *log_tags[] = {
    "stderr", "emerg", "alert", "crit", "error", "warn", "notice", "info", "debug",
};

void mgx_log_stderr(const char *fmt, ...)
{
    u_char err_str_buf[MGX_MAX_ERR_STR_SIZE] = { 0 };

    u_char *p = memcpy_end(err_str_buf, STD_ERR_TAG, strlen(STD_ERR_TAG));

    va_list args;
    va_start(args, fmt);
    p = mgx_vslprintf(p, err_str_buf + MGX_MAX_ERR_STR_SIZE, fmt, args);
    va_end(args);

    if (p >= err_str_buf + MGX_MAX_ERR_STR_SIZE - 1)
        p = err_str_buf + MGX_MAX_ERR_STR_SIZE - 2;
    *p++ = '\n';
    *p = '\0';

    (void)write(STDERR_FILENO, err_str_buf, p - err_str_buf);
}

static char *get_cur_time_string(const char *time_format, char *time_buf)
{
    time_t t = time(nullptr);
    struct tm *cur_tm = localtime(&t);
    sprintf(time_buf, time_format,
            cur_tm->tm_year + 1900, cur_tm->tm_mon + 1, cur_tm->tm_mday,
            cur_tm->tm_hour, cur_tm->tm_min, cur_tm->tm_sec);
    return time_buf;
}

static void mk_pdirs(const char *fname)
{
    char dirname[FILENAME_MAX] = { 0 };
    sprintf(dirname, "%s", fname);

    struct stat dstat;
    char *p = dirname;
    while ( (p = strchr(p + 1, '/')) ) {
        *p = 0;
        if (stat(dirname, &dstat)) {
            mkdir(dirname, 0755);
        }
        *p = '/';
    }
}

void mgx_log_init()
{
    Mgx_conf *mgx_conf = Mgx_conf::get_instance();

    g_mgx_log.log_level = mgx_conf->get_int(CONFIG_LogLevel, DEFAULT_LOG_LEVEL);
    g_mgx_log.debug_mode = mgx_conf->get_int(CONFIG_DebugMode, DEFAULT_DEBUG_MODE) ? true : false;
    std::string log_path = mgx_conf->get_string(CONFIG_LogPathName, DEFAULT_LOG_PATH);

    char time_buf[64] = { 0 };
    get_cur_time_string("%04d/%02d/%02d %02d/%02d/%02d", time_buf);
    std::string log_file = log_path + "/" + strtok(time_buf, " ") + "/mgx.log";
    mk_pdirs(log_file.c_str());

    if (-1 != g_mgx_log.fd && STDERR_FILENO != (g_mgx_log.fd))
        close(g_mgx_log.fd);
    g_mgx_log.fd = open(log_file.c_str(), O_WRONLY | O_APPEND | O_SYNC | O_CREAT, 0644);
    if (g_mgx_log.fd < 0) {
        mgx_log_stderr("open %s error: %s", log_file.c_str(), strerror(errno));
        /* if log path open failed, use stderr */
        g_mgx_log.fd = STDERR_FILENO;
    }
}

void mgx_log(int level, const char *fmt, ...)
{
    if (level > g_mgx_log.log_level)
        return;

    char time_buf[64] = { 0 };
    get_cur_time_string("%04d-%02d-%02d %02d:%02d:%02d", time_buf);

    u_char log_str_buf[MGX_MAX_LOG_STR_SIZE] = { 0 };
    u_char *p = memcpy_end(log_str_buf, time_buf, strlen(time_buf));
    p = mgx_slprintf(p, log_str_buf + MGX_MAX_LOG_STR_SIZE, " [%s] %d: ", log_tags[level], getpid());

    va_list args;
    va_start(args, fmt);
    p = mgx_vslprintf(p, log_str_buf + MGX_MAX_LOG_STR_SIZE, fmt, args);
    va_end(args);

    if (p >= log_str_buf + MGX_MAX_LOG_STR_SIZE - 1)
        p = log_str_buf + MGX_MAX_LOG_STR_SIZE - 2;
    *p++ = '\n';
    *p = '\0';

    if (write(g_mgx_log.fd, log_str_buf, p - log_str_buf) != (p - log_str_buf)) {
        mgx_log_stderr("write log error: %s", strerror(errno));
        if (g_mgx_log.fd != STDERR_FILENO)
            close(g_mgx_log.fd);
        /* if write failed, write stderr */
        (void)write(STDERR_FILENO, log_str_buf, p - log_str_buf);
        return;
    }

    if (MGX_LOG_STDERR == level || g_mgx_log.debug_mode) {
        *(--p) = '\0';
        mgx_log_stderr((char *)log_str_buf);
    }
}

void mgx_log(int level, const std::string &str)
{
    mgx_log(level, str.c_str());
}
