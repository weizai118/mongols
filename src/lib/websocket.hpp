#ifndef WEBSOCKET_HPP
#define WEBSOCKET_HPP


#include <string>
#include <sstream>
#include <cstdlib>
#include <cstring>

#include "libwshandshake.hpp"

namespace mongols {

    enum WS_Status {
        WS_STATUS_CONNECT = 0,
        WS_STATUS_UNCONNECT = 1,
    };

    enum WS_FrameType {
        WS_EMPTY_FRAME = 0xF0,
        WS_ERROR_FRAME = 0xF1,
        WS_TEXT_FRAME = 0x01,
        WS_BINARY_FRAME = 0x02,
        WS_PING_FRAME = 0x09,
        WS_PONG_FRAME = 0x0A,
        WS_OPENING_FRAME = 0xF3,
        WS_CLOSING_FRAME = 0x08
    };

    int ws_handshake(const std::string &request, std::string &response) {
        int ret = WS_STATUS_UNCONNECT;
        std::istringstream stream(request.c_str());
        std::string reqType;
        std::getline(stream, reqType);
        if (reqType.substr(0, 4) != "GET ") {
            return ret;
        }

        std::string header;
        std::string::size_type pos = 0;
        std::string websocketKey;
        while (std::getline(stream, header) && header != "\r") {
            header.erase(header.end() - 1);
            pos = header.find(": ", 0);
            if (pos != std::string::npos) {
                std::string key = header.substr(0, pos);
                std::string value = header.substr(pos + 2);
                if (key == "Sec-WebSocket-Key") {
                    ret = WS_STATUS_CONNECT;
                    websocketKey = value;
                    break;
                }
            }
        }

        if (ret != WS_STATUS_CONNECT) {
            return ret;
        }

        response = "HTTP/1.1 101 Switching Protocols\r\n";
        response += "Upgrade: websocket\r\n";
        response += "Connection: upgrade\r\n";
        response += "Sec-WebSocket-Accept: ";



        char output[29] = {};
        WebSocketHandshake::generate(websocketKey.c_str(), output);
        std::string serverKey = std::move(output);
        serverKey += "\r\n\r\n";
        response += serverKey;

        return ret;
    }

    int ws_decode_frame(const std::string& inFrame, std::string &outMessage) {
        int ret = WS_OPENING_FRAME;
        const char *frameData = inFrame.c_str();
        const int frameLength = inFrame.size();
        if (frameLength < 2) {
            ret = WS_ERROR_FRAME;
        }

        if ((frameData[0] & 0x70) != 0x0) {
            ret = WS_ERROR_FRAME;
        }

        ret = (frameData[0] & 0x80);
        if (ret != 0x80) {
            ret = WS_ERROR_FRAME;
        }


        if ((frameData[1] & 0x80) != 0x80) {
            ret = WS_ERROR_FRAME;
        }


        uint16_t payloadLength = 0;
        uint8_t payloadFieldExtraBytes = 0;
        uint8_t opcode = static_cast<uint8_t> (frameData[0] & 0x0f);
        ret = opcode;
        if (opcode == WS_TEXT_FRAME) {
            payloadLength = static_cast<uint16_t> (frameData[1] & 0x7f);
//            if (payloadLength == 0x7e) {
                uint16_t payloadLength16b = 0;
                payloadFieldExtraBytes = 2;
                memcpy(&payloadLength16b, &frameData[2], payloadFieldExtraBytes);
                payloadLength = ntohs(payloadLength16b);
//            } else if (payloadLength == 0x7f) {
//                ret = WS_ERROR_FRAME;
//            }
        }

        if ((ret != WS_ERROR_FRAME) && (payloadLength > 0)) {
            const char *maskingKey = &frameData[2 + payloadFieldExtraBytes];
            char *payloadData = new char[payloadLength + 1];
            memset(payloadData, 0, payloadLength + 1);
            memcpy(payloadData, &frameData[2 + payloadFieldExtraBytes + 4], payloadLength);
            for (int i = 0; i < payloadLength; i++) {
                payloadData[i] = payloadData[i] ^ maskingKey[i % 4];
            }

            outMessage = payloadData;
            delete[] payloadData;
        }

        return ret;
    }

    size_t ws_calc_frame_size(enum WS_FrameType flags, size_t data_len) {
        size_t size = data_len + 2; // body + 2 bytes of head
        if (data_len >= 126) {
            if (data_len > 0xFFFF) {
                size += 8;
            } else {
                size += 2;
            }
        }
        if (flags & 0x20) {
            size += 4;
        }

        return size;
    }

    uint8_t ws_decode(char * dst, const char * src, size_t len, const char mask[4], uint8_t mask_offset) {
        size_t i = 0;
        for (; i < len; i++) {
            dst[i] = src[i] ^ mask[(i + mask_offset) % 4];
        }

        return (uint8_t) ((i + mask_offset) % 4);
    }

    size_t ws_build_frame(char * frame, enum WS_FrameType flags, const char mask[4], const char * data, size_t data_len) {
        size_t body_offset = 0;
        frame[0] = 0;
        frame[1] = 0;
        if (flags & 0x10) {
            frame[0] = (char) (1 << 7);
        }
        frame[0] |= flags & 0xF;
        if (flags & 0x20) {
            frame[1] = (char) (1 << 7);
        }
        if (data_len < 126) {
            frame[1] |= data_len;
            body_offset = 2;
        } else if (data_len <= 0xFFFF) {
            frame[1] |= 126;
            frame[2] = (char) (data_len >> 8);
            frame[3] = (char) (data_len & 0xFF);
            body_offset = 4;
        } else {
            frame[1] |= 127;
            frame[2] = (char) ((data_len >> 56) & 0xFF);
            frame[3] = (char) ((data_len >> 48) & 0xFF);
            frame[4] = (char) ((data_len >> 40) & 0xFF);
            frame[5] = (char) ((data_len >> 32) & 0xFF);
            frame[6] = (char) ((data_len >> 24) & 0xFF);
            frame[7] = (char) ((data_len >> 16) & 0xFF);
            frame[8] = (char) ((data_len >> 8) & 0xFF);
            frame[9] = (char) ((data_len) & 0xFF);
            body_offset = 10;
        }
        if (flags & 0x20) {
            if (mask != NULL) {
                memcpy(&frame[body_offset], mask, 4);
            }
            ws_decode(&frame[body_offset + 4], data, data_len, &frame[body_offset], 0);
            body_offset += 4;
        } else {
            memcpy(&frame[body_offset], data, data_len);
        }

        return body_offset + data_len;
    }

    int ws_encode_frame(const std::string& inMessage, std::string &outFrame, enum WS_FrameType frameType) {
        size_t frame_size = ws_calc_frame_size(frameType, inMessage.size());
        char frame[frame_size];
        memset(frame, 0, frame_size);
        size_t len = ws_build_frame(frame, (WS_FrameType) (frameType | 0x10), NULL, inMessage.c_str(), inMessage.size());
        outFrame.assign(frame, len);
        return frameType;
    }

}

#endif /* WEBSOCKET_HPP */

