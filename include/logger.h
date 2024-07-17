#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include "web_socket_server.h"

class Logger
{
public:
    static void sendMessage(std::string message);
};

#endif