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
        if ((frameData[0] & 0x80) != 0x80) {
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
            if (payloadLength == 0x7e) {
                uint16_t payloadLength16b = 0;
                payloadFieldExtraBytes = 2;
                memcpy(&payloadLength16b, &frameData[2], payloadFieldExtraBytes);
                payloadLength = ntohs(payloadLength16b);
            } else if (payloadLength == 0x7f) {
                ret = WS_ERROR_FRAME;
            }
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

    int ws_encode_frame(const std::string& inMessage, std::string &outFrame, enum WS_FrameType frameType) {
        int ret = WS_EMPTY_FRAME;
        const uint32_t messageLength = inMessage.size();
        if (messageLength > 32767) {
            return WS_ERROR_FRAME;
        }

        uint8_t payloadFieldExtraBytes = (messageLength <= 0x7d) ? 0 : 2;
        uint8_t frameHeaderSize = 2 + payloadFieldExtraBytes;
        uint8_t *frameHeader = new uint8_t[frameHeaderSize];
        memset(frameHeader, 0, frameHeaderSize);
        frameHeader[0] = static_cast<uint8_t> (0x80 | frameType);

        if (messageLength <= 0x7d) {
            frameHeader[1] = static_cast<uint8_t> (messageLength);
        } else {
            frameHeader[1] = 0x7e;
            uint16_t len = htons(messageLength);
            memcpy(&frameHeader[2], &len, payloadFieldExtraBytes);
        }


        uint32_t frameSize = frameHeaderSize + messageLength;
        char *frame = new char[frameSize + 1];
        memcpy(frame, frameHeader, frameHeaderSize);
        memcpy(frame + frameHeaderSize, inMessage.c_str(), messageLength);
        frame[frameSize] = '\0';
        outFrame = frame;

        delete[] frame;
        delete[] frameHeader;
        ret = frameType;
        return ret;
    }
}

#endif /* WEBSOCKET_HPP */

