#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <vector>
#include <map>
#include <iostream>

#include "../include/conn.h"
#include "../include/utils/print_utils.h"
#include "../include/entry.h"
#include "../include/utils/string_utils.h"
#include "../include/enums/res_enum.h"

#define container_of(ptr, type, member) ({                  \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type, member) ); })

static struct
{
    HMap db;
} g_data;

static bool entry_eq(HNode *lhs, HNode *rhs)
{
    struct Entry *le = container_of(lhs, struct Entry, node);
    struct Entry *re = container_of(rhs, struct Entry, node);
    return le->key == re->key;
}

static uint32_t do_get(
    const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen)
{
    Entry key;
    key.key = cmd[1];
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (!node)
    {
        return RES_NX;
    }

    const std::string &val = container_of(node, Entry, node)->val;
    assert(val.size() <= k_max_msg);
    memcpy(res, val.data(), val.size());
    *reslen = (uint32_t)val.size();
    return RES_OK;
}

static uint32_t do_set(
    std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen)
{
    (void)res;
    (void)reslen;

    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_lookup(&g_data.db, &key.node, &entry_eq);
    if (node)
    {
        container_of(node, Entry, node)->val.swap(cmd[2]);
    }
    else
    {
        Entry *ent = new Entry();
        ent->key.swap(key.key);
        ent->node.hcode = key.node.hcode;
        ent->val.swap(cmd[2]);
        hm_insert(&g_data.db, &ent->node);
    }
    return RES_OK;
}

static uint32_t do_del(
    std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen)
{
    (void)res;
    (void)reslen;

    Entry key;
    key.key.swap(cmd[1]);
    key.node.hcode = str_hash((uint8_t *)key.key.data(), key.key.size());

    HNode *node = hm_pop(&g_data.db, &key.node, &entry_eq);
    if (node)
    {
        delete container_of(node, Entry, node);
    }
    return RES_OK;
}

/**
 * @brief parse the request from client and populate in cmd vector
 *
 * @param *data: request
 * @param len: length of the request
 * @param out: vector to populate commands
 *
 * @return 0 if successful, -1 otherwise
 */
static int32_t parse_req(
    const uint8_t *data, size_t len, std::vector<std::string> &out)
{
    if (len < 4)
    {
        return -1;
    }

    uint32_t n = 0;
    memcpy(&n, &data[0], 4);
    if (n > k_max_args)
    {
        return -1;
    }

    size_t pos = 4;
    while (n--)
    {
        if (pos + 4 > len)
        {
            return -1;
        }
        uint32_t sz = 0;
        memcpy(&sz, &data[pos], 4);
        if (pos + 4 + sz > len)
        {
            return -1;
        }
        out.push_back(std::string((char *)&data[pos + 4], sz));
        pos += 4 + sz;
    }

    if (pos != len)
    {
        return -1; // trailing garbage
    }
    return 0;
}

/**
 * @brief call the appropriate function according to request from client
 *
 * @param *req: request
 * @param reqlen: length of the request
 * @param *rescode: response code according to res_enum
 * @param *res: pointer to response array
 * @param *reslen: length of the response
 *
 * @return 0 if successful
 */
static int32_t do_request(
    const uint8_t *req, uint32_t reqlen,
    uint32_t *rescode, uint8_t *res, uint32_t *reslen)
{
    std::vector<std::string> cmd;
    if (0 != parse_req(req, reqlen, cmd))
    {
        msg("bad req");
        return -1;
    }

    for (auto ci : cmd)
        print(ci);
    print("");

    if (cmd.size() == 2 && cmd_is(cmd[0], "get"))
    {
        *rescode = do_get(cmd, res, reslen);
    }
    else if (cmd.size() == 3 && cmd_is(cmd[0], "set"))
    {
        *rescode = do_set(cmd, res, reslen);
    }
    else if (cmd.size() == 2 && cmd_is(cmd[0], "del"))
    {
        *rescode = do_del(cmd, res, reslen);
    }
    else
    {
        // cmd is not recognized
        *rescode = RES_ERR;
        const char *msg = "Unknown cmd";
        strcpy((char *)res, msg);
        *reslen = strlen(msg);
        return 0;
    }
    return 0;
}

/**
 * @brief sends all data in write-buffer to the client
 *
 * sends all data in write-buffer to client and updates buffer accordingly
 * if all data is sent, state is changed back to STATE_REQ
 *
 * @param *conn: pointer to the Conn object
 *
 * @return true if all data is sent, false otherwise.
 * in case true is returned the function is called again
 *
 */
