#if !defined(__MSG_STRING_H__)
#define __MSG_STRING_H__

#include <cstring>
#include <string>

#define memcpy_end(dst, src, n) (((u_char*)memcpy(dst, src, n)) + (n))

static inline std::string &strim(std::string& str)
{
    str.erase(0, str.find_first_not_of(' '));
    str.erase(str.find_last_not_of(' ') + 1);
    return str;
}

#endif  // __MSG_STRING_H__