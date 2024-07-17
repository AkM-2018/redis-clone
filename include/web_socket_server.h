#ifndef WEB_SOCKET_SERVER_H
#define WEB_SOCKET_SERVER_H

#include "crow_all.h"
#include "connect.h"
#include "logger.h"

class WebSocketServer
{
private:
    static crow::SimpleApp *app;
    static std::unordered_set<crow::websocket::connection *> users;
    static Connect *connect;

    WebSocketServer(){};

public:
    ~WebSocketServer();
    static void init();
    static std::unordered_set<crow::websocket::connection *> getUsers();
};

#endif