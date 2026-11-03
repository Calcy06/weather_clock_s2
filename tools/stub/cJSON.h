#pragma once

typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;
    int type;
    char *valuestring;
    int valueint;
    double valuedouble;
    char *string;
} cJSON;

#define cJSON_Invalid 0
#define cJSON_False 1
#define cJSON_True 2
#define cJSON_NULL 4
#define cJSON_Number 8
#define cJSON_String 16
#define cJSON_Array 32
#define cJSON_Object 64

cJSON *cJSON_Parse(const char *value);
void cJSON_Delete(cJSON *item);
cJSON *cJSON_GetObjectItem(const cJSON *object, const char *string);
cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *object, const char *string);
cJSON *cJSON_GetArrayItem(const cJSON *array, int index);
int cJSON_GetArraySize(const cJSON *array);

#define cJSON_IsString(x) ((x) != 0 && (x)->type == cJSON_String)
#define cJSON_IsNumber(x) ((x) != 0 && (x)->type == cJSON_Number)
#define cJSON_IsObject(x) ((x) != 0 && (x)->type == cJSON_Object)
#define cJSON_IsArray(x) ((x) != 0 && (x)->type == cJSON_Array)
