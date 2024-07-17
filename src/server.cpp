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

#include "../include/server.h"
#include "../include/conn.h"
#include "../include/constants.h"
#include "../include/utils/print_utils.h"
#include "../include/utils/string_utils.h"
#include "../include/enums/state_enum.h"
#include "../include/enums/res_enum.h"
#include "../include/entry.h"
#include "../include/hashtable.h"
#include "../include/enums/status_enum.h"

/**
 * @brief adds a new conn to the connection list
 *
 * the connection is added to the vector indexed by it fd
 *
 * @param fd2conn: vector of pointer to connections
 * @param *conn: pointer to the new connection object
 *
 */
void conn_put(std::vector<Conn *> &fd2conn, struct Conn *conn)
{
    if (fd2conn.size() <= (size_t)conn->fd)
    {
        fd2conn.resize(conn->fd + 1);
    }
    fd2conn[conn->fd] = conn;
}

/**
 * @brief accepts a new connection and adds it to fd2conn vector
 *
 * the new connection is added in the request state
 *
 * @param fd2conn: vector of pointer to connections
 * @param fd: file descriptor of the new connection
 *
 * @return -1 if error, 0 otherwise
 *
 */
int32_t Server::accept_new_conn(int fd)
{
    // accept
    struct sockaddr_in client_addr = {};
    socklen_t socklen = sizeof(client_addr);

    /**
     * accept new connection on the address
     *
     * int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
     *
     * populates the client_addr struct when a connection is recieved
     *
     * returns a new file descriptor for this connection
     *      this can be used to send() and recv()
     *      the orignal sockfd is still listening for new connections
     *
     * -1 is returned in case of an error and errno is set accordingly
     *
     */
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &socklen);
    if (connfd < 0)
    {
        msg("accept() error");
        return -1; // error
    }

    // set the new connection fd to nonblocking mode
    set_non_blocking(connfd);

    // creating the struct Conn
    struct Conn *conn = (struct Conn *)malloc(sizeof(struct Conn));
    if (!conn)
    {
        close(connfd);
        return -1;
    }

    conn->fd = connfd;
    conn->state = STATE_REQ;
    conn->rbuf_size = 0;
    conn->wbuf_size = 0;
    conn->wbuf_sent = 0;
    conn_put(fd2conn, conn);

    return 0;
}

Server::Server()
{
}

int Server::init()
{
    /**
     * get the file descriptor
     *
     * int socket(int domain, int type, int protocol);
     *
     * domain is set to AF_INET which is for IPv4
     * type is set to SOCK_STREAM which is for TCP
     * protocol is set to 0 to choose proper protocol for given type
     *
     * -1 is returned in case of an error and errno is set accordingly
     *
     */
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        die("socket()");
        return FAILED;
    }
    print("fd", fd);

    /**
     * set socket configuration
     *
     * int setsockopt(int socket, int level, int option_name,
     *      const void *option_value, socklen_t option_len);
     *
     * level is set to SOL_SOCKET to manipulate the socket layer
     *
     * SO_REUSEADDR is set to the value of 1(yes) to reuse the address
     * and get rid of the "Address already in use" error message
     *
     */
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    int portNumber = 1234;
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(portNumber);
    addr.sin_addr.s_addr = ntohl(0); // wildcard address 0.0.0.0

    /**
     * Associate the socket with a port on local machine
     *
     * int bind(int sockfd, struct sockaddr *my_addr, int addrlen);
     *
     * my_addr is a pointer to the struct sockaddr_in which contains
     * the details about port and IP address.
     *
     * -1 is returned in case of an error and errno is set accordingly
     *
     */
    int rv = bind(fd, (const sockaddr *)&addr, sizeof(addr));
    print("rv bind", rv);
    if (rv)
    {
        die("bind()");
        return FAILED;
    }

    /**
     * listen on the address for connections
     *
     * int listen(int sockfd, int backlog);
     *
     * sockfd is the socket file descriptor
     * backlog is the number of connections allowed in the incoming queue
     *      its set to SOMAXCONN which is defined as 128 on linux
     *
     * -1 is returned in case of an error and errno is set accordingly
     *
     */
    rv = listen(fd, SOMAXCONN);
    print("rv listen", rv);
    if (rv)
    {
        die("listen()");
        return FAILED;
    }

    return fd;
}

/**
 * @brief sets the socket to non-blocking mode
 *
 * get the current flags set on the sockfd,
 * sets the non-blocking flag on the returned flags,
 * and sets this new flag to the socket *
 *
 * @param fd: file-descriptor of the listening connection
 *
 */
int Server::set_non_blocking(int fd)
{
    errno = 0;

    /**
     * int fcntl(int sockfd, int cmd, long arg);
     *
     * sockfd is the socket you want to operate on
     * cmd is the command you want to do
     *      F_GETFL returns the current flags that are set for sockfd
     *      F_SETFL sets the flags exactly what you pass in third param
     *
     */
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno)
    {
        die("fcntl error");
        return FAILED;
    }

    // set the flag for non-blocking
    flags |= O_NONBLOCK;
    errno = 0;

    (void)fcntl(fd, F_SETFL, flags);

    if (errno)
    {
        die("fcntl error");
        return FAILED;
    }

    return SUCCESS;
}

void Server::run_server(int fd)
{
    while (true)
    {
        poll_args.clear();

        // the listening fd is put in the first position
        struct pollfd pfd = {fd, POLLIN, 0};
        poll_args.push_back(pfd);

        for (Conn *conn : fd2conn)
        {
            if (conn == NULL)
            {
                continue;
            }

            struct pollfd pfd = {};
            pfd.fd = conn->fd;
            pfd.events = (conn->state == STATE_REQ) ? POLLIN : POLLOUT;
            print("POLLIN, POLLOUT, POLLERR", POLLIN, POLLOUT, POLLERR);
            print("pfd events without POLLERR", pfd.events);
            pfd.events = pfd.events | POLLERR;
            print("pfd events", pfd.events);
            poll_args.push_back(pfd);
        }

        /**
         * poll for active fds
         *
         * int poll(struct pollfd fds[], nfds_t nfds, int timeout);
         *
         * fds is the array of sockets we want to moniter
         * nfds is the size of the array
         * timeout is timeout in milliseconds, pass negative timeout to wait forever
         *
         * returns the number of elements in the array that have an event occur
         *
         */
        int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), 1000);
        print("No of conn with events", rv);
        if (rv < 0)
        {
            die("poll");
        }

        // process active connections
        for (size_t i = 1; i < poll_args.size(); i++)
        {
            print("index", i, "revents", poll_args[i].revents);
            if (poll_args[i].revents)
            {
                Conn *conn = fd2conn[poll_args[i].fd];
                conn->connection_io();

                if (conn->state == STATE_END)
                {
                    // client closed normally, or something bad happened.
                    // destroy this connection
                    print("STATE_END reached, freeing conn", conn->fd);
                    fd2conn[conn->fd] = NULL;
                    (void)close(conn->fd);
                    free(conn);
                }
            }
        }

        // try to accept a new connection if the listening fd is active
        if (poll_args[0].revents)
        {
            accept_new_conn(fd);
        }
    }
}
