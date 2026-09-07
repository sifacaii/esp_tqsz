#ifndef WEBCONFIG_H
#define WEBCONFIG_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <DNSServer.h>
#include "config_storage.h"

#define WIFI_CONFIG_AP_SSID "ESP8266-Config"
#define WIFI_CONFIG_AP_PASS "12345678"

static ESP8266WebServer webServer(80);
static ESP8266HTTPUpdateServer httpUpdater;
static DNSServer dnsServer;
static bool captivePortal = false; /* 是否处于AP强制门户模式 */

String buildConfigPage(const String &ssid, const String &pass) {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>WiFi配置</title><style>"
    "body{font-family:sans-serif;padding:20px;max-width:400px;margin:auto}"
    "h2{text-align:center}label{font-size:14px}input{width:100%;padding:10px;"
    "margin:5px 0 15px;box-sizing:border-box;font-size:16px}"
    "button{width:100%;padding:12px;background:#4caf50;color:#fff;border:none;"
    "border-radius:4px;font-size:16px}</style></head><body>"
    "<h2>WiFi 配置</h2>"
    "<form method='post'>"
    "<label>WiFi名称(SSID)</label><br>"
    "<input name='ssid' value='" + ssid + "'><br>"
    "<label>WiFi密码</label><br>"
    "<input name='pass' type='password' value='" + pass + "'><br>"
    "<button type='submit'>保存并连接</button>"
    "</form>"
    "<p style='text-align:center;margin-top:20px'>"
    "<a href='/update' style='color:#4caf50'>固件OTA更新</a></p>"
    "</body></html>";
  return html;
}

void handleConfigRoot() {
  String ssid, pass;
  loadWiFiConfig(ssid, pass);
  webServer.send(200, "text/html", buildConfigPage(ssid, pass));
}

void handleConfigRootPost() {
  String ssid = webServer.arg("ssid");
  String pass = webServer.arg("pass");
  ssid.trim();
  pass.trim();

  saveWiFiConfig(ssid, pass);
  webServer.send(200, "text/html",
    "<!DOCTYPE html><html><head><meta charset='utf-8'></head>"
    "<body><h3 style='text-align:center;margin-top:40px'>配置已保存，正在重启并连接...</h3></body></html>");
  delay(100);
  ESP.restart();
}

/* 未请求的任意域名重定向到配置页（兼容设备验证/强制门户） */
void handleConfigNotFound() {
  String ip = captivePortal ? WiFi.softAPIP().toString()
                            : WiFi.localIP().toString();
  webServer.sendHeader("Location", "http://" + ip + "/", true);
  webServer.send(302, "text/html", "");
}

/* 启动Web配置服务器（任意模式下都可调用） */
void initWebServer() {
  webServer.on("/", HTTP_GET, handleConfigRoot);
  webServer.on("/", HTTP_POST, handleConfigRootPost);
  webServer.onNotFound(handleConfigNotFound);

  /* OTA固件更新，访问 http://<ip>/update 上传固件 */
  httpUpdater.setup(&webServer);

  webServer.begin();
  Serial.println("Web配置服务已启动，OTA更新地址: /update");
}

/* 启动AP热点与强制门户（仅配置模式） */
void initWebConfig() {
  captivePortal = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_CONFIG_AP_SSID, WIFI_CONFIG_AP_PASS);

  dnsServer.start(53, "*", WiFi.softAPIP());

  Serial.println("配置模式已启动，请连接AP: " + String(WIFI_CONFIG_AP_SSID) + " 并访问 192.168.4.1");
}

/* 在loop中调用以处理Web/DNS请求 */
void handleWebConfig() {
  if (captivePortal) {
    dnsServer.processNextRequest();
  }
  webServer.handleClient();
}

#endif
