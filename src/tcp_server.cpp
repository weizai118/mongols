#include <fcntl.h>          
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/signal.h>

#include <cstring>         
#include <cstdlib> 


#include <string>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <functional>


#include "tcp_server.hpp"



namespace mongols {

    bool tcp_server::done = false;

    void tcp_server::signal_normal_cb(int sig) {
        switch (sig) {
            case SIGTERM:
            case SIGHUP:
            case SIGQUIT:
            case SIGINT:
                tcp_server::done = true;
                break;
        }
    }

    tcp_server::tcp_server(const std::string& host
            , int port
            , int timeout
            , size_t buffer_size
            , size_t thread_size
            , int max_event_size) :
    epoll(max_event_size, -1)
    , host(host), port(port), listenfd(0), timeout(timeout), serveraddr()
    , buffer_size(buffer_size), clients(), main_mtx()
    , work_pool(thread_size) {

    }

    void tcp_server::run(const std::function<std::pair<std::string, bool>(const std::string&, bool&, std::pair<size_t, size_t>&) >& g
            , const std::function<bool(const std::pair<size_t, size_t>&)>& h) {

        this->listenfd = socket(AF_INET, SOCK_STREAM, 0);

        int on = 1;
        setsockopt(this->listenfd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof (on));

        struct timeval send_timeout, recv_timeout;
        send_timeout.tv_sec = this->timeout;
        send_timeout.tv_usec = 0;
        setsockopt(this->listenfd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof (send_timeout));

        recv_timeout.tv_sec = this->timeout;
        recv_timeout.tv_usec = 0;
        setsockopt(this->listenfd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof (recv_timeout));



        memset(&this->serveraddr, '\0', sizeof (this->serveraddr));
        this->serveraddr.sin_family = AF_INET;
        inet_aton(this->host.c_str(), &serveraddr.sin_addr);
        this->serveraddr.sin_port = htons(this->port);
        bind(this->listenfd, (struct sockaddr*) & this->serveraddr, sizeof (this->serveraddr));

        this->setnonblocking(this->listenfd);

        if (!this->epoll.is_ready()) {
            perror("epoll error");
            return;
        }
        this->epoll.add(this->listenfd, EPOLLIN | EPOLLET);


        listen(this->listenfd, 10);

        signal(SIGHUP, tcp_server::signal_normal_cb);
        signal(SIGTERM, tcp_server::signal_normal_cb);
        signal(SIGINT, tcp_server::signal_normal_cb);
        signal(SIGQUIT, tcp_server::signal_normal_cb);

        auto main_fun = std::bind(&tcp_server::main_loop, this, std::placeholders::_1, std::cref(g), std::cref(h));

        while (!tcp_server::done) {
            this->epoll.loop(main_fun);
        }

        if (!this->work_pool.empty()) {
            struct timespec thread_exit_timeout;
            thread_exit_timeout.tv_sec = 0;
            thread_exit_timeout.tv_nsec = 200;
            auto thread_exit_fun = std::bind(&tcp_server::work, this, -1, std::cref(g),std::cref(h));
            for (size_t i = 0; i<this->work_pool.size(); ++i) {
                this->work_pool.submit(thread_exit_fun);
                std::this_thread::yield();
                if (nanosleep(&thread_exit_timeout, 0) < 0) {
                    --i;
                }
            }
        }

    }

    void tcp_server::setnonblocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    void tcp_server::add_client(int fd) {
        if (this->work_pool.empty()) {
            this->clients.insert(std::move(std::make_pair(fd, std::move(std::make_pair(0, 0)))));
        } else {
            std::lock_guard<std::mutex> lk(this->main_mtx);
            this->clients.insert(std::move(std::make_pair(fd, std::move(std::make_pair(0, 0)))));
            std::this_thread::yield();
        }
    }

    void tcp_server::del_client(int fd) {
        if (this->work_pool.empty()) {
            this->clients.erase(fd);
        } else {
            std::lock_guard<std::mutex> lk(this->main_mtx);
            this->clients.erase(fd);
            std::this_thread::yield();
        }
    }

