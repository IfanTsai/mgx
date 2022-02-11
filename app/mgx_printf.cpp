#include "mgx_printf.h"
#include "mgx_string.h"

static u_char *mgx_sprintf_num(u_char *buf, u_char *last, uint64_t ui64,
                               u_char zero, uintptr_t hexadecimal,
                               uintptr_t width)
{
    u_char *p, temp[MGX_INT64_LEN + 1];
    /*
     * we need temp[MGX_INT64_LEN] only,
     * but icc issues the warning
     */
    size_t len;
    static u_char hex[] = "0123456789abcdef";
    static u_char HEX[] = "0123456789ABCDEF";

    p = temp + MGX_INT64_LEN;

    if (hexadecimal == 0) {
        if (ui64 <= (uint64_t)MGX_MAX_UINT32_VALUE) {
            /*
             * To divide 64-bit numbers and to find remainders
             * on the x86 platform gcc and icc call the libc functions
             * [u]divdi3() and [u]moddi3(), they call another function
             * in its turn.  On FreeBSD it is the qdivrem() function,
             * its source code is about 170 lines of the code.
             * The glibc counterpart is about 150 lines of the code.
             *
             * For 32-bit numbers and some divisors gcc and icc use
             * a inlined multiplication and shifts.  For example,
             * unsigned "i32 / 10" is compiled to
             *
             *     (i32 * 0xCCCCCCCD) >> 35
             */

            uint32_t ui32 = (uint32_t)ui64;

            do {
                *--p = (u_char)(ui32 % 10 + '0');
            } while (ui32 /= 10);

        } else {
            do {
                *--p = (u_char)(ui64 % 10 + '0');
            } while (ui64 /= 10);
        }

    } else if (hexadecimal == 1) {
        do {
            /* the "(uint32_t)" cast disables the BCC's warning */
            *--p = hex[(uint32_t)(ui64 & 0xf)];

        } while (ui64 >>= 4);

    } else { /* hexadecimal == 2 */

        do {
            /* the "(uint32_t)" cast disables the BCC's warning */
            *--p = HEX[(uint32_t)(ui64 & 0xf)];

        } while (ui64 >>= 4);
    }

    /* zero or space padding */

    len = (temp + MGX_INT64_LEN) - p;

    while (len++ < width && buf < last) {
        *buf++ = zero;
    }

    /* number safe copy */

    len = (temp + MGX_INT64_LEN) - p;

    if (buf + len > last) {
        len = last - buf;
    }

    return memcpy_end(buf, p, len);
}

