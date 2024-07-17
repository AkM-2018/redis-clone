#include "../include/logger.h"

void Logger::sendMessage(std::string message)
{
    std::unordered_set<crow::websocket::connection *> users = WebSocketServer::getUsers();

    for (auto user : users)
    {
        std::string ip = (user)->get_remote_ip();
    }

    auto user = users.begin();

    std::string ip = (*user)->get_remote_ip();
    (*user)->send_text(ip + " " + message);
}