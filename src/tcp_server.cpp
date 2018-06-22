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
#include <thread>
#include <vector>


#include "tcp_server.hpp"



namespace mongols {

    tcp_server::tcp_server(const std::string& host
            , int port
            , int timeout
            , size_t buffer_size
            , size_t thread_size
            , int max_event_size) :
    epoll(max_event_size, timeout)
    , host(host), port(port), listenfd(0), serveraddr()
    , buffer_size(buffer_size), thread_size(thread_size) {

    }

    tcp_server::~tcp_server() {
        for (;;) {
            int v;
            if (this->q.try_pop(v)) {
                this->epoll.del(v);
                close(v);
            } else {
                break;
            }
        }
        if (this->listenfd > 0) {
            close(this->listenfd);
        }
    }

    void tcp_server::run(const std::function<std::pair<std::string, bool>(const std::string&) >& g) {

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

        std::function<void(int) > w = [&](int fd) {
            char buffer[this->buffer_size] = {0};
            ssize_t ret = recv(fd, buffer, this->buffer_size, MSG_DONTWAIT);
            if (ret >= 0) {
                std::string input = std::move(std::string(buffer, ret));
                std::pair < std::string, bool> output = std::move(g(input));
                if (send(fd, output.first.c_str(), output.first.size(), MSG_DONTWAIT) < 0 || output.second) {
                    goto ev_error;
                }
            } else {
ev_error:
                this->epoll.del(fd);
                close(fd);
            }
        };

        std::vector<std::thread> th;
        for (size_t i = 0; i < this->thread_size; ++i) {
            th.push_back(std::move(std::thread(
                    [&]() {
                        while (1) {
                            int fd;
                            this->q.wait_and_pop(fd);
                            w(fd);
                            std::this_thread::yield();
                        }
                    }
            )));
        }

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
                        this->epoll.add(connfd, EPOLLIN | EPOLLRDHUP | EPOLLET);

                    } else {
                        break;
                    }
                }
            } else {
                if (this->thread_size > 0) {
                    this->q.push(event->data.fd);
                } else {
                    w(event->data.fd);
                }
            }
        };




        while (1) {
            int ret = this->epoll.wait();
            this->epoll.loop(ret, f);
        }


        for (auto& i : th) {
            i.join();
        }


    }

    void tcp_server::setnonblocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }



}