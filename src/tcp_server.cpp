#include <fcntl.h>          
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <cstring>         
#include <cstdlib> 


#include <string>
#include <iostream>


#include "tcp_server.hpp"



namespace mongols {

    tcp_server::tcp_server(const std::string& host, int port, int max_event_size, int timeout, size_t buffer_size) :
    epoll(max_event_size, timeout)
    , host(host), port(port), listenfd(0), serveraddr(), buffer_size(buffer_size) {

    }

    tcp_server::~tcp_server() {
        if (this->listenfd > 0) {
            close(this->listenfd);
        }
    }

    void tcp_server::run(const std::function<std::string(const std::string&) >& g) {

        int connfd;
        struct timeval timeout;


        this->listenfd = socket(AF_INET, SOCK_STREAM, 0);

        int on = 1;
        setsockopt(this->listenfd, SOL_SOCKET, SO_REUSEPORT, (const char *) &on, sizeof (on));
        timeout.tv_sec = this->epoll.expires() < 0 ? 5 : this->epoll.expires() / 1000;
        timeout.tv_usec = 0;
        setsockopt(this->listenfd, SOL_SOCKET, SO_SNDTIMEO, (const char *) &timeout, sizeof (timeout));
        setsockopt(this->listenfd, SOL_SOCKET, SO_RCVTIMEO, (const char *) &timeout, sizeof (timeout));



        bzero(&this->serveraddr, sizeof (this->serveraddr));
        this->serveraddr.sin_family = AF_INET;
        inet_aton(this->host.c_str(), &serveraddr.sin_addr);
        this->serveraddr.sin_port = htons(this->port);
        bind(this->listenfd, (struct sockaddr*) & this->serveraddr, sizeof (this->serveraddr));

        this->setnonblocking(this->listenfd);

        if (!this->epoll.is_ready()) {
            std::cout << "epoll error" << std::endl;
            return;
        }
        this->epoll.add(this->listenfd, EPOLLIN | EPOLLET);


        listen(this->listenfd, 10);



        std::function<void(struct epoll_event*) > f = [&](struct epoll_event * event) {
            if ((event->events & EPOLLERR) ||
                    (event->events & EPOLLHUP) ||
                    (!(event->events & EPOLLIN))) {
                close(event->data.fd);
            } else if (event->events & EPOLLRDHUP) {
                close(event->data.fd);
            } else if (event->data.fd == this->listenfd) {
                while (1) {
                    struct sockaddr_in clientaddr;
                    socklen_t clilen;
                    connfd = accept(listenfd, (struct sockaddr*) &clientaddr, &clilen);
                    if (connfd > 0) {
                        this->epoll.add(connfd, EPOLLIN | EPOLLRDHUP | EPOLLET);
                        setnonblocking(connfd);
                    } else {
                        break;
                    }
                }
            } else {
                char buffer[this->buffer_size] = {0};
                ssize_t ret = recv(event->data.fd, buffer, buffer_size, 0);
                if (ret > 0) {
                    std::string input = std::move(std::string(buffer, ret))
                            , output = std::move(g(input));
                    if (send(event->data.fd, output.c_str(), output.size(), 0) < 0) {
                        goto ev_error;
                    }
                }
ev_error:
                this->epoll.del(event->data.fd);
                close(event->data.fd);
            }
        };




        while (1) {
            int ret = this->epoll.wait();
            this->epoll.loop(ret, f);
        }




    }

    void tcp_server::setnonblocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }



}