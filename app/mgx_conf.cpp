#include "mgx_conf.h"
#include "mgx_string.h"

Mgx_conf::Mgx_conf() {}

Mgx_conf::~Mgx_conf() {}

Mgx_conf *Mgx_conf::instance = nullptr;

bool Mgx_conf::load(const char *file_name)
{
    std::ifstream is(file_name);
    if (!is.is_open()) return false;

    std::string line;
    while (std::getline(is, line)) {
        /* empty line */
        if (line.empty()) continue;
        /* strim */
        strim(line);
        // comment or [] or // or /*
        if (line[0] == '#' || line[0] == '[' || line[0] == '/') continue;

        /* split by '=', get key and value */
        int index = line.find_first_of('=');
        std::string key = line.substr(0, index - 1);
        std::string val = line.substr(index + 1, line.size() - 1);

        /* put key and value in configs map */
        configs[strim(key)] = strim(val);
    }
    is.close();
    return true;
}

std::string Mgx_conf::get_string(const std::string &key, std::string default_str)
{
    return (configs.find(key) != configs.end()) ? configs[key] : default_str;
}

int Mgx_conf::get_int(const std::string &key, int default_int)
{
    std::string val = get_string(key);
    if (val.empty()) return default_int;

    return stoi(val);
}