#include "../include/web_socket_server.h"

crow::SimpleApp *WebSocketServer::app;
Connect *WebSocketServer::connect;
std::unordered_set<crow::websocket::connection *> WebSocketServer::users;

void WebSocketServer::init()
{
    connect = new Connect();
    app = new crow::SimpleApp();

    CROW_WEBSOCKET_ROUTE((*app), "/ws")
        .onopen([&](crow::websocket::connection &conn) {})
        .onclose([&](crow::websocket::connection &conn, const std::string &reason) {})
        .onmessage([&](crow::websocket::connection &conn, const std::string &data, bool is_binary)
                   {
                       connect->do_request(data);
                       std::string rescode = "[" + std::to_string(connect->rescode) + "]: ";
                       std::string text = connect->wbuf_size != 0 ? connect->wbuf : "";
                       conn.send_text(rescode + text);
                       Logger::sendMessage("Success"); });

    CROW_WEBSOCKET_ROUTE((*app), "/log")
        .onopen([&](crow::websocket::connection &conn)
                { 
                    CROW_LOG_INFO << "new websocket connection: " + conn.get_remote_ip();
                    users.insert(&conn); });

    (*app).port(18080).multithreaded().run();
}

std::unordered_set<crow::websocket::connection *> WebSocketServer::getUsers()
{
    return users;
}

WebSocketServer::~WebSocketServer()
{
    delete connect;
}