#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include "CAN_Interface.h"
#include "websocket_config.h"
#include "can_driver_handler.h"
#include "nvm.h"

extern Nvm config_persistor;

// https://stackoverflow.com/questions/3266580/extern-struct-in-c
// extern struct cannelloni_config_t config;

/* ------------ Websocket ---------------*/
#ifdef __USE_WEBSOCKET_CONFIG__
WiFiServer web_server(WEBSOCKET_PORT);
WebSocketServer websocket_server;
static TaskHandle_t websocket_recv_task_handle;
int udp_socket;
IPAddress ap_ip;


/* ------------ HTTP ------------- */
#ifdef __USE_HTTP_CONFIG__
const char* html =
#include "html.h"
  ;

WiFiServer webui_server(HTTP_PORT);
static TaskHandle_t http_task_handle;

void http() {
    WiFiClient client;
    webui_server.begin();
    String header;
    while (1) {
        client = webui_server.available();
        if (client.connected()) {
            Serial.println("Client connected...");
            String current_line = "";
            while (client.connected()) {
                if (client.available()) {
                    char c = client.read();
                    header += c;
                    if (c == '\n') {
                        if (current_line.length() == 0) {
                            client.println("HTTP/1.1 200 OK");
                            client.println("Content-type:text/html");
                            client.println("Connection: close");
                            client.println();

                            client.println(html);
                            break;
                        }
                        else {
                            current_line = "";
                        }
                    }
                    else if (c != '\r') {
                        current_line += c;
                    }
                }
            }
            Serial.println(header);

            header = "";
            client.stop();
            Serial.println("Client disconnected...");
            Serial.println("");
        }
        else {
            delay(200);
        }
    }
}

void http_task(void *pv_parm) {
    http();
    vTaskDelete(NULL);
}

void http_start() {
    xTaskCreate(http_task, "http_task", 15000, NULL, 1, &http_task_handle); // 2
}

void http_stop() {
    vTaskDelete(http_task_handle);
}

#endif //  __USE_HTTP_CONFIG__


void handle_recv_message(String message) {
    StaticJsonDocument<500> json_buffer; /* update to version 6 - https://arduinojson.org/v6/doc/upgrade/ */ 

    auto error = deserializeJson(json_buffer, message);

    if (error) {
        Serial.print(F("deserializeJson() failed with code "));
        Serial.println(error.c_str());
        return;
}

    int bitrate = json_buffer["bitrate"];
    settings_write("bitrate", &config, &bitrate);

    int can_mode = json_buffer["can_mode"];
    settings_write("can_mode", &config, &can_mode);

    int filter = json_buffer["filter"];
    settings_write("filter", &config, &filter);

    int is_extended = json_buffer["is_extended"];
    settings_write("is_extended", &config, &is_extended);

    int start_id = json_buffer["start_id"];
    settings_write("start_id", &config, &start_id);

    int end_id = json_buffer["end_id"];
    settings_write("end_id", &config, &end_id);

    Serial.println();
    Serial.println("----- NEW DATA FROM CLIENT ----");

    Serial.print("bitrate: ");
    Serial.println(bitrate);

    Serial.print("can_mode: ");
    Serial.println(config.can_mode);

    Serial.print("filter: ");
    Serial.println(config.filter);

    Serial.print("is extended: ");
    Serial.println(config.is_extended);

    Serial.print("start_id: ");
    Serial.println(config.start_id);

    Serial.print("end_id: ");
    Serial.println(config.end_id);

    Serial.println("------------------------------");
    config_persistor.require_update();
    //writeNvm();
    setup_can_driver(&config);

}

void websocket_recv() {
    web_server.begin();
    WiFiClient client;
    while (1) {
        client = web_server.available();
        if (client.connected() && websocket_server.handshake(client)) {
            String data;

            while (client.connected()) {
                data = websocket_server.getData();
                if (data.length() > 0) {
                    handle_recv_message(data);
                }
                delay(10);
            }
            Serial.println("The client disconnected");
            delay(100);
        }
        delay(100);
    }
}

void websocket_recv_task(void *pv_parm) {
    websocket_recv();
    vTaskDelete(NULL);
}

void websocket_start() {
    xTaskCreate(websocket_recv_task, "websocket_recv_task", 15000, NULL, 1, &websocket_recv_task_handle);
}

void websocket_stop() {
    vTaskDelete(websocket_recv_task_handle);
}
#endif // __USE_WEBSOCKET_CONFIG__

/* ------------ WIFI ------------ */
static struct sockaddr_in local_address;

void setup_socket() {
    if ((udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        Serial.println("count not create socket");
        return;
    }

    int yes = 1;
    if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        Serial.printf("could not set socket option: %d", errno);
        Serial.println();
        return;
    }

    memset((char *) &local_address, 0, sizeof(local_address));
    local_address.sin_family = AF_INET;
    local_address.sin_len = sizeof(local_address);
    local_address.sin_port = htons(LOCAL_PORT);
    local_address.sin_addr.s_addr = (in_addr_t)ap_ip;

    if (bind(udp_socket, (struct sockaddr*)&local_address, sizeof(local_address)) == -1) {
        Serial.printf("could not bind socket: %d", errno);
        Serial.println();
        return;
    }
}

void setup_ap() {
    const char * ssid = SSID;
    const char * pswd = PSWD;

    // delete the old configuration
    WiFi.disconnect(true);

    Serial.println("Setting AP [Access Point]...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(ssid, pswd);
    ap_ip = WiFi.softAPIP();
    Serial.println("AP IP-Address: ");
    Serial.println(ap_ip);
}