u_char *mgx_vslprintf(u_char *buf, u_char *last, const char *fmt, va_list args)
{
    u_char *p, zero;
    int d;
    double f;
    size_t len, slen;
    int64_t i64;
    uint64_t ui64, frac;
    uintptr_t ms;
    uintptr_t width, sign, hex, frac_width, scale, n;

    while (*fmt && buf < last) {
        /*
         * "buf < last" means that we could copy at least one character:
         * the plain character, "%%", "%c", and minus without the checking
         */

        if (*fmt == '%') {
            i64 = 0;
            ui64 = 0;

            zero = (u_char)((*++fmt == '0') ? '0' : ' ');
            width = 0;
            sign = 1;
            hex = 0;
            frac_width = 0;
            slen = (size_t)-1;

            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt++ - '0');
            }

            for (;;) {
                switch (*fmt) {
                    case 'u':
                        sign = 0;
                        fmt++;
                        continue;

                    case 'X':
                        hex = 2;
                        sign = 0;
                        fmt++;
                        continue;

                    case 'x':
                        hex = 1;
                        sign = 0;
                        fmt++;
                        continue;

                    case '.':
                        fmt++;

                        while (*fmt >= '0' && *fmt <= '9') {
                            frac_width = frac_width * 10 + (*fmt++ - '0');
                        }

                        break;

                    case '*':
                        slen = va_arg(args, size_t);
                        fmt++;
                        continue;

                    default:
                        break;
                }

                break;
            }

            switch (*fmt) {
                case 's':
                    p = va_arg(args, u_char *);

                    if (slen == (size_t)-1) {
                        while (*p && buf < last) {
                            *buf++ = *p++;
                        }

                    } else {
                        len = mgx_min(((size_t)(last - buf)), slen);
                        buf = memcpy_end(buf, p, len);
                    }

                    fmt++;

                    continue;

                case 'O':
                    i64 = (int64_t)va_arg(args, off_t);
                    sign = 1;
                    break;

                case 'P':
                    i64 = (int64_t)va_arg(args, pid_t);
                    sign = 1;
                    break;

                case 'T':
                    i64 = (int64_t)va_arg(args, time_t);
                    sign = 1;
                    break;

                case 'M':
                    ms = (uintptr_t)va_arg(args, uintptr_t);
                    if ((intptr_t)ms == -1) {
                        sign = 1;
                        i64 = -1;
                    } else {
                        sign = 0;
                        ui64 = (uint64_t)ms;
                    }
                    break;

                case 'z':
                    if (sign) {
                        i64 = (int64_t)va_arg(args, ssize_t);
                    } else {
                        ui64 = (uint64_t)va_arg(args, size_t);
                    }
                    break;

                case 'i':
                case 'd':
                    if (sign) {
                        i64 = (int64_t)va_arg(args, int);
                    } else {
                        ui64 = (uint64_t)va_arg(args, u_int);
                    }
                    break;

                case 'l':
                    if (sign) {
                        i64 = (int64_t)va_arg(args, long);
                    } else {
                        ui64 = (uint64_t)va_arg(args, u_long);
                    }
                    break;

                case 'D':
                    if (sign) {
                        i64 = (int64_t)va_arg(args, int32_t);
                    } else {
                        ui64 = (uint64_t)va_arg(args, uint32_t);
                    }
                    break;

                case 'L':
                    if (sign) {
                        i64 = va_arg(args, int64_t);
                    } else {
                        ui64 = va_arg(args, uint64_t);
                    }
                    break;

                case 'f':
                    f = va_arg(args, double);

                    if (f < 0) {
                        *buf++ = '-';
                        f = -f;
                    }

                    ui64 = (int64_t)f;
                    frac = 0;

                    if (frac_width) {
                        scale = 1;
                        for (n = frac_width; n; n--) {
                            scale *= 10;
                        }

                        frac = (uint64_t)((f - (double)ui64) * scale + 0.5);

                        if (frac == scale) {
                            ui64++;
                            frac = 0;
                        }
                    }

                    buf = mgx_sprintf_num(buf, last, ui64, zero, 0, width);

                    if (frac_width) {
                        if (buf < last) {
                            *buf++ = '.';
                        }

                        buf = mgx_sprintf_num(buf, last, frac, '0', 0,
                                              frac_width);
                    }

                    fmt++;

                    continue;

                case 'p':
                    ui64 = (uintptr_t)va_arg(args, void *);
                    hex = 2;
                    sign = 0;
                    zero = '0';
                    width = 2 * sizeof(void *);
                    break;

                case 'c':
                    d = va_arg(args, int);
                    *buf++ = (u_char)(d & 0xff);
                    fmt++;

                    continue;

                case 'Z':
                    *buf++ = '\0';
                    fmt++;

                    continue;

                case 'N':
#if (MGX_WIN32)
                    *buf++ = CR;
                    if (buf < last) {
                        *buf++ = LF;
                    }
#else
                    *buf++ = LF;
#endif
                    fmt++;

                    continue;

                case '%':
                    *buf++ = '%';
                    fmt++;

                    continue;

                default:
                    *buf++ = *fmt++;

                    continue;
            }

            if (sign) {
                if (i64 < 0) {
                    *buf++ = '-';
                    ui64 = (uint64_t)-i64;

                } else {
                    ui64 = (uint64_t)i64;
                }
            }

            buf = mgx_sprintf_num(buf, last, ui64, zero, hex, width);

            fmt++;

        } else {
            *buf++ = *fmt++;
        }
    }

    return buf;
}

u_char *mgx_slprintf(u_char *buf, u_char *last, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    buf = mgx_vslprintf(buf, last, fmt, args);
    va_end(args);

    return buf;
}
