#include "cmds.h"
#include "utility.h"
#include "tcp_server.h"

#include "esp_log.h"
#include <cstring>

namespace cmds {

constexpr static char TAG[] = "cmds";

#define CMD_ASSERT(condition) do { if (!(condition)) { return Utility::snprint("assert failed, condition '" #condition "'"); } } while (0)

#define CMD_SWITCH(...) \
CMD_ASSERT(argc >= 1); \
switch (Utility::BKDR_hash(argv[0])) { \
	__VA_ARGS__ \
	default: return Utility::snprint("unknown option '%s'", argv[0]); \
}

#define CMD_CASE_REUSE(name1, name2) \
case #name1##_hash: { \
	CMD_ASSERT(argc >= 1); \
	return Utility::snprint("%s,", argv[0]) + cmd_##name2(argc - 1, argv + 1); \
}

#define CMD_CASE_ROOT(name) CMD_CASE_REUSE(name, name)

#define CMD_CASE(name, ...) \
case #name##_hash: { \
	CMD_ASSERT(argc >= 1); \
	return Utility::snprint("%s,", argv[0]) + ([](int argc, char* argv[]) { __VA_ARGS__ })(argc - 1, argv + 1) + OBuf(1, ','); \
} break;

#define CMD_EXPR(name, ...) \
case #name##_hash: { \
	return Utility::snprint("%s,", #name) + Utility::snprint(__VA_ARGS__) + OBuf(1, ','); \
}


// void client_cmd_handle(tcp_socket_info_t *handle, char *recv_data, client_protocol_t protocol)
// {
//     char *respond = NULL;
//     client_evt_t handle_evt = CLIENT_UNKNOWN_EVT;
//     TcpServer::ClientInfo *current_client = TcpServer::getClientsInfo();
//     /* 进行身份注册检测 */
//     for ( ; current_client; current_client = current_client->next) {
//         if (current_client->socket == handle->socket) {
//             break;
//         }
//     }
//     if (current_client == NULL) return;

//     cJSON *root = cJSON_Parse((char *)recv_data);  // 解析JSON字符串
//     cJSON *cmd = cJSON_GetObjectItem(root, CMD_KEY_COMMAND);
//     cJSON *transmit = cJSON_GetObjectItem(root, CMD_KEY_TRANSMIT);

//     /* 数据转发判断 */
//     if (transmit != NULL && transmit->type == cJSON_String) {
//         handle_evt = CLIENT_TRANSMIT_EVT;
//     } else if (cmd != NULL && cmd->type == cJSON_String) {
//         if (strcmp(cmd->valuestring, CMD_STRING_REGISTER) == 0) {
//             /* 注册信息 */
//             handle_evt = CMD_REGISTER_EVT;
//         } else if (strcmp(cmd->valuestring, CMD_STRING_LIST) == 0) {
//             /* 客户端信息列表指令 */
//             handle_evt = CMD_LIST_EVT;
//         } else if (strcmp(cmd->valuestring, CMD_STRING_ROUTER) == 0) {
//             /* 修改路由器帐号信息指令 */
//             handle_evt = CMD_ROUTER_EVT;
//         } else if (strcmp(cmd->valuestring, CMD_STRING_UUID) == 0) {
//             /* 获取目标客户端ID */
//             handle_evt = CMD_UUID_EVT;
//         }
//     }

//     if (current_client->name[0] == 0 && handle_evt != CMD_REGISTER_EVT) {
//         // 未注册事件
//         handle_evt = CMD_UNREGISTER_EVT;
//     }
    
//     switch (handle_evt) {
//     case CMD_UNREGISTER_EVT : {
//         /* 未注册回应 */
//         respond = create_json_string(1, CMD_KEY_STATUS, "unregister");
//         break;
//     }
//     case CMD_REGISTER_EVT : {
//         ESP_LOGI(TAG, "device info register");
//         cJSON *name = cJSON_GetObjectItem(root, CMD_KEY_NAME);
//         if (name != NULL && name->type == cJSON_String) {
//             // 注册记录客户端设备名
//             strncpy(current_client->name, name->valuestring, 32);

//             respond = create_json_string(1, CMD_KEY_STATUS, "succeed");
//         }
//         break;
//     }
//     case CMD_LIST_EVT : {
//         // 回应客户端信息列表
//         char sock_str[8] = {0};
//         char port_str[8] = {0};
//         char id_str[8] = {0};
//         for (TcpServer::ClientInfo *list = TcpServer::getClientsInfo(); list; list = list->next) {
//             sprintf(sock_str, "%d", list->socket);
//             sprintf(id_str, "%d", list->id);
//             sprintf(port_str, "%d", list->port);
//             respond = create_json_string(5, "name", list->name,
//                                                 "ip", list->ip, 
//                                                 "port", port_str, 
//                                                 "sock", sock_str,
//                                                 "id", id_str);
//             data_frame_send(handle->socket, current_client->id, strlen(respond) + 1, respond, protocol);
//             free(respond);
//         }
//         respond = NULL;
        
