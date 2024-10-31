#include "tcp_data_handle.h"
#include "tcp_server.h"
#include "socket_wrapper.h"
#include "json_wrapper.h"
#include "shell_wrapper.h"
#include "utility_wrapper.h"

#include "esp_log.h"
#include "esp_heap_caps.h"
#include <cstring>

namespace TcpDataHandle {

static const char TAG[] = "tcp_data_handle";
static uint8_t* _tx_buffer = nullptr;

FrameHeader frameUnpack(IBuf& buf, OBuf& out) {
    FrameHeader frame;
    memset(&frame, 0, sizeof(frame));

    /* data frame parse*/
    frame = *(FrameHeader *)buf.data();
    if (frame.head != TcpDataHandle::FRAME_HEAD || frame.length != (buf.size() - sizeof(FrameHeader))) {
        out.clear();
        frame.type = FrameType::UNKNOWN;
        return frame;
    }

    out = buf.substr(sizeof(FrameHeader));
    return frame;
}

int packageSend(uint8_t goal, FrameType type, IBuf buf) {
    TcpServer::ClientInfo* client = TcpServer::getClientsInfo();
    if (_tx_buffer == nullptr) return -1;
    for ( ; client; client = client->next) {
        if (client->id == goal) {
            break;
        }
    }
    if (client == nullptr) return -2;
    
    _tx_buffer[0] = TcpDataHandle::FRAME_HEAD;
    _tx_buffer[1] = type;
    _tx_buffer[2] = goal;
    _tx_buffer[3] = 1;
    *(uint16_t *)&_tx_buffer[4] = buf.size();
    memcpy(&_tx_buffer[6], buf.data(), buf.size());

    return Wrapper::Socket::send(client->socket, _tx_buffer, buf.size() + sizeof(FrameHeader));
}

int packageRespond(int sock, FrameType type, IBuf buf) {
    TcpServer::ClientInfo* client = TcpServer::getClientsInfo();
    if (_tx_buffer == nullptr) return -1;
    for ( ; client; client = client->next) {
        if (client->socket == sock) {
            break;
        }
    }
    if (client == nullptr) return -2;
    
    _tx_buffer[0] = TcpDataHandle::FRAME_HEAD;
    _tx_buffer[1] = type;
    _tx_buffer[2] = client->id;
    _tx_buffer[3] = 1;
    *(uint16_t *)&_tx_buffer[4] = buf.size();
    memcpy(&_tx_buffer[6], buf.data(), buf.size());

    return Wrapper::Socket::send(sock, _tx_buffer, buf.size() + sizeof(FrameHeader));
}

OBuf json_string_parse(IBuf buf) {
    std::string json_str = (char *)buf.data();
    Wrapper::JsonObject json(json_str);
    OBuf out;
    if (!json.isObject()) {
        ESP_LOGE(TAG, "json parse failed.");
        return out;
    }

    if (json["cmd"].isString()) {
        out = Wrapper::Utility::snprint("%s", json["cmd"].getString().data());
    }
    if (json["args"].isArray()) {
        for (int i = 0; i < json["args"].getArraySize(); i++) {
            out += Wrapper::Utility::snprint(" %s", json["args"][i].getString().data());
        }
    }

    return out;
}

void response(int sock, IBuf info) {
    OBuf buf;
    OBuf out;
    FrameHeader frame = frameUnpack(info, buf);
    if (frame.type == FrameType::UNKNOWN) {
        ESP_LOGE(TAG, "unknown frame.");
        return;
    }
    if (frame.goal != SERVER_ID) {
        /* 桢数据转发 */
        for (TcpServer::ClientInfo *list = TcpServer::getClientsInfo(); list; list = list->next) {
            if (list->id == frame.goal) {
                /* transmit */
                Wrapper::Socket::send(list->socket, info.data(), info.size());
                break;
            }
        }
    } else {
        ESP_LOGI(TAG, "type: %d", frame.type);
        switch (frame.type) {
        case FrameType::JSON:
            out = Wrapper::Shell::response(json_string_parse(buf.data()));
            break;
        case FrameType::CMD:
            out = Wrapper::Shell::response(buf);
            break;
        default:
            out = Wrapper::Utility::snprint("unknown frame type");
            break;
        }
        // send response
        packageRespond(sock, FrameType::CMD, out);
    }
}

int init() {
    /* allocation sock date buffer */
    _tx_buffer = (uint8_t *)heap_caps_malloc(TcpServer::SOCK_BUF_SIZE + sizeof(FrameHeader), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (_tx_buffer == NULL) {
        ESP_LOGE(TAG, "tx buffer malloc failed");
        return -1;
    }
    return 0;
}

}
