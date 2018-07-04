#ifndef TCP_THREADING_SERVER_HPP
#define TCP_THREADING_SERVER_HPP

#include "tcp_server.hpp"


namespace mongols {

    class tcp_threading_server : public tcp_server {
    public:
        tcp_threading_server() = delete;

        tcp_threading_server(const std::string& host, int port
                , int timeout
                , size_t buffer_size
                , size_t thread_size
                , int max_event_size);
        virtual~tcp_threading_server();
    protected:
        void add_client(int) override;
        void del_client(int) override;
        void process(int, const handler_function&) override;
        bool send_to_all_client(int, const std::string&, const filter_handler_function&) override;
        bool work(int, const handler_function&) override;

    private:

        std::mutex main_mtx;
        mongols::thread_pool work_pool;





    };
}

#endif /* TCP_THREADING_SERVER_HPP */

