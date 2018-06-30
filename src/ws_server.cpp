

#include "ws_server.hpp"
#include "lib/libwshandshake.hpp"
#include "lib/WebSocket.h"

#include <algorithm>


namespace mongols {

    ws_server::ws_server(const std::string& host, int port, int timeout
            , size_t buffer_size, size_t thread_size, int max_event_size)
    : server(host, port, timeout, buffer_size, thread_size, max_event_size) {

    }

    void ws_server::run(const std::function<std::string(const std::string&, bool&, bool&)>& f
            , const std::function<bool(const std::pair<size_t, size_t>&)>& h) {
        this->server.run(std::bind(&ws_server::work, this
                , std::cref(f)
                , std::placeholders::_1
                , std::placeholders::_2
                , std::placeholders::_3)
                , std::cref(h));
    }

    std::pair<std::string, bool> ws_server::work(const std::function<std::string(const std::string&, bool&, bool&)>& f
            , const std::string& input
            , bool& send_to_other
            , std::pair<size_t, size_t>& g_u_id) {
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

            const char* close_msg = "close.", *empty_msg = "", *error_msg = "error message";

            const size_t buffer_size = this->server.get_buffer_size();
            char in_buffer[buffer_size];

            strcpy(in_buffer, input.c_str());

            std::string out;
            int len;

            auto wsft = ws.getFrame(in_buffer, strlen(in_buffer), out);

            memset(in_buffer, 0, buffer_size);
            if (wsft == TEXT_FRAME || wsft == BINARY_FRAME) {
                out = std::move(f(out, keepalive, send_to_other));
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
