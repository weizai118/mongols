#include <sys/stat.h>
#include <utility>
#include <algorithm>
#include <functional>
#include <memory>


#include "tcp_threading_server.hpp"
#include "http_server.hpp"
#include "util.hpp"
#include "MPFDParser/Parser.h"

#define mongols_http_server_version "mongols/0.9.0"
#define form_urlencoded_type "application/x-www-form-urlencoded"
#define form_urlencoded_type_len (sizeof(form_urlencoded_type) - 1)
#define TEMP_DIRECTORY "temp"

namespace mongols {

    http_request_parser::http_request_parser(mongols::request& req)
    : tmp(), parser(), settings(), req(req), body() {
        http_parser_init(&this->parser, HTTP_REQUEST);
        http_parser_settings_init(&this->settings);
        this->tmp.parser = this;
        this->parser.data = &this->tmp;
    }

    bool http_request_parser::parse(const std::string& str) {


        this->settings.on_message_begin = [](http_parser * p) {
            return 0;
        };

        this->settings.on_header_field = [](http_parser *p, const char *buf, size_t len) {
            tmp_* THIS = (tmp_*) p->data;
            THIS->pair.first = std::move(std::string(buf, len));
            THIS->parser->req.headers.insert(std::make_pair(THIS->pair.first, ""));
            return 0;
        };


        this->settings.on_header_value = [](http_parser *p, const char *buf, size_t len) {
            tmp_* THIS = (tmp_*) p->data;
            THIS->parser->req.headers[THIS->pair.first] = std::move(std::string(buf, len));
            return 0;
        };



        this->settings.on_url = [](http_parser *p, const char *buf, size_t len) {
            tmp_* THIS = (tmp_*) p->data;
            THIS->parser->req.uri = std::move(std::string(buf, len));
            return 0;
        };


        this->settings.on_status = [](http_parser*, const char *at, size_t length) {
            return 0;
        };

        this->settings.on_body = [](http_parser *p, const char *buf, size_t len) {
            tmp_* THIS = (tmp_*) p->data;
            THIS->parser->body = std::move(std::string(buf, len));
            return 0;
        };

        this->settings.on_headers_complete = [](http_parser * p) {
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

        this->settings.on_message_complete = [](http_parser * p) {
            return 0;
        };

        this->settings.on_chunk_header = [](http_parser * p) {
            return 0;
        };

        this->settings.on_chunk_complete = [](http_parser * p) {
            return 0;
        };



        return http_parser_execute(&this->parser, &this->settings, str.c_str(), str.size()) == str.size();

    }

    const std::string& http_request_parser::get_body() const {
        return this->body;
    }

    std::string& http_request_parser::get_body() {
        return this->body;
    }

    http_server::http_server(const std::string& host, int port
            , int timeout
            , size_t buffer_size
            , size_t thread_size
            , size_t max_body_size
            , int max_event_size)
    : server(0), max_body_size(max_body_size) {
        if (thread_size > 0) {
            this->server = new tcp_threading_server(host, port, timeout, buffer_size, thread_size, max_event_size);
        } else {
            this->server = new tcp_server(host, port, timeout, buffer_size, max_event_size);
        }

    }

    http_server::~http_server() {
        if (this->server) {
            delete this->server;
        }
    }

    void http_server::run(const std::function<bool(const mongols::request&)>& req_filter
            , const std::function<void(const mongols::request&, mongols::response&)>& res_filter) {

        tcp_server::handler_function g = std::bind(&http_server::work, this
                , std::cref(req_filter)
                , std::cref(res_filter)
                , std::placeholders::_1
                , std::placeholders::_2
                , std::placeholders::_3
                , std::placeholders::_4);

        this->server->run(g);
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
        res.headers.insert(std::move(std::make_pair("Server", mongols_http_server_version)));
        if (b == KEEPALIVE_CONNECTION) {
            res.headers.insert(std::move(std::make_pair("Connection", "keep-alive")));
        } else {
            res.headers.insert(std::move(std::make_pair("Connection", "close")));
        }
        res.headers.insert(std::move(std::make_pair("Content-Length", std::to_string(res.content.size() + 2))));
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

    std::string http_server::tolower(std::string& str) {
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
            return std::tolower(c);
        });
        return str;
    }

    void http_server::upload(mongols::request& req, const std::string& body) {
        try {
            if ((is_dir(TEMP_DIRECTORY) || mkdir(TEMP_DIRECTORY, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0)) {
                std::shared_ptr<MPFD::Parser> POSTParser(new MPFD::Parser());
                POSTParser->SetTempDirForFileUpload(TEMP_DIRECTORY);
                POSTParser->SetUploadedFilesStorage(MPFD::Parser::StoreUploadedFilesInFilesystem);
                POSTParser->SetMaxCollectedDataLength(this->server->get_buffer_size());
                POSTParser->SetContentType(req.headers["Content-Type"]);
                POSTParser->AcceptSomeData(body.c_str(), body.size());
                auto fields = POSTParser->GetFieldsMap();

                for (auto &item : fields) {
                    if (item.second->GetType() == MPFD::Field::TextType) {
                        req.form.insert(std::make_pair(item.first, item.second->GetTextTypeContent()));
                    } else {
                        std::string upload_file_name = item.second->GetFileName(), ext;
                        std::string::size_type p = upload_file_name.find_last_of(".");
                        if (p != std::string::npos) {
                            ext = std::move(upload_file_name.substr(p));
                        }
                        std::string temp_file = std::move(TEMP_DIRECTORY + ("/" + mongols::random_string(req.client + item.second->GetFileName()).append(ext)));
                        rename(item.second->GetTempFileName().c_str(), temp_file.c_str());
                        req.form.insert(std::make_pair(item.first, temp_file));
                    }
                }
            }
        } catch (MPFD::Exception& err) {

        }
    }

    std::pair < std::string, bool> http_server::work(
            const std::function<bool(const mongols::request&)>& req_filter
            , const std::function<void(const mongols::request&, mongols::response&)>& res_filter
            , const std::string& input
            , bool& send_to_other
            , std::pair<size_t, size_t>&
            , tcp_server::filter_handler_function&) {
        send_to_other = false;
        mongols::request req;
        mongols::response res;
        std::string body, output;
        bool conn = CLOSE_CONNECTION;
        if (this->parse_reqeust(input, req, body)) {

            if (body.size()>this->max_body_size) {
                body.clear();
            }
            if (req_filter(req)) {

                std::unordered_map<std::string, std::string>::iterator tmp;
                if ((tmp = req.headers.find("Connection")) != req.headers.end()) {
                    if (tmp->second == "keep-alive") {
                        conn = KEEPALIVE_CONNECTION;
                    }
                }
                if (!req.param.empty()) {
                    mongols::parse_param(req.param, req.form);
                }
                if (!body.empty()&& (tmp = req.headers.find("Content-Type")) != req.headers.end()) {
                    if (tmp->second.size() != form_urlencoded_type_len
                            || tmp->second != form_urlencoded_type) {
                        this->upload(req, body);
                        body.clear();
                    } else {
                        mongols::parse_param(body, req.form);
                    }
                }

                res_filter(req, res);

            }
        }


        output = std::move(this->create_response(res, conn));


        return std::make_pair(std::move(output), conn);
    }

}


