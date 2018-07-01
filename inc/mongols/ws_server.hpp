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
        void run(const std::function<std::string(const std::string&, bool&, bool&,std::pair<size_t, size_t>&)>&
        ,const std::function<bool(const std::pair<size_t, size_t>&)>&);
    private:
        virtual std::pair < std::string, bool> work(const std::function<std::string(const std::string&, bool&, bool&,std::pair<size_t, size_t>&)>&
        ,const std::string&, bool&, std::pair<size_t, size_t>&);

    private:
        mongols::tcp_server server;

    };
}

#endif /* WS_SERVER_HPP */

