#if !defined(__MGX_PRINTF_H__)
#define __MGX_PRINTF_H__

#include <unistd.h>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#define LF (u_char)'\n'
#define CR (u_char)'\r'
#define CRLF "\r\n"
#define mgx_abs(value) (((value) >= 0) ? (value) : -(value))
#define mgx_max(val1, val2) ((val1 < val2) ? (val2) : (val1))
#define mgx_min(val1, val2) ((val1 > val2) ? (val2) : (val1))

#define MGX_MAX_UINT32_VALUE (uint32_t)0xffffffff
#define MGX_INT64_LEN (sizeof("-9223372036854775808") - 1)

u_char *mgx_vslprintf(u_char *buf, u_char *last, const char *fmt, va_list args);
u_char *mgx_slprintf(u_char *buf, u_char *last, const char *fmt, ...);

#endif  // __MGX_PRINTF_H__
