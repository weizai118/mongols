#include "ws_server.hpp"
#include "lib/libwshandshake.hpp"
#include "lib/WebSocket.h"
#include "lib/json11.hpp"

#include <algorithm>
#include <vector>


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
        handler_function f = std::bind(&ws_server::ws_json_parse, this
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

    std::string ws_server::ws_json_parse(const std::string& input
            , bool& keepalive
            , bool& send_to_other
            , std::pair<size_t, size_t>& g_u_id
            , tcp_server::filter_handler_function& send_to_other_filter) {
        if (input == "quit" || input == "exit" || input == "close")return input;
        std::string err;
        json11::Json root = json11::Json::parse(input, err);
        if (err.empty()) {
            if (!root["uid"].is_number()
                    || !root["gid"].is_number()
                    || !root["name"].is_string()
                    || !root["message"].is_string()
                    || !root["gfilter"].is_array()
                    || !root["ufilter"].is_array()) {
                goto json_err;
            } else {
                if (g_u_id.first == 0) {
                    g_u_id.first = root["gid"].int_value();
                }
                if (g_u_id.second == 0) {
                    g_u_id.second = root["uid"].int_value();
                }
                keepalive = KEEPALIVE_CONNECTION;
                send_to_other = true;
                std::vector<size_t> gfilter, ufilter;
                for (auto &i : root["gfilter"].array_items()) {
                    if (i.is_number()) {
                        gfilter.push_back(i.int_value());
                    }
                }
                for (auto &i : root["ufilter"].array_items()) {
                    if (i.is_number()) {
                        ufilter.push_back(i.int_value());
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


                return input;
            }
        } else {
json_err:

            return "error json message";
        }

    }

    std::pair<std::string, bool> ws_server::work(const handler_function& f
            , const std::string& input
            , bool& send_to_other
            , std::pair<size_t, size_t>& g_u_id
            , tcp_server::filter_handler_function& send_to_other_filter) {
        std::string response;
        bool keepalive = KEEPALIVE_CONNECTION;
        send_to_other = false;
        WebSocket ws;
        if (input[0] == 'G') {

            if (ws.parseHandshake(input.c_str(), input.size()) == OPENING_FRAME) {
                response = ws.answerHandshake();
            } else {
                response = "";
                keepalive = CLOSE_CONNECTION;
            }
            goto ws_done;
        } else {

            const char* close_msg = "connection closed.", *empty_msg = "", *error_msg = "error message."
                    , *binary_msg = "not accept binary message.";

            const size_t buffer_size = this->server.get_buffer_size();
            char in_buffer[buffer_size];

            strcpy(in_buffer, input.c_str());

            std::string out;
            int len;

            auto wsft = ws.getFrame(in_buffer, buffer_size, out);

            memset(in_buffer, 0, buffer_size);
            if (wsft == TEXT_FRAME || wsft == BINARY_FRAME) {
                out = std::move(f(out, keepalive, send_to_other, g_u_id, send_to_other_filter));
                if (out == "close" || out == "quit" || out == "exit") {
                    goto ws_exit;
                }
                if (out.empty()) {
                    out = std::move("empty message.");
                    send_to_other = false;
                }
                len = ws.makeFrame((WebSocketFrameType) wsft, out.c_str(), out.size(), in_buffer, buffer_size);
                response.assign(in_buffer, len);
                goto ws_done;
            } else if (wsft == PONG_FRAME) {
                len = ws.makeFrame(PING_FRAME, empty_msg, strlen(empty_msg), in_buffer, buffer_size);
                response.assign(in_buffer, len);
                goto ws_done;
            } else if (wsft == PING_FRAME) {
                len = ws.makeFrame(PONG_FRAME, empty_msg, strlen(empty_msg), in_buffer, buffer_size);
                response.assign(in_buffer, len);
                goto ws_done;
            } else if (wsft == CLOSING_FRAME
                    || wsft == ERROR_FRAME
                    || wsft == INCOMPLETE_TEXT_FRAME
                    || wsft == INCOMPLETE_BINARY_FRAME
                    || wsft == INCOMPLETE_FRAME) {
ws_exit:
                len = ws.makeFrame(CLOSING_FRAME, close_msg, strlen(close_msg), in_buffer, buffer_size);
                response.assign(in_buffer, len);
                keepalive = CLOSE_CONNECTION;
                goto ws_done;
            } else {
                len = ws.makeFrame(ERROR_FRAME, error_msg, strlen(error_msg), in_buffer, buffer_size);
                response.assign(in_buffer, len);
                keepalive = CLOSE_CONNECTION;
                goto ws_done;
            }
        }

ws_done:
        return std::make_pair(std::move(response), keepalive);
    }



}
