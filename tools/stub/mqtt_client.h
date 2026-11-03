#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_event.h"

typedef struct esp_mqtt_client *esp_mqtt_client_handle_t;

typedef enum {
    MQTT_EVENT_ANY = -1,
    MQTT_EVENT_ERROR = 0,
    MQTT_EVENT_CONNECTED,
    MQTT_EVENT_DISCONNECTED,
    MQTT_EVENT_SUBSCRIBED,
    MQTT_EVENT_UNSUBSCRIBED,
    MQTT_EVENT_PUBLISHED,
    MQTT_EVENT_DATA,
    MQTT_EVENT_BEFORE_CONNECT,
    MQTT_EVENT_DELETED,
} esp_mqtt_event_id_t;

typedef struct esp_mqtt_event_t {
    esp_mqtt_event_id_t event_id;
    esp_mqtt_client_handle_t client;
    void *user_context;
    char *data;
    int data_len;
    int total_data_len;
    int current_data_offset;
    char *topic;
    int topic_len;
    int msg_id;
    int retain;
    int qos;
} esp_mqtt_event_t;

typedef esp_mqtt_event_t *esp_mqtt_event_handle_t;

typedef struct {
    struct {
        struct {
            const char *uri;
        } address;
    } broker;
    struct {
        const char *client_id;
        const char *username;
        struct {
            const char *password;
        } authentication;
    } credentials;
    struct {
        int keepalive;
        struct {
            int timeout_ms;
        } last_will;
    } session;
    struct {
        int reconnect_timeout_ms;
        bool disable_auto_reconnect;
        int network_timeout_ms;
    } network;
} esp_mqtt_client_config_t;

esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t *config);
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client,
                                         esp_mqtt_event_id_t event,
                                         esp_event_handler_t event_handler, void *event_handler_arg);
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client);
esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t client);
int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client, const char *topic, int qos);
int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data,
                            int len, int qos, int retain);