//         break;
//     }
//     case CMD_UUID_EVT : {
//         /* 获取目标设备ID */
//         TcpServer::ClientInfo *list = NULL;
//         cJSON *name = cJSON_GetObjectItem(root, CMD_KEY_NAME);
//         if (name != NULL && name->type == cJSON_String) {
//             for (list = TcpServer::getClientsInfo(); list; list = list->next) {
//                 if(strcmp (list->name, name->valuestring) == 0) {
//                     break;
//                 }
//             }
//         }
//         if (list == NULL) {
//             respond = create_json_string(1, CMD_KEY_STATUS, "failed");
//         } else {
//             char id_str[8];
//             snprintf(id_str, 8, "%d", list->id);
//             respond = create_json_string(1, CMD_KEY_ID, id_str);
//         }

//         break;
//     }
//     case CMD_ROUTER_EVT: {
//         /* 设置wifi STA的路由器帐号 */
//         cJSON *ssid = cJSON_GetObjectItem(root, CMD_KEY_SSID);
//         cJSON *pwd = cJSON_GetObjectItem(root, CMD_KEY_PWD);
//         if (ssid != NULL && pwd != NULL) {
//             wifi_wrapper_account_cfg_t wifi_account;
//             strncpy((char *)wifi_account.ssid, ssid->valuestring, sizeof(wifi_account.ssid));
//             strncpy((char *)wifi_account.password, pwd->valuestring, sizeof(wifi_account.password));
//             if (wifi_wrapper_nvs_store_account(&wifi_account)) {
//                 ESP_LOGE(TAG, "nvs store failed!");
//                 respond = create_json_string(1, CMD_KEY_STATUS, "failed");
//             } else {
//                 respond = create_json_string(1, CMD_KEY_STATUS, "succeed");
//             }
//         }
//         break;
//     }
//     case CLIENT_TRANSMIT_EVT : {
//         /* 数据转发事件 */
//         ESP_LOGD(TAG, "transmit len %d", handle->len);
//         uint8_t target = (uint8_t )atoi(transmit->valuestring);
//         TcpServer::ClientInfo *list;
//         for (list = TcpServer::getClientsInfo(); list; list = list->next) {
//             if (list->id == target) {
//                 /* transmit */
//                 SocketWrapper::socketSend(list->socket, handle->data, handle->len);
//                 break;
//             }
//         }

//         if (list != NULL) {
//             respond = create_json_string(1, CMD_KEY_STATUS, "succeed");
//         } else {
//             respond = create_json_string(1, CMD_KEY_STATUS, "failed");
//         }
//         break;
//     }
//     case CLIENT_NOT_FOUND_EVT : {
//         ESP_LOGW(TAG, "client target not found");
//         respond = create_json_string(1, CMD_KEY_STATUS, "failed");
//         break;
//     }
//     case CLIENT_FRAME_INVALID_EVT : {
//         ESP_LOGW(TAG, "[sock=%d]: data frame invalid", handle->socket);
//         respond = create_json_string(1, CMD_KEY_STATUS, "invalid");
//         break;
//     }
//     case CLIENT_UNKNOWN_EVT : {
//         ESP_LOGW(TAG, "unknown event");
//         respond = create_json_string(1, CMD_KEY_STATUS, "unknown");
//         break;
//     }
    
//     default : break;
//     }

//     cJSON_Delete(root);

//     if (respond == NULL) return;
//     data_frame_send(handle->socket, current_client->id, strlen(respond) + 1, respond, protocol);
//     free(respond);
// }


OBuf cmd_login(int argc, char* argv[]) {
	ESP_LOGI(TAG, "device info register");
	CMD_ASSERT(argc == 1);
	std::string respond = "{\"status\":\"failed\"}";
	TcpServer::ClientInfo *current_client = TcpServer::getClientsInfo();
    for ( ; current_client; current_client = current_client->next) {
        if (current_client->socket == TcpServer::getSourceSock()) {
			// 注册记录客户端设备名
			strncpy(current_client->name, argv[0], 32);
			respond = std::string("{\"status\":\"succeed\"}");
            break;
        }
    }

	return Utility::snprint("%s", respond.data());
}


OBuf default_cmd_bundle(int argc, char* argv[]) {
	CMD_SWITCH(
		CMD_CASE_ROOT(login);
	);
}

OBuf call( int argc, char* argv[]) {
	return default_cmd_bundle(argc, argv) + OBuf(1, '\n');
}

}
