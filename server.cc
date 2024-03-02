// proj
#include <iostream>
#include "connection.h"

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