    bool tcp_server::send_to_all_client(int fd, const std::string& str, const std::function<bool(const std::pair<size_t, size_t>&)>& h) {
        if (fd > 0) {
            if (this->work_pool.empty()) {
                for (auto& i : this->clients) {
                    if (i.first != fd && h(i.second)) {
                        send(i.first, str.c_str(), str.size(), 0);
                    }
                }

            } else {
                std::lock_guard<std::mutex> lk(this->main_mtx);
                for (auto& i : this->clients) {
                    if (i.first != fd && h(i.second)) {
                        send(i.first, str.c_str(), str.size(), 0);
                        std::this_thread::yield();
                    }
                }

            }
        }
        return fd > 0 ? false : true;
    }

    bool tcp_server::work(int fd, const std::function<std::pair<std::string, bool>(const std::string&, bool&, std::pair<size_t, size_t>&) >& g
            , const std::function<bool(const std::pair<size_t, size_t>&)>& h) {
        if (fd > 0) {
            char buffer[this->buffer_size] = {0};
            ssize_t ret = recv(fd, buffer, this->buffer_size, MSG_WAITALL);
            if (ret >= 0) {
                std::string input = std::move(std::string(buffer, ret));
                if (this->work_pool.empty()) {
                    bool send_to_all = false;
                    std::pair<size_t, size_t>& g_u_id = this->clients[fd];
                    std::pair < std::string, bool> output = std::move(g(input, send_to_all, g_u_id));
                    if (send(fd, output.first.c_str(), output.first.size(), 0) < 0 || output.second) {
                        goto ev_error;
                    } else if (send_to_all) {
                        this->work_pool.submit(std::bind(&tcp_server::send_to_all_client, this, fd, std::cref(output.first), std::cref(h)));
                        std::this_thread::yield();
                    }
                } else {
                    std::lock_guard<std::mutex> lk(this->main_mtx);
                    bool send_to_all = false;
                    std::pair<size_t, size_t>& g_u_id = this->clients[fd];
                    std::pair < std::string, bool> output = std::move(g(input, send_to_all, g_u_id));
                    if (send(fd, output.first.c_str(), output.first.size(), 0) < 0 || output.second) {
                        goto ev_error;
                    } else if (send_to_all) {
                        this->work_pool.submit(std::bind(&tcp_server::send_to_all_client, this, fd, (output.first), std::cref(h)));
                        std::this_thread::yield();
                    }
                    std::this_thread::yield();
                }
            } else {
ev_error:
                close(fd);
                this->del_client(fd);

            }
        }
        return fd > 0 ? false : true;
    }

    void tcp_server::main_loop(struct epoll_event * event
            , const std::function<std::pair<std::string, bool>(const std::string&, bool&, std::pair<size_t, size_t>&)>& g
            , const std::function<bool(const std::pair<size_t, size_t>&)>& h) {
        if ((event->events & EPOLLERR) ||
                (event->events & EPOLLHUP) ||
                (!(event->events & EPOLLIN))) {
            close(event->data.fd);
        } else if (event->events & EPOLLRDHUP) {
            close(event->data.fd);
        } else if (event->data.fd == this->listenfd) {
            while (!tcp_server::done) {
                struct sockaddr_in clientaddr;
                socklen_t clilen;
                int connfd = accept(listenfd, (struct sockaddr*) &clientaddr, &clilen);
                if (connfd > 0) {
                    setnonblocking(connfd);
                    this->epoll.add(connfd, EPOLLIN | EPOLLRDHUP | EPOLLET);

                    this->add_client(connfd);
                } else {
                    break;
                }
            }
        } else {
            if (this->work_pool.empty()) {
                work(event->data.fd, g, h);
            } else {
                this->work_pool.submit(std::bind(&tcp_server::work, this, int(event->data.fd), g, h));
                std::this_thread::yield();
            }
        }
    }


}