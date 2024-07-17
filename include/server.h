#ifndef SERVER_H
#define SERVER_H

#include <vector>
#include <cstdint>
#include <poll.h>

#include "./conn.h"

class Server
{
private:
    std::vector<Conn *> fd2conn;
    std::vector<struct pollfd> poll_args;

    int32_t accept_new_conn(int);

public:
    Server();
    int init();
    int set_non_blocking(int);
    void run_server(int);
};

#endif