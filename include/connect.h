#ifndef CONNECT_H
#define CONNECT_H

#include <cstdint>
#include <string>
#include <vector>
#include "./enums/state_enum.h"
#include "constants.h"

class Connect
{
public:
    uint32_t rescode = 0;
    size_t wbuf_size = 0;
    size_t wbuf_sent = 0;
    std::string wbuf;

public:
    void do_request(std::string);
    void do_set(std::vector<std::string>);
    void do_get(std::vector<std::string>);
    void do_del(std::vector<std::string>);
};

#endif