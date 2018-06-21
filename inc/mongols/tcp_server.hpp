#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP


#include <netinet/in.h>   
#include <string>


#include "epoll.hpp"

namespace mongols {

    class tcp_server {
    public:
        tcp_server() = delete;
        tcp_server(const std::string& host, int port, int max_event_size = 64, int timeout = 60000,size_t buffer_size=1024);
        virtual~tcp_server();


    public:
        void run(const std::function<std::string(const std::string&) >&);
    private:
        mongols::epoll epoll;
        std::string host;
        int port, listenfd;
        struct sockaddr_in serveraddr;
        size_t buffer_size;
    private:
        void setnonblocking(int fd);
    };
}


#endif /* TCP_SERVER_HPP */

