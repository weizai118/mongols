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

#include "tcp_threading_server.hpp"


namespace mongols {

    tcp_threading_server::tcp_threading_server(const std::string& host, int port
            , int timeout, size_t buffer_size
            , size_t thread_size, int max_event_size)
    : tcp_server(host, port
    , timeout, buffer_size
    , max_event_size)
    , main_mtx(), work_pool(thread_size == 0 ? std::thread::hardware_concurrency() : thread_size) {

    }

    tcp_threading_server::~tcp_threading_server() {
        struct timespec thread_exit_timeout;
        thread_exit_timeout.tv_sec = 0;
        thread_exit_timeout.tv_nsec = 200;
        handler_function g = [](const std::string&
                , bool&
                , std::pair<size_t, size_t>&
                , filter_handler_function&) {
            return std::make_pair("", CLOSE_CONNECTION);
        };
        auto thread_exit_fun = std::bind(&tcp_threading_server::work, this, -1, g);
        for (size_t i = 0; i<this->work_pool.size(); ++i) {
            this->work_pool.submit(thread_exit_fun);
            std::this_thread::yield();
            if (nanosleep(&thread_exit_timeout, 0) < 0) {
                --i;
            }
        }
    }

    void tcp_threading_server::process(int fd, const handler_function& g) {
        this->work_pool.submit(std::bind(&tcp_threading_server::work, this, fd, g));
        std::this_thread::yield();
    }

    void tcp_threading_server::add_client(int fd) {
        std::lock_guard<std::mutex> lk(this->main_mtx);
        this->clients.insert(std::move(std::make_pair(fd, std::move(std::make_pair(0, 0)))));
        std::this_thread::yield();
    }

    void tcp_threading_server::del_client(int fd) {
        std::lock_guard<std::mutex> lk(this->main_mtx);
        this->clients.erase(fd);
        std::this_thread::yield();
    }

    bool tcp_threading_server::send_to_all_client(int fd, const std::string& str, const filter_handler_function& h) {
        if (fd > 0) {
            std::lock_guard<std::mutex> lk(this->main_mtx);
            for (auto& i : this->clients) {
                if (i.first != fd && h(i.second)) {
                    send(i.first, str.c_str(), str.size(), 0);
                    std::this_thread::yield();
                }
            }

        }
        return fd > 0 ? false : true;
    }

    bool tcp_threading_server::work(int fd, const handler_function& g) {
        if (fd > 0) {
            std::string input, temp_input;
            size_t ret;
            do {
                char buffer[this->buffer_size];
                ret = recv(fd, buffer, this->buffer_size, MSG_WAITALL);
                if (ret == -1) {
                    if (errno == EAGAIN) {
                        return false;
                    }
                } else if (ret > 0) {
                    try {
                        temp_input.assign(buffer, ret);
                    } catch (const std::length_error& e) {
                        temp_input = e.what();
                        break;
                    }
                } else {
                    break;
                }
            } while (!this->check_finished(temp_input, input));

            if (ret > 0) {
                filter_handler_function send_to_other_filter = [](const std::pair<size_t, size_t>&) {
                    return true;
                };

                std::lock_guard<std::mutex> lk(this->main_mtx);
                bool send_to_all = false;
                std::pair<size_t, size_t>& g_u_id = this->clients[fd];
                std::pair < std::string, bool> output = std::move(g(input, send_to_all, g_u_id, send_to_other_filter));
                size_t n = send(fd, output.first.c_str(), output.first.size(), 0);

                if (n >= 0) {
                    if (send_to_all) {
                        this->work_pool.submit(std::bind(&tcp_threading_server::send_to_all_client, this, fd, output.first, send_to_other_filter));
                        std::this_thread::yield();
                    }
                }
                if (n < 0 || output.second) {
                    goto ev_error;
                }

                std::this_thread::yield();



            } else {

ev_error:
                close(fd);
                this->del_client(fd);

            }
        }
        return fd > 0 ? false : true;
    }

    bool tcp_threading_server::check_finished(const std::string& temp_input, std::string& input) {
        input.append(temp_input);
        return true;
    }



}