#include "http_server.hpp"

#include <utility>


namespace mongols {

    http_request_parser::http_request_parser(mongols::request& req)
    : tmp(), parser(), settings(), req(req), body() {
        http_parser_init(&this->parser, HTTP_REQUEST);
        http_parser_settings_init(&this->settings);
        this->tmp.parser = this;
        this->parser.data = &this->tmp;
    }

    bool http_request_parser::parse(const std::string& str) {


        auto message_begin_cb = [](http_parser * p) {
            return 0;
        };
        this->settings.on_message_begin = message_begin_cb;

        auto header_field_cb = [](http_parser *p, const char *buf, size_t len) {
            tmp_* THIS = (tmp_*) p->data;
            THIS->pair.first = std::move(std::string(buf, len));
            THIS->parser->req.headers.insert(std::make_pair(THIS->pair.first, ""));
            return 0;
        };
        this->settings.on_header_field = header_field_cb;


        auto header_value_cb = [](http_parser *p, const char *buf, size_t len) {
            tmp_* THIS = (tmp_*) p->data;
            THIS->parser->req.headers[THIS->pair.first] = std::move(std::string(buf, len));
            return 0;
        };
        this->settings.on_header_value = header_value_cb;


        auto url_cb = [](http_parser *p, const char *buf, size_t len) {
            tmp_* THIS = (tmp_*) p->data;
            THIS->parser->req.uri = std::move(std::string(buf, len));
            return 0;
        };
        this->settings.on_url = url_cb;

        auto status_cb = [](http_parser*, const char *at, size_t length) {
            return 0;
        };
        this->settings.on_status = status_cb;

        auto body_cb = [](http_parser *p, const char *buf, size_t len) {
            tmp_* THIS = (tmp_*) p->data;
            THIS->parser->body = std::move(std::string(buf, len));
            return 0;
        };
        this->settings.on_body = body_cb;

        auto headers_complete_cb = [](http_parser * p) {
            tmp_* THIS = (tmp_*) p->data;
            THIS->parser->req.method = std::move(http_method_str((enum http_method)p->method));
            struct http_parser_url u;
            http_parser_url_init(&u);
            http_parser_parse_url(THIS->parser->req.uri.c_str(), THIS->parser->req.uri.size(), 0, &u);
            std::string path, param;
            if (u.field_set & (1 << UF_PATH)) {
                path = std::move(THIS->parser->req.uri.substr(u.field_data[UF_PATH].off, u.field_data[UF_PATH].len));
            }
            if (u.field_set & (1 << UF_QUERY)) {
                param = std::move(THIS->parser->req.uri.substr(u.field_data[UF_QUERY].off, u.field_data[UF_QUERY].len));
            }
            THIS->parser->req.param = std::move(param);
            THIS->parser->req.uri = std::move(path);
            return 0;
        };
        this->settings.on_headers_complete = headers_complete_cb;

        auto message_complete_cb = [](http_parser * p) {
            return 0;
        };
        this->settings.on_message_complete = message_complete_cb;

        auto chuck_header_cb = [](http_parser * p) {
            return 0;
        };
        this->settings.on_chunk_header = chuck_header_cb;

        auto chunk_complete_cb = [](http_parser * p) {
            return 0;
        };
        this->settings.on_chunk_complete = chunk_complete_cb;



        return http_parser_execute(&this->parser, &this->settings, str.c_str(), str.size()) == str.size();

    }

    const std::string& http_request_parser::get_body() const {
        return this->body;
    }

    std::string& http_request_parser::get_body() {
        return this->body;
    }

    class helloworld : public mongols::servlet {
    public:
        helloworld() = default;
        virtual~helloworld() = default;

        void handler(request& req, response& res) {
            res.headers.find("Content-Type")->second = "text/plain;charset=utf-8";
            res.content = std::move("hello,world");
            res.status = 200;

        }

    };

    http_server::http_server(const std::string& host, int port, int timeout, size_t buffer_size, size_t thread_size, int max_event_size)
    : server(host, port, timeout, buffer_size, thread_size, max_event_size) {

    }

    void http_server::run(const std::function<bool(const mongols::request&)>& req_filter
            , const std::function<void(const mongols::request& req, mongols::response&)>& res_filter) {

        std::function < std::pair < std::string, bool>(const std::string&) > g = [&](const std::string & input) {

            mongols::request req;
            mongols::response res;
            std::string body, output;
            if (this->parse_reqeust(input, req, body)) {

                if (req_filter(req)) {
                    //example
                    mongols::helloworld default_instance;
                    default_instance.handler(req, res);

                    res_filter(req,res);
                }

            } else {
                //default response
                mongols::helloworld default_instance;
                default_instance.handler(req, res);
                std::cout << "parser http failed." << std::endl;
            }



            bool b = CLOSE_CONNECTION;
            if (req.headers.find("Connection") != req.headers.end()) {
                if (req.headers["Connection"] == "keep-alive") {
                    b = KEEPALIVE_CONNECTION;
                }
            }

            output = std::move(this->create_response(res, b));


            return std::make_pair(std::move(output), b);
        };

        this->server.run(g);
    }

    bool http_server::parse_reqeust(const std::string& str, mongols::request& req, std::string& body) {
        mongols::http_request_parser parser(req);
        if (parser.parse(str)) {
            body = std::move(parser.get_body());
            return true;
        }
        return false;
    }

    std::string http_server::create_response(mongols::response& res, bool b) {
        std::string output;
        output.append("HTTP/1.1 ").append(std::to_string(res.status)).append(" " + this->get_status_text(res.status)).append("\r\n");
        res.headers.insert(std::make_pair("Server", "mongols/0.1.0"));
        if (b == KEEPALIVE_CONNECTION) {
            res.headers.insert(std::make_pair("Connection", "keep-alive"));
        } else {
            res.headers.insert(std::make_pair("Connection", "close"));
        }
        res.headers.insert(std::make_pair("Content-Length", std::to_string(res.content.size() + 2)));
        for (auto& i : res.headers) {
            output.append(i.first).append(": ").append(i.second).append("\r\n");
        }
        output.append("\r\n").append(res.content).append("\r\n");
        return output;
    }

    std::string http_server::get_status_text(int status) {
        switch (status) {
            case 100: return "Continue";
            case 101: return "Switching Protocols";
            case 200: return "OK";
            case 201: return "Created";
            case 202: return "Accepted";
            case 203: return "Non-Authoritative Information";
            case 204: return "No Content";
            case 205: return "Reset Content";
            case 206: return "Partial Content";
            case 300: return "Multiple Choices";
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 303: return "See Other";
            case 304: return "Not Modified";
            case 305: return "Use Proxy";
            case 306: return "Switch Proxy";
            case 307: return "Temporary Redirect";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 402: return "Payment Required";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 406: return "Not Acceptable";
            case 407: return "Proxy Authentication Required";
            case 408: return "Request Timeout";
            case 409: return "Conflict";
            case 410: return "Gone";
            case 411: return "Length Required";
            case 412: return "Precondition Failed";
            case 413: return "Request Entity Too Large";
            case 414: return "Request-URI Too Long";
            case 415: return "Unsupported Media Type";
            case 416: return "Requested Range Not Satisfiable";
            case 417: return "Expectation Failed";
            case 500: return "Internal Server Error";
            case 501: return "Not Implemented";
            case 502: return "Bad Gateway";
            case 503: return "Service Unavailable";
            case 504: return "Gateway Timeout";
            case 505: return "HTTP Version Not Supported";
            default: return ("Not supported status code.");
        }
    }

}


