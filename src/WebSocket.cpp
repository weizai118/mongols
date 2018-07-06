// WebSocket, v1.00 2012-09-13
//
// Description: WebSocket RFC6544 codec, written in C++.
// Homepage: http://katzarsky.github.com/WebSocket
// Author: katzarsky@gmail.com



#include <iostream>
#include <string>
#include <vector>
#include "lib/WebSocket.h"
#include "lib/libwshandshake.hpp"

WebSocket::WebSocket() {

}

WebSocketFrameType WebSocket::parseHandshake(const char* input_frame, size_t input_len) {
    std::string headers(input_frame, input_len);
    size_t header_end = headers.find("\r\n\r\n");

    if (header_end == std::string::npos) {
        return INCOMPLETE_FRAME;
    }

    headers.resize(header_end);
    std::vector<std::string> headers_rows = std::move(this->explode(headers, std::string("\r\n")));
    for (size_t i = 0; i < headers_rows.size(); i++) {
        std::string& header = headers_rows[i];
        if (header.find("GET") == 0) {
            std::vector<std::string> get_tokens = std::move(this->explode(header, std::string(" ")));
            if (get_tokens.size() >= 2) {
                this->resource = get_tokens[1];
            }
        } else {
            size_t pos = header.find(":");
            if (pos != std::string::npos) {
                std::string header_key(header, 0, pos);
                std::string header_value(header, pos + 1);
                header_value = std::move(this->trim(header_value));
                if (header_key == "Host") this->host = std::move(header_value);
                else if (header_key == "Origin") this->origin = std::move(header_value);
                else if (header_key == "Sec-WebSocket-Key") this->key = std::move(header_value);
                else if (header_key == "Sec-WebSocket-Protocol") this->protocol = std::move(header_value);
            }
        }
    }

    return OPENING_FRAME;
}

std::string WebSocket::trim(std::string& str) {
    const char* whitespace = " \t\r\n";
    std::string::size_type pos = str.find_last_not_of(whitespace);
    if (pos != std::string::npos) {
        str.erase(pos + 1);
        pos = str.find_first_not_of(whitespace);
        if (pos != std::string::npos) str.erase(0, pos);
    } else {
        return std::string();
    }
    return str;
}

std::vector<std::string> WebSocket::explode(
        const std::string& theString,
        const std::string& theDelimiter,
        bool theIncludeEmptyStrings) {


    std::vector<std::string> theStringVector;
    size_t start = 0, end = 0, length = 0;

    while (end != std::string::npos) {
        end = theString.find(theDelimiter, start);


        length = (end == std::string::npos) ? std::string::npos : end - start;

        if (theIncludeEmptyStrings || ((length > 0) && (start < theString.size()))) {
            theStringVector.push_back(std::move(theString.substr(start, length)));
        }


        start = ((end > (std::string::npos - theDelimiter.size()))
                ? std::string::npos : end + theDelimiter.size());
    }
    return theStringVector;
}

std::string WebSocket::answerHandshake() {

    std::string answer;
    answer += "HTTP/1.1 101 Switching Protocols\r\n";
    answer += "Upgrade: WebSocket\r\n";
    answer += "Connection: Upgrade\r\n";
    if (this->key.length() > 0) {
        char output[29] = {0};
        WebSocketHandshake::generate(this->key.c_str(), output);
        std::string accept_key = std::move(output);

        answer += "Sec-WebSocket-Accept: " + (accept_key) + "\r\n";
    }
    if (this->protocol.length() > 0) {
        answer += "Sec-WebSocket-Protocol: " + (this->protocol) + "\r\n";
    }
    answer += "\r\n";
    return answer;
}

size_t WebSocket::makeFrame(WebSocketFrameType frame_type, const char* msg, size_t msg_length, char* buffer, size_t buffer_size) {
    size_t pos = 0;
    size_t size = msg_length;
    buffer[pos++] = (char) frame_type;

    if (size <= 125) {
        buffer[pos++] = size;
    } else if (size <= 65535) {
        buffer[pos++] = 126;

        buffer[pos++] = (size >> 8) & 0xFF;
        buffer[pos++] = size & 0xFF;
    } else {
        buffer[pos++] = 127;


        for (size_t i = 3; i >= 0; i--) {
            buffer[pos++] = 0;
        }

        for (size_t i = 3; i >= 0; i--) {
            buffer[pos++] = ((size >> 8 * i) & 0xFF);
        }
    }
    memcpy((void*) (buffer + pos), msg, size);
    return (size + pos);
}

WebSocketFrameType WebSocket::getFrame(char* in_buffer, size_t in_length, std::string& out) {

    if (in_length < 3) return INCOMPLETE_FRAME;

    unsigned char msg_opcode = in_buffer[0] & 0x0F;
    unsigned char msg_fin = (in_buffer[0] >> 7) & 0x01;
    unsigned char msg_masked = (in_buffer[1] >> 7) & 0x01;

    // *** message decoding 

    size_t payload_length = 0;
    size_t pos = 2;
    size_t length_field = in_buffer[1]& 0x7f;

    unsigned int mask = 0;



    if (length_field <= 125) {
        payload_length = length_field;
    } else if (length_field == 126) {
        payload_length = (
                (in_buffer[2] << 8) |
                (in_buffer[3])
                );
        pos += 2;
    } else if (length_field == 127) {
        payload_length = (
                (in_buffer[2] << 56) |
                (in_buffer[3] << 48) |
                (in_buffer[4] << 40) |
                (in_buffer[5] << 32) |
                (in_buffer[6] << 24) |
                (in_buffer[7] << 16) |
                (in_buffer[8] << 8) |
                (in_buffer[9])
                );
        pos += 8;
    }



    if (in_length < payload_length + pos) {
        return INCOMPLETE_FRAME;
    }

    if (msg_masked) {
        mask = *((unsigned int*) (in_buffer + pos));

        pos += 4;

        unsigned char* c = (unsigned char*) in_buffer + pos;
        for (size_t i = 0; i < payload_length; i++) {
            c[i] = c[i] ^ ((unsigned char*) (&mask))[i % 4];
        }

    }


    out.assign(in_buffer + pos, payload_length);


    if (msg_opcode == 0x0) return (msg_fin) ? TEXT_FRAME : INCOMPLETE_TEXT_FRAME;
    if (msg_opcode == 0x1) return (msg_fin) ? TEXT_FRAME : INCOMPLETE_TEXT_FRAME;
    if (msg_opcode == 0x2) return (msg_fin) ? BINARY_FRAME : INCOMPLETE_BINARY_FRAME;
    if (msg_opcode == 0x9) return PING_FRAME;
    if (msg_opcode == 0xA) return PONG_FRAME;

    return ERROR_FRAME;
}
