#include "ws_server.hpp"
#include "lib/websocket.hpp"
#include "util.hpp"
#include <algorithm>
#include <vector>
#include <sstream>


namespace mongols {

    ws_server::ws_server(const std::string& host, int port, int timeout
            , size_t buffer_size, size_t thread_size, int max_event_size)
    : server(host, port, timeout, buffer_size, thread_size, max_event_size) {

    }

    void ws_server::run(const handler_function& f) {
        this->server.run(std::bind(&ws_server::work, this
                , std::cref(f)
                , std::placeholders::_1
                , std::placeholders::_2
                , std::placeholders::_3
                , std::placeholders::_4));
    }

    void ws_server::run() {
        handler_function f = std::bind(&ws_server::ws_message_parse, this
                , std::placeholders::_1
                , std::placeholders::_2
                , std::placeholders::_3
                , std::placeholders::_4
                , std::placeholders::_5
                );
        this->server.run(std::bind(&ws_server::work, this
                , f
                , std::placeholders::_1
                , std::placeholders::_2
                , std::placeholders::_3
                , std::placeholders::_4
                ));
    }

    std::string ws_server::ws_message_parse(const std::string& message
            , bool& keepalive
            , bool& send_to_other
            , std::pair<size_t, size_t>& g_u_id
            , tcp_server::filter_handler_function& send_to_other_filter) {
        std::istringstream stream(message.c_str());
        std::string tmp, k, v;
        std::string::size_type pos = 0;
        keepalive = KEEPALIVE_CONNECTION;
        send_to_other = true;
        std::vector<size_t> gfilter, ufilter;
        int n = 0;
        while (n != 4 && std::getline(stream, tmp) && tmp != "\r\n") {
            pos = tmp.find(": ");
            if (pos != std::string::npos) {
                k = std::move(tmp.substr(0, pos));
                v = std::move(tmp.substr(pos + 2, tmp.size() - 2 - pos));
                try {
                    if (k == "uid") {
                        if (g_u_id.second == 0) {
                            g_u_id.second = std::stoul(v);
                        }
                        ++n;
                    } else if (k == "gid") {
                        if (g_u_id.first == 0) {
                            g_u_id.first = std::stoul(v);
                        }
                        ++n;
                    } else if (k == "ufilter") {
                        std::vector<std::string> utmp;
                        split(v, ',', utmp);
                        for (auto& i : utmp) {
                            ufilter.push_back(std::stoul(i));
                        }
                        ++n;
                    } else if (k == "gfilter") {
                        std::vector<std::string> gtmp;
                        split(v, ',', gtmp);
                        for (auto& i : gtmp) {
                            gfilter.push_back(std::stoul(i));
                        }
                        ++n;
                    }
                } catch (...) {

                }
            }
        }

        send_to_other_filter = [ = ](const std::pair<size_t, size_t>& cur_g_u_id){
            bool res = false;
            if (gfilter.empty()) {
                if (ufilter.empty()) {
                    res = true;
                } else {
                    res = std::find(ufilter.begin(), ufilter.end(), cur_g_u_id.second) != ufilter.end();
                }
            } else {
                if (ufilter.empty()) {
                    res = std::find(gfilter.begin(), gfilter.end(), cur_g_u_id.first) != gfilter.end();
                } else {
                    res = std::find(gfilter.begin(), gfilter.end(), cur_g_u_id.first) != gfilter.end()
                            && std::find(ufilter.begin(), ufilter.end(), cur_g_u_id.second) != ufilter.end();
                }
            }
            return res;

        };

        return message;
    }

    std::pair<std::string, bool> ws_server::work(const handler_function& f
            , const std::string& input
            , bool& send_to_other
            , std::pair<size_t, size_t>& g_u_id
            , tcp_server::filter_handler_function& send_to_other_filter) {
        std::string response;
        bool keepalive = KEEPALIVE_CONNECTION;
        send_to_other = false;

        if (input[0] == 'G') {

            if (ws_handshake(input, response) == WS_STATUS_CONNECT) {

            } else {
                response = "";
                keepalive = CLOSE_CONNECTION;
            }
            goto ws_done;
        } else {

            const std::string close_msg = "connection closed.", empty_msg, error_msg = "error message."
                    , binary_msg = "not accept binary message.";
            std::string message;

            auto wsft = (WS_FrameType) ws_decode_frame(input, message);


            if (wsft == WS_TEXT_FRAME) {
                f(message, keepalive, send_to_other, g_u_id, send_to_other_filter);
                ws_encode_frame(message,response,WS_TEXT_FRAME);
                goto ws_done;
            } else if (wsft == WS_BINARY_FRAME) {
                ws_encode_frame(binary_msg, response, WS_TEXT_FRAME);
                goto ws_done;
            } else if (wsft == WS_PONG_FRAME) {
                ws_encode_frame(empty_msg, response, WS_PING_FRAME);
                goto ws_done;
            } else if (wsft == WS_PING_FRAME) {
                ws_encode_frame(empty_msg, response, WS_PONG_FRAME);
                goto ws_done;
            } else {
                ws_encode_frame(close_msg, response, WS_CLOSING_FRAME);
                keepalive = CLOSE_CONNECTION;
                goto ws_done;
            }
        }

ws_done:
        return std::make_pair(std::move(response), keepalive);
    }



}
