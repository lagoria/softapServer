#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "esp_log.h"
#include "app_cjson.h"

#define TAG     "app_cjson"


// num键值对个数，创建可变参数列表的cjson字符串
char* create_json_string(int num, ...) 
{
    va_list args;
    va_start(args, num);

    cJSON *root = cJSON_CreateObject();  // 创建cJSON对象
    for (int i = 0; i < num; i++) {
        const char* key = va_arg(args, const char*);
        const char* value = va_arg(args, const char*);
        cJSON_AddStringToObject(root, key, value);  // 添加键值对
    }

    va_end(args);

    char* json_str = cJSON_PrintUnformatted(root);  // 生成json字符串
    cJSON_Delete(root);  // 释放cJSON对象

    return json_str;
}


//在cjson数据中搜索感兴趣的键值(字符串)
char* get_json_value(const char* json_str, const char* key) 
{
    cJSON *root = cJSON_Parse(json_str);  // 解析JSON字符串

    cJSON *target = cJSON_GetObjectItem(root, key);
    char *value = NULL;
    if(target != NULL && target->type == cJSON_String) {
        value = malloc(strlen(target->valuestring) + 1);
        if(value != NULL) {
            strncpy(value, target->valuestring, strlen(target->valuestring));
            value[strlen(target->valuestring)] = '\0';  // 确保字符串结尾是NULL字符
        }
    }

    cJSON_Delete(root);  // 释放cJSON对象

    return value;
}



