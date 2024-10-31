#include "cmds.h"
#include "app_config.h"
#include "utility_wrapper.h"
#include "json_wrapper.h"
#include "wifi_wrapper.h"
#include "tcp_server.h"

#include "esp_log.h"
#include <cstring>

namespace cmds {

constexpr static char TAG[] = "cmds";

#define CMD_ASSERT(condition) do { if (!(condition)) { return Wrapper::Utility::snprint("assert failed, condition '" #condition "'"); } } while (0)

#define CMD_SWITCH(...) \
CMD_ASSERT(argc >= 1); \
switch (Wrapper::Utility::BKDR_hash(argv[0])) { \
	__VA_ARGS__ \
	default: return Wrapper::Utility::snprint("unknown option '%s'", argv[0]); \
}

#define CMD_CASE_REUSE(name1, name2) \
case #name1##_hash: { \
	CMD_ASSERT(argc >= 1); \
	return cmd_##name2(argc - 1, argv + 1); \
}

#define CMD_CASE_ROOT(name) CMD_CASE_REUSE(name, name)


static OBuf cmd_list(int argc, char* argv[]) {
	// 回应客户端信息列表
	Wrapper::JsonObject json;
	json.setArray();
	for (TcpServer::ClientInfo *list = TcpServer::getClientsInfo(); list; list = list->next) {
		Wrapper::JsonObject item;
		item.add("name", list->name);
		item.add("ip", list->ip);
		item.add("port", list->port);
		item.add("sock", list->socket);
		item.add("mark", list->id);
		json.addArray(item);
	}
	return Wrapper::Utility::snprint("%s", json.serialize().data());
}

static OBuf cmd_mark(int argc, char* argv[]) {
	/* 获取目标设备ID */
	CMD_ASSERT(argc == 1);
	Wrapper::JsonObject json;
	TcpServer::ClientInfo *list = nullptr;
	for (list = TcpServer::getClientsInfo(); list; list = list->next) {
		if(strcmp (list->name, argv[0]) == 0) {
			break;
		}
	}
	if (list == nullptr) {
		json.add(AppCfg::JSON_KEY_MARK, 0);
	} else {
		json.add(AppCfg::JSON_KEY_MARK, list->id);
	}
	return Wrapper::Utility::snprint("%s", json.serialize().data());
}

static OBuf cmd_wifi(int argc, char* argv[]) {
	/* 设置wifi STA的路由器帐号 */
	CMD_ASSERT(argc == 2);
	Wrapper::JsonObject json;
	Wrapper::WiFi::State state = Wrapper::WiFi::Apsta::provision(argv[0], argv[1]);
	if (state == Wrapper::WiFi::State::CONNECTED) {
		json.add("status", "succeed");
	} else {
		json.add("status", "failed");
	}

	return Wrapper::Utility::snprint("%s", json.serialize().data());
}

static OBuf cmd_login(int argc, char* argv[]) {
	ESP_LOGI(TAG, "device info register");
	CMD_ASSERT(argc == 1);
	Wrapper::JsonObject json;
	std::string respond = "failed";
	TcpServer::ClientInfo *current_client = TcpServer::getClientsInfo();
    for ( ; current_client; current_client = current_client->next) {
        if (current_client->socket == TcpServer::getSourceSock()) {
			// 注册记录客户端设备名
			strncpy(current_client->name, argv[0], 32);
			respond = std::string("succeed");
            break;
        }
    }
	json.add("status", respond.data());

	return Wrapper::Utility::snprint("%s", json.serialize().data());
}


OBuf default_cmd_bundle(int argc, char* argv[]) {
	CMD_SWITCH(
		CMD_CASE_ROOT(login);
		CMD_CASE_ROOT(wifi);
		CMD_CASE_ROOT(mark);
		CMD_CASE_ROOT(list);
	);
}

OBuf call( int argc, char* argv[]) {
	return default_cmd_bundle(argc, argv) + OBuf(1, '\n');
}

}
