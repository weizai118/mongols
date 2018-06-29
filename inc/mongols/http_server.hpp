#ifndef HTTP_SERVER_HPP
#define HTTP_SERVER_HPP



#include "tcp_server.hpp"
#include "servlet.hpp"
#include "lib/http_parser.h"


namespace mongols {

    class http_request_parser {
    public:
        http_request_parser() = delete;
        http_request_parser(mongols::request& req);
        virtual~http_request_parser() = default;

        bool parse(const std::string& str);


        const std::string& get_body()const;
        std::string& get_body();

    private:

        struct tmp_ {
            std::pair<std::string, std::string> pair;
            http_request_parser* parser;
        };
    private:
        tmp_ tmp;
        struct http_parser parser;
        struct http_parser_settings settings;
        mongols::request& req;
        std::string body;


    };

    class http_server {
    public:
        http_server() = delete;
        http_server(const std::string& host, int port, int timeout = 5000
                , size_t buffer_size = 1024, size_t thread_size = 0, int max_event_size = 64);

    public:
        void run(const std::function<bool(const mongols::request&)>& req_filter
                , const std::function<void(const mongols::request&, mongols::response&)>& res_filter);
    protected:
        virtual std::pair < std::string, bool> work(
                const std::function<bool(const mongols::request&)>& req_filter
                , const std::function<void(const mongols::request& req, mongols::response&)>& res_filter
                , const std::string&
                , bool&);
    private:
        bool parse_reqeust(const std::string& str, mongols::request& req, std::string& body);
        std::string create_response(mongols::response& res, bool b);
        std::string get_status_text(int status);
    private:
        mongols::tcp_server server;
    private:
        std::string tolower(std::string&);


    };


}

#endif /* HTTP_SERVER_HPP */

