#include "mgx_util.h"
#include "mgx_log.h"
#include <vector>
#include <string>
#include <sstream>
#include <execinfo.h>

static void mgx_backtrace(std::vector<std::string> &bt, int size, int skip)
{
    void **buf = (void **)malloc(sizeof(void *) *size);
    size_t nptrs = backtrace(buf, size);

    char **strs = backtrace_symbols(buf, nptrs);
    if (!strs) {
        mgx_log(MGX_LOG_EMERG, "backtrace_symbols error");
        return;
    }

    for (size_t i = skip; i < nptrs; i++)
        bt.push_back(strs[i]);

    free(strs);
    free(buf);
}

std::string mgx_backtrace2str(int size, int skip, const std::string &prefix)
{
    std::vector<std::string> bt;
    mgx_backtrace(bt, size, skip);
    std::stringstream ss;
    for (auto it = bt.begin(); it != bt.end(); it++)
        ss << std::endl << prefix << (*it);
    return ss.str();
}
