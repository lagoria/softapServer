#pragma once

#include "bufdef.h"

namespace TcpDataHandle {

/* --------------------------------
1 byte          FRAME_HEADER        帧头
1 byte          uint8_t             帧类型
1 byte          uint8_t             目标设备ID
1 byte          uint8_t             发送方设备ID
4 byte          uint32_t            数据长度
......                              数据
-------------------------------- */

constexpr uint8_t FRAME_HEAD = 0xAA;        // 帧头标志(1010 1010)
constexpr uint8_t SERVER_ID  = 1;           // 服务器ID
constexpr char KEY_STATUS[]  = "status";
constexpr char STATUS_OK[]   = "succeed";
constexpr char STATUS_FAIL[] = "failed";

/* date frame type decline */
enum FrameType : uint8_t {
    UNKNOWN = 0,            // 未知格式
    JSON,                   // JSON字符串格式
    BINARY,                 // 二进制数据格式
    CMD,                    // 命令数据格式
};

struct FrameHeader {
    uint8_t head;
    FrameType type;
    uint8_t goal;
    uint8_t source;
    uint16_t length;
};

int init();

FrameHeader frameUnpack(IBuf& buf, OBuf& out);

int packageSend(uint8_t goal, FrameType type, IBuf buf);

void response(int sock, IBuf info);

}