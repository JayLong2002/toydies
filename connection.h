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
#include <string>
#include <vector>
// proj
#include <iostream>
#include "command.h"
#include "kv.h"

#ifndef NDEBUG
#   define toydies_assert(condition, message) \
    do { \
        if (! (condition)) { \
            std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
                      << " line " << __LINE__ << ": " << message << std::endl; \
            std::terminate(); \
        } \
    } while (false)
#else
#   define ASSERT(condition, message) do { } while (false)
#endif

const size_t k_max_msg = 4096; //4k

static void msg(const char *msg) {
    fprintf(stderr, "%s\n", msg);
} 

static int32_t read_full(int fd, char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = recv(fd, buf, n,0);
        if (rv <= 0) {
            return -1;  // error, or unexpected EOF
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t write_all(int fd, const char *buf, size_t n) {
    while (n > 0) {
        ssize_t rv = send(fd, buf, n,0);
        if (rv <= 0) {
            return -1;  // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}
// no-blocking io impl
// readindg means the server need to read from the connection fd
enum CONN_STATE {
    READING,
    SENDING,
    END,
};

struct connection{
    int fd = -1;
    CONN_STATE state;
    // read  buffer
    size_t rbuf_offset = 0;
    u_int8_t rbuf[k_max_msg + 4];
    // writing 
    size_t wbuf_offset = 0;
    size_t wbuf_send = 0 ;
    u_int8_t wbuf[4 + k_max_msg];
};

static void fd2nb(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);

    toydies_assert(errno == 0,"fcntl error");

    //bit map
    flags |= O_NONBLOCK;

    errno = 0;
    (void)fcntl(fd, F_SETFL, flags);

    toydies_assert(errno == 0,"fcntl error");
}

static bool try_fill_buffer(connection *conn);

static void server_reading(connection *conn)
{
    while (try_fill_buffer(conn)) {};
}

static void server_sending(connection *conn);

// the state of request 
enum REQUEST_STATE{
    RES_OK = 0,
    RES_ERR = 1,
    RES_NX = 2,
};

//------KV-----part

// handle one request one time ,and put the msg into out
static void do_request(std::vector<std::string> &cmd,std::string &out)
{
    auto cmd_type = parser_cmd(cmd);

    //request handler
    if (cmd_type == GET) {
        do_get(cmd,out);
    } else if (cmd_type == SET) {
        do_set(cmd,out);
    } else if (cmd_type == DEL) {
        do_del(cmd,out);
    }else if(cmd_type == SEARCH){
        do_search(cmd,out);
    }else {
        out_err(out, 2, "Unknown cmd");
    }
}

// maybe more than one request one buffer
static bool try_handle_request(connection *conn)
{
    // no enough data from rbuf,no length
    if(conn->rbuf_offset < 4) return false;

    uint32_t len = 0;
    memcpy(&len,conn->rbuf,4);
    
    if(len > k_max_msg){
        msg("parser request error : msg too long");
        conn->state = END;
        return false;
    }

    // wait another call,data lack
    if(4 + len > conn->rbuf_offset ) return false;

    //parser the request
    std::vector<std::string> cmd;
    if (0 != parse_req(&conn->rbuf[4], len, cmd)) {
        msg("bad req");
        conn->state = END;
        return false;
    }

    std::string out;
    do_request(cmd, out);

    // back msg : wlen(4)--out(type-length-value)
    uint32_t wlen = (uint32_t)out.size();
    memcpy(&conn->wbuf[0], &wlen, 4);
    memcpy(&conn->wbuf[4], out.data(), out.size());
    
    conn->wbuf_offset = 4 + wlen;

    size_t remain = conn->rbuf_offset - 4 - len;

    if (remain) {
        memmove(conn->rbuf, &conn->rbuf[4 + len], remain);
    }
    conn->rbuf_offset = remain;
    conn->state = SENDING;
    server_sending(conn);

    bool should_reading = (conn->state == READING);

    return should_reading;
}


/**
 * @brief try to fill the buffer
*/
static bool try_fill_buffer(connection *conn)
{
    //toydies_assert(conn->rbuf_offset < sizeof(conn->rbuf),"read buffer capacity error");
    ssize_t rv = 0;
    do
    {
        size_t capacity =  sizeof(conn->rbuf) - conn->rbuf_offset;
        rv = read(conn->fd,&conn->rbuf[conn->rbuf_offset],capacity);
    } while (rv < 0 && errno == EINTR);

    if (rv < 0 && errno == EAGAIN) {
       
        return false;
    }

    if (rv < 0) {
        msg("read() error!");
        conn->state = END;
        return false;
    }

    if(rv == 0){
        msg("EOF");
        conn->state = END;
        return false;
    }

    conn->rbuf_offset += (size_t)rv;
    toydies_assert(conn->rbuf_offset <= sizeof(conn->rbuf),"read buffer capacity error");
    // read the all things, should handle
    while(try_handle_request(conn)) {};

    return (conn->state == READING);

}

static bool try_flush_buffer(connection *conn)
{
    ssize_t rv = 0;
    do
    {
        size_t remain = conn->wbuf_offset - conn->wbuf_send;
        rv = write(conn->fd,&conn->wbuf[conn->wbuf_send],remain);
    } while (rv < 0 && errno == EINTR);
     if (rv < 0 && errno == EAGAIN) {
        // got EAGAIN, stop.
        return false;
    }
    if (rv < 0) {
        msg("write() error");
        conn->state = END;
        return false;
    }
    conn->wbuf_send += (size_t)rv;
    toydies_assert(conn->wbuf_send <= conn->wbuf_offset,"error in write()");

    if(conn->wbuf_send == conn->wbuf_offset)
    {
        conn->state = READING;
        conn->wbuf_send = 0;
        conn->wbuf_offset = 0;
        return false;
    }
    return true;
}

static void server_sending(connection *conn)
{
    while(try_flush_buffer(conn)) {};
}

static void handle(connection *conn)
{
    if(conn->state == READING){
        (void)server_reading(conn);
    }else if(conn->state == SENDING)
    {
        (void)server_sending(conn);
    }else{
        toydies_assert(false,"handle() error");
    }
}

static void conn_put(std::vector<connection *> &fd2conn, struct connection *conn) {
    if (fd2conn.size() <= (size_t)conn->fd) {
        fd2conn.resize(conn->fd + 1);
    }
    fd2conn[conn->fd] = conn;
}

/**
 * @brief the listen fd poll, server init the connection
 * 
*/
static int accept_new_conn(std::vector<connection *>  &fd2conn, int listen_fd)
{
    //accept
    struct sockaddr_in client_adrr = {};
    socklen_t socklen = sizeof(client_adrr);
    int connfd = accept(listen_fd,(struct sockaddr *)&client_adrr,&socklen);
    toydies_assert(connfd >=0 , "accept() error");
    fd2nb(connfd);
    auto conn = (struct connection *)malloc(sizeof(struct connection));
    if(!conn){
        close(connfd);
        return -1;
    }
    conn->fd = connfd;
    conn->state = READING;
    conn->rbuf_offset = 0;
    conn->wbuf_offset = 0;
    conn->wbuf_send = 0;
    conn_put(fd2conn, conn);
    return 0;
}