bool try_flush_buffer(Conn *conn)
{
    print("inside try_flush_buffer...");
    ssize_t rv = 0;

    do
    {
        size_t remain = conn->wbuf_size - conn->wbuf_sent;
        rv = write(conn->fd, &conn->wbuf[conn->wbuf_sent], remain);
        print("rv flush", rv);
    } while (rv < 0 && errno == EINTR);

    if (rv < 0 && errno == EAGAIN)
    {
        // got EAGAIN, stop.
        return false;
    }

    if (rv < 0)
    {
        msg("write() error");
        conn->state = STATE_END;
        return false;
    }

    conn->wbuf_sent += (size_t)rv;
    assert(conn->wbuf_sent <= conn->wbuf_size);

    if (conn->wbuf_sent == conn->wbuf_size)
    {
        // response was fully sent, change state back
        conn->state = STATE_REQ;
        conn->wbuf_sent = 0;
        conn->wbuf_size = 0;
        return false;
    }

    // still got some data in wbuf, could try to write again
    return true;
}

void state_res(Conn *conn)
{
    while (try_flush_buffer(conn))
    {
    }
}

/**
 * @brief reads message from client and sends the same message back
 *
 * reads the message from the client
 *      if buffer size does not include the len of string, try in next iteation
 *      if the buffer size does not include the string itself, try in next iteration
 * copy the same message to write buffer and send it back to client
 * shift the read-buffer accordingly i.e. remove the part of message we just read
 *
 * @param *conn: pointer to the Conn object
 *
 * @return if the state is STATE_REQ, in which case it'll call the same function again
 */
static bool try_one_request(Conn *conn)
{
    print("inside try_one_request...");

    if (conn->rbuf_size < 4)
    {
        /**
         * not enough data in the buffer. will retry in the next iteration
         * state is still STATE_REQ, so it will call try_fill_buffer again
         * which inturn calls try_one_request again
         */
        return false;
    }

    uint32_t len = 0;
    memcpy(&len, &conn->rbuf[0], 4);
    if (len > k_max_msg)
    {
        msg("too long");
        conn->state = STATE_END;
        return false;
    }

    if (4 + len > conn->rbuf_size)
    {
        /**
         * not enough data in the buffer. will retry in the next iteration
         * state is still STATE_REQ, so it will call try_fill_buffer again
         * which inturn calls try_one_request again
         */
        return false;
    }

    // got one request generate the response
    uint32_t rescode = 0;
    uint32_t wlen = 0;
    int32_t err = do_request(&conn->rbuf[4], len,
                             &rescode, &conn->wbuf[4 + 4], &wlen);

    if (err)
    {
        conn->state = STATE_END;
        return false;
    }

    wlen += 4;
    memcpy(&conn->wbuf[0], &wlen, 4);
    memcpy(&conn->wbuf[4], &rescode, 4);
    conn->wbuf_size = 4 + wlen;

    size_t remain = conn->rbuf_size - 4 - len;
    if (remain)
    {
        memmove(conn->rbuf, &conn->rbuf[4 + len], remain);
    }
    conn->rbuf_size = remain;

    // change state
    conn->state = STATE_RES;
    state_res(conn);

    // continue the outer loop if the request was fully processed
    return (conn->state == STATE_REQ);
}

/**
 * @brief fills buffer and calls try_one_request
 *
 * fills the read buffer to maximum cap possible
 * while the state is in request mode, calls try_one_request in a loop
 *
 * @param *conn: pointer to the Conn object
 *
 */
bool try_fill_buffer(Conn *conn)
{
    print("inside try_fill_buffer...");
    assert(conn->rbuf_size < sizeof(conn->rbuf));
    ssize_t rv = 0;

    /**
     * EINTR - returned when the function was interrupted by a signal
     * before the function could finish its normal task
     *
     */
    do
    {
        size_t cap = sizeof(conn->rbuf) - conn->rbuf_size;
        print("cap", cap);
        rv = read(conn->fd, &conn->rbuf[conn->rbuf_size], cap);
        print("rv fill", rv);
    } while (rv < 0 && errno == EINTR);

    /**
     * EAGAIN - is often raised when performing non-blocking I/O
     * it means that there is no data available right now, try later
     *
     */
    if (rv < 0 && errno == EAGAIN)
    {
        return false;
    }

    if (rv < 0)
    {
        msg("read() error");
        conn->state = STATE_END;
        return false;
    }

    if (rv == 0)
    {
        if (conn->rbuf_size > 0)
        {
            msg("unexpected EOF");
        }
        else
        {
            msg("EOF");
        }
        conn->state = STATE_END;
        return false;
    }

    conn->rbuf_size += (size_t)rv;
    assert(conn->rbuf_size <= sizeof(conn->rbuf));

    while (try_one_request(conn))
    {
    }

    return (conn->state == STATE_REQ);
}

/**
 * @brief
 *
 * @param *conn: pointer to the Conn object
 *
 */
void state_req(Conn *conn)
{
    while (try_fill_buffer(conn))
    {
    }
}

/**
 * @brief calls respective connection state function for the connection
 *
 * @param *conn: pointer to the Conn object
 *
 */
void Conn::connection_io()
{
    if (this->state == STATE_REQ)
    {
        state_req(this);
    }
    else if (this->state == STATE_RES)
    {
        state_res(this);
    }
    else
    {
        assert(0); // not expected
    }
}