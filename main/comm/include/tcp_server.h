#pragma once

#include "bufdef.h"

namespace TcpServer {

struct ClientInfo {
    /* 自动获取 */
    int socket;                 // 套接字
    uint8_t id;                 // 客户端设备标识码
    char ip[16];                // 客户端ip
    int  port;                  // 客户端端口
    /* 手动获取 */
    char name[32];              // 客户端名
    struct ClientInfo *next;
};

using RecvCallback = void (*)(int, IBuf);
constexpr uint16_t SOCK_BUF_SIZE = 1024;

int init(uint16_t port);
void registerRecvCallback(RecvCallback cb);
ClientInfo* getClientsInfo();
int getSourceSock();

}
