#ifndef WS_SERVER_HPP
#define WS_SERVER_HPP

#include "tcp_server.hpp"



namespace mongols {


    class ws_server {
    public:
        ws_server() = delete;
        virtual~ws_server() = default;
        ws_server(const std::string& host, int port, int timeout = 5000
                , size_t buffer_size = 1024, size_t thread_size = 0, int max_event_size = 64);

    public:
        void run();
    private:
        virtual std::pair < std::string, bool> work(const std::string&,bool&);

    private:
        mongols::tcp_server server;

    };
}

#endif /* WS_SERVER_HPP */

