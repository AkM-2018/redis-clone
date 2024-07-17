#include "../include/server.h"
#include "../include/conn.h"
#include "../include/connect.h"
#include "../include/utils/print_utils.h"
#include "../include/enums/res_enum.h"
#include "../include/web_socket_server.h"

#include <iostream>

#include "../include/crow_all.h"

int main(int argc, char **argv)
{
    std::string input = "set key value";
    Connect *connect = new Connect();
    connect->do_request(input);
}