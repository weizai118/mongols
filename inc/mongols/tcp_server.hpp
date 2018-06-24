#ifndef TCP_SERVER_HPP
#define TCP_SERVER_HPP


#include <netinet/in.h>   
#include <string>
#include <utility>
#include <unordered_map>
#include <mutex>


#include "thead_pool.hpp"
#include "epoll.hpp"

#define CLOSE_CONNECTION true
#define KEEPALIVE_CONNECTION false

namespace mongols {

    class tcp_server {
    public:
        tcp_server() = delete;
        tcp_server(const std::string& host, int port, int timeout = 5000, size_t buffer_size = 1024, size_t thread_size = 0, int max_event_size = 64);
        virtual~tcp_server()=default;


    public:
        void run(const std::function<std::pair<std::string, bool>(const std::string&, bool&) >&);
    private:
        mongols::epoll epoll;
        std::string host;
        int port, listenfd, timeout;
        struct sockaddr_in serveraddr;
        size_t buffer_size;
        std::unordered_map<int, std::pair<size_t, size_t>> clients;
        std::mutex main_mtx;
    private:
        mongols::thread_pool work_pool;
        static bool done;
        static void signal_normal_cb(int sig);

    private:
        void setnonblocking(int fd);
        void add_client(int);
        void del_client(int);
        void send_to_all_client(int, const std::string&);
        bool work(int, const std::function<std::pair<std::string, bool>(const std::string&, bool&) >&);
        void main_loop(struct epoll_event *, const std::function<std::pair<std::string, bool>(const std::string&, bool&) >&);
    };
}


#endif /* TCP_SERVER_HPP */

