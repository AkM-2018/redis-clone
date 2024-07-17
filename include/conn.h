#ifndef CONN_H
#define CONN_H

#include "./enums/state_enum.h"
#include "constants.h"

class Conn
{
public:
    int fd = -1;
    uint32_t state = 0; // either STATE_REQ or STATE_RES

    size_t rbuf_size = 0;
    uint8_t rbuf[4 + k_max_msg];

    size_t wbuf_size = 0;
    size_t wbuf_sent = 0;
    uint8_t wbuf[4 + k_max_msg];

    void connection_io();
};

#endif