#if !defined(__MGX_CONF_H__)
#define __MGX_CONF_H__

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include "mgx_configs.h"
#include "mgx_mutex.h"

class Mgx_conf
{
private:
    Mgx_conf();

public:
    ~Mgx_conf();

private:
    static Mgx_conf *instance;
    std::unordered_map<std::string, std::string> configs;

public:
    static Mgx_conf *get_instance()
    {
        if (!instance) {
            instance = new Mgx_conf();
        }

        return instance;
    }
    bool load(const char *file_name);
    std::string get_string(const std::string &key, const std::string &default_str = "");
    int get_int(const std::string &key, const int default_int = 1);
};

#endif  // __MGX_CONF_H__
