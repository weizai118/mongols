

#include "ws_server.hpp"
#include "lib/libwshandshake.hpp"
#include "lib/WebSocket.h"

#include <algorithm>


namespace mongols {

    ws_server::ws_server(const std::string& host, int port, int timeout
            , size_t buffer_size, size_t thread_size, int max_event_size)
    : server(host, port, timeout, buffer_size, thread_size, max_event_size) {

    }

    void ws_server::run() {
        this->server.run(std::bind(&ws_server::work, this, std::placeholders::_1, std::placeholders::_2));
    }

    std::pair<std::string, bool> ws_server::work(const std::string& input, bool& send_to_other) {
        std::string response;
        bool b = KEEPALIVE_CONNECTION;
        send_to_other = false;
        WebSocket ws;
        if (input[0] == 'G') {

            if (ws.parseHandshake(input.c_str(), input.size()) == OPENING_FRAME) {
                response = ws.answerHandshake();
            } else {
                response = "";
                b = CLOSE_CONNECTION;
            }
            goto ws_done;
        } else {

            const char* close_msg = "close.", *empty_msg = "",
                    *error_msg = "error message";

            const size_t buffer_size = 1024;
            char in_buffer[buffer_size];

            strcpy(in_buffer, input.c_str());

            std::string out;
            int len;

            auto wsft = ws.getFrame(in_buffer, strlen(in_buffer), out);

            memset(in_buffer, 0, buffer_size);
            if (wsft == TEXT_FRAME||wsft==BINARY_FRAME) {
                len = ws.makeFrame((WebSocketFrameType)wsft, out.c_str(), out.size(), in_buffer, buffer_size);
                response.assign(in_buffer, len);
                send_to_other = true;
                goto ws_done;
            } else if (wsft == PONG_FRAME) {
                len = ws.makeFrame(PING_FRAME, empty_msg, strlen(empty_msg), in_buffer, buffer_size);
                response.assign(in_buffer, len);
                goto ws_done;
            } else if (wsft == PING_FRAME) {
                len = ws.makeFrame(PONG_FRAME, empty_msg, strlen(empty_msg), in_buffer, buffer_size);
                response.assign(in_buffer, len);
                goto ws_done;
            } else if (wsft == CLOSING_FRAME || wsft == ERROR_FRAME
                    || wsft == INCOMPLETE_TEXT_FRAME
                    || wsft == INCOMPLETE_BINARY_FRAME
                    || wsft == INCOMPLETE_FRAME) {
                len = ws.makeFrame(CLOSING_FRAME, close_msg, strlen(close_msg), in_buffer, buffer_size);
                response.assign(in_buffer, len);
                b = CLOSE_CONNECTION;
                goto ws_done;
            } else {
                len = ws.makeFrame(ERROR_FRAME, error_msg, strlen(error_msg), in_buffer, buffer_size);
                response.assign(in_buffer, len);
                goto ws_done;
            }
        }

ws_done:
        return std::make_pair(std::move(response), b);
    }



}
