#ifndef APP_CJSON_H
#define APP_CJSON_H

#include "cJSON.h"


/*-----------------------------------------------------*/

// num键值对个数，创建可变参数列表的cjson字符串
char* create_json_string(int num, ...);

// 在cjson数据中搜索感兴趣的键值(字符串)
char* get_json_value(const char* json_str, const char* key);




#endif