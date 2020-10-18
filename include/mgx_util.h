#ifndef __MGX_UTIL_H__
#define __MGX_UTIL_H__

#include <string>
#include <cassert>
#include "mgx_log.h"

std::string mgx_backtrace2str(int size, int skip = 1, const std::string &prefix = "     ");

#define MGX_ASSERT(exp, msg...) \
    do {  \
        if (!(exp)) { \
            mgx_log(MGX_LOG_STDERR, std::string("Assertion: " #exp) + \
                    "\n" + msg + "\n" + \
                    "backtrace: " +  \
                    mgx_backtrace2str(100)); \
            assert(exp); \
        }  \
    } while(0)

#endif
