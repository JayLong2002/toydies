#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <cassert>
#include <iostream>
#include <fcntl.h>
#include <unordered_map>
#include <vector>
#include <poll.h>
#include <map>

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

const size_t k_max_args = 1024;


/*
    the protocal msg
    +------+-----+------+-----+------+-----+-----+------+
    | nstr | len | str1 | len | str2 | ... | len | strn |
    +------+-----+------+-----+------+-----+-----+------+
*/
static int32_t parse_req(
    const uint8_t *data, size_t len, std::vector<std::string> &out)
{
    if (len < 4) {
        return -1;
    }
    uint32_t n = 0;
    memcpy(&n, &data[0], 4);
    if (n > k_max_args) {
        return -1;
    }

    size_t pos = 4;
    while (n--) {
        if (pos + 4 > len) {
            return -1;
        }
        uint32_t sz = 0;
        memcpy(&sz, &data[pos], 4);
        if (pos + 4 + sz > len) {
            return -1;
        }
        out.push_back(std::string((char *)&data[pos + 4], sz));
        pos += 4 + sz;
    }

    if (pos != len) {
        return -1; 
    }
    return 0;
}

static bool cmd_is(const std::string &word, const char *cmd) {
    return 0 == strcasecmp(word.c_str(), cmd);
}

enum CMD{
    GET,
    SET,
    DEL,
    UNKNOWN
};

static CMD parser_cmd(const std::vector<std::string> &cmd)
{
    if (cmd.size() == 2 && cmd_is(cmd[0], "get")) {
        return GET;
    } else if (cmd.size() == 3 && cmd_is(cmd[0], "set")) {
        return SET;
    } else if (cmd.size() == 2 && cmd_is(cmd[0], "del")) {
        return DEL;
    } else {
        return UNKNOWN;
    }
}

// TODO:finish kv storgae
// The data structure for the key space. This is just a placeholder
// until we implement a hashtable in the next chapter.
static std::map<std::string, std::string> g_map;

static uint32_t do_get(const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen)
{
    auto key = cmd[1];

    if(!g_map.count(key))
    {
        return RES_NX;
    }
    
    auto &val = g_map[key];
    
    toydies_assert(val.size() <= k_max_msg,"value too long");

    memcpy(res,val.data(),val.size());

    *reslen = (uint32_t)val.size();

    return RES_OK;
}

static uint32_t do_set(
    const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen)
{
    (void)res;
    (void)reslen;
    g_map[cmd[1]] = cmd[2];
    return RES_OK;
}

static uint32_t do_del(
    const std::vector<std::string> &cmd, uint8_t *res, uint32_t *reslen)
{
    (void)res;
    (void)reslen;
    g_map.erase(cmd[1]);
    return RES_OK;
}

/**
 * @brief do request
 * @param req : the buffer of request
 * @param reqlen : the len of request 
 * @param resmsg : the state back from doing request
 * @param res : the return msg to stay
 * @param reslen : this param should fill by doing function, told the returen msg length
 * 
*/
static int32_t do_request(
    const uint8_t *req,uint32_t reqlen,
    uint32_t *rescode,uint8_t *res,uint32_t *reslen
)
{
    std::vector<std::string> cmd;
    int32_t rv = parse_req(req,reqlen,cmd);
    if(rv){
        msg("error in req!");
        return -1;
    }

    auto cmd_type = parser_cmd(cmd);

    //request handler
    if (cmd_type == GET) {
        *rescode = do_get(cmd, res, reslen);
    } else if (cmd_type == SET) {
        *rescode = do_set(cmd, res, reslen);
    } else if (cmd_type == DEL) {
        *rescode = do_del(cmd, res, reslen);
    } else {
        // cmd is not recognized
        *rescode = RES_ERR;
        const char *msg = "Unknown cmd";
        strcpy((char *)res, msg);
        *reslen = strlen(msg);
        return 0;
    }
    return 0;
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

    uint32_t rescode = 0;
    uint32_t wlen = 0 ;
    
    int32_t rv = do_request(
        &conn->rbuf[4], len,
        &rescode, &conn->wbuf[8], &wlen
    );

    if (rv) {
        conn->state = END;
        return false;
    }

    // return msg : len(not include len self) + rescode + resbody
    wlen += 4;
    // generating echoing response
    memcpy(&conn->wbuf[0], &wlen, 4);
    
    memcpy(&conn->wbuf[4], &rescode, 4);
    
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

int main(){

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    toydies_assert(listen_fd >= 0 , "bad socket");
    
    int val = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));
    
    // bind
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(1234);
    addr.sin_addr.s_addr = ntohl(0); 
    
    //确认你的两个数字跟四个数字都是 Network Byte Order
    //指向 struct sockaddr_in 的指针可以转型（cast）为指向 struct sockaddr 的指针
    int rv = bind(listen_fd,(const sockaddr *)&addr,sizeof(addr));
    
    toydies_assert(rv==0,"error in bind");


    // queuen length 4096
    rv = listen(listen_fd,SOMAXCONN);

    toydies_assert(rv == 0 ,"error in listen");

    // event looping impl

    // set the listen file discriptor to non-blocking
    fd2nb(listen_fd);
    
    std::vector<connection *> fd2conn;
    
    // all the events will poll
    std::vector<struct pollfd> events;

    while(true){
        events.clear();
        struct pollfd pfd = {listen_fd,POLLIN,0};
        events.push_back(pfd);

        for(auto &conn : fd2conn){
            if(!conn){
                continue;
            }
        struct pollfd pfd = {};
        pfd.fd = conn->fd;
        pfd.events = (conn->state == READING) ? POLLIN : POLLOUT;
        pfd.events = pfd.events | POLLERR; 
        events.push_back(pfd);
        }
        

        // polling

        int rv = poll(events.data(),(nfds_t) events.size(),1000);

        toydies_assert(rv >=0, "error in poll");
          
        // handle 


        for (size_t i = 1; i < events.size(); i++)
        {
            if(events[i].revents){

                    //handle other event
                    auto conn = fd2conn[events[i].fd];
                    handle(conn);

                    if(conn->state == END)
                    {
                        //refresh the resource the connection use
                        fd2conn[conn->fd] = NULL;
                        (void)close(conn->fd);
                        free(conn);
                    }
                }
         }
        if (events[0].revents) {
            (void)accept_new_conn(fd2conn, listen_fd);
        }
    }
        
}

