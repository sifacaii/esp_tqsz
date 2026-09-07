#include "WString.h"
#include "config_storage.h"
#include "webconfig.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <U8g2lib.h>
#include <WiFiClientSecure.h>


#include "Wire.h"
#include <SHT2x.h>

#define SDA_PIN 0 /* 屏幕数据线 */
#define SCL_PIN 2 /* 屏幕时钟线 */

/* 心知天气API密钥 */
String weatherUrl = "https://api.seniverse.com/v3/weather/now.json?";
String weatherApiKey = ""; /* 心知天气API密钥 */
String weatherLocation = ""; /* 心知天气API位置 */

/* 星期数组 */
String weekArray[] = {"日", "一", "二", "三", "四", "五", "六"};

/* 天气数据缓存 */
String weatherText = "--";   /* 天气缓存 */
String weatherTemp = "--";   /* 温度缓存 */
time_t lastWeatherFetch = 0; /* 上次天气数据刷新时间 */

float localTemp = 0.0f;        /* 本地温度缓存 */
float localHumidity = 0.0f;    /* 本地湿度缓存 */
time_t lastLocalTempFetch = 0; /* 上次本地温度数据刷新时间 */

SHT2x sht2x; /* HTU20D 温湿度传感器 */

U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R2, SCL_PIN, SDA_PIN,
                                         U8X8_PIN_NONE);

bool isShowPoint = true; /* 是否显示时间点 */
bool configMode = false; /* 配置模式 */
time_t lastNtpSync = 0;  /* 上次NTP同步时间 */

/* 从阿里云NTP服务器同步时间 */
void syncNtp() {
  configTime(8 * 3600, 0, "ntp.aliyun.com", "ntp1.aliyun.com",
             "ntp2.aliyun.com");
  Serial.print("正在同步NTP...");
  time_t now = time(nullptr);
  int retry = 0;
  while (now < 100000 && retry < 40) {
    delay(500);
    retry++;
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();

  if (now > 100000) {
    lastNtpSync = now;
    struct tm *t = localtime(&now);
    Serial.print("NTP同步成功: ");
    Serial.printf("%04d-%02d-%02d %02d:%02d:%02d\n", t->tm_year + 1900,
                  t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
  } else {
    Serial.println("NTP同步失败");
  }
}

/* 从心知天气API获取天气和气温 */
void fetchWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi未连接，无法获取天气");
    return;
  }

  String url = weatherUrl + "key=" + weatherApiKey +
               "&location=" + weatherLocation + "&language=zh-Hans&unit=c";

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (!http.begin(client, url)) {
    Serial.println("天气请求初始化失败");
    return;
  }

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (!err) {
      const char *text = doc["results"][0]["now"]["text"];
      float temp = doc["results"][0]["now"]["temperature"];

      weatherText = text ? String(text) : "--";
      weatherTemp = String((int)temp) + "°C";

      Serial.print("天气: ");
      Serial.println(weatherText + " " + weatherTemp);
    } else {
      Serial.print("天气JSON解析失败: ");
      Serial.println(err.c_str());
    }
  } else {
    Serial.print("天气HTTP错误: ");
    Serial.println(httpCode);
  }

  http.end();
  lastWeatherFetch = time(nullptr);
}

void fetchLocalTemp() {
  sht2x.read();
  delay(100);
  localTemp = sht2x.getTemperature();
  localHumidity = sht2x.getHumidity();
}

/* 初始化OTA无线升级 */
void initOta() {
  ArduinoOTA.setHostname("esp-clock");

  ArduinoOTA.onStart([]() {
    Serial.println("OTA更新开始");
    /* 断开阻塞服务的HTTP/DNS，释放资源 */
    WiFi.disconnect();
  });
  ArduinoOTA.onEnd([]() { Serial.println("OTA更新结束"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA进度: %u%%\r", (progress * 100) / total);
  });
  ArduinoOTA.onError(
      [](ota_error_t error) { Serial.printf("OTA错误[%u]\n", error); });

  ArduinoOTA.begin();
  Serial.println("OTA已就绪");
}

/* 在屏幕上显示天气、时间与日期 */
void updateClockDisplay() {
  time_t now = time(nullptr);
  if (now < 100000) {
    return;
  }
  struct tm *t = localtime(&now);

  static uint8_t lastSec = 0;
  uint8_t sec = t->tm_sec;
  if (sec == lastSec) {
    return;
  }
  lastSec = sec;

  String hour = t->tm_hour < 10 ? "0" + String(t->tm_hour) : String(t->tm_hour);
  String min = t->tm_min < 10 ? "0" + String(t->tm_min) : String(t->tm_min);
  String timeStr = hour + (isShowPoint ? ":" : " ") + min;
  // timeStr = String(t->tm_hour)+ (isShowPoint ? ":" : " ")+String(t->tm_min);
  isShowPoint = !isShowPoint;

  String month =
      t->tm_mon < 10 ? "0" + String(t->tm_mon + 1) : String(t->tm_mon + 1);
  String day = t->tm_mday < 10 ? "0" + String(t->tm_mday) : String(t->tm_mday);
  String dateStr = String(t->tm_year + 1900) + "-" + month + "-" + day;
  /* 星期 */
  String week = "星期" + weekArray[t->tm_wday];

  u8g2.clearBuffer();
  /* 天气 */
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.setCursor(0, 15);
  u8g2.print(weatherText);
  u8g2.setCursor(0, 31);
  u8g2.print(weatherTemp);

  /* 时间 */
  u8g2.setFont(u8g2_font_freedoomr25_tn);
  u8g2.setCursor(41, 31);
  u8g2.print(timeStr);

  /* 本地温湿度 */
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.setCursor(0, 47);
  u8g2.print("温度:" + String(localTemp, 1) + "℃");
  u8g2.setCursor(65, 47);
  u8g2.print("湿度:" + String(localHumidity, 1) + "%");

  /* 日期与星期 */
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.setCursor(0, 63);
  u8g2.print(dateStr);
  u8g2.setCursor(90, 63);
  u8g2.print(week);
  u8g2.sendBuffer();
}

/* 从EEPROM读取并连接WiFi；失败或无配置时进入Web配置模式 */
void connectWifi() {
  String ssid, pass;
  loadWiFiConfig(ssid, pass);

  Serial.print("WiFi SSID: ");
  Serial.println(ssid);

  if (ssid.length() == 0) {
    Serial.println("EEPROM中无SSID，进入配置模式");
    initWebConfig();
    configMode = true;
    u8g2.clearBuffer();
    u8g2.setCursor(0, 15);
    u8g2.print("请配置WiFi");
    u8g2.setCursor(0, 31);
    u8g2.print("SSID:ESP8266-Config");
    u8g2.setCursor(0, 45);
    u8g2.print("192.168.4.1");
    u8g2.sendBuffer();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(1000);
    timeout++;
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi连接成功，IP: ");
    Serial.println(WiFi.localIP());
    u8g2.clearBuffer();
    u8g2.setCursor(0, 29);
    u8g2.print("WiFi连接成功");
    u8g2.setCursor(0, 44);
    u8g2.print(WiFi.localIP());
    u8g2.sendBuffer();

    syncNtp();
    fetchWeather();
  } else {
    Serial.println("WiFi连接失败，进入配置模式");
    initWebConfig();
    configMode = true;
    u8g2.clearBuffer();
    u8g2.setCursor(0, 15);
    u8g2.print("请配置WiFi");
    u8g2.setCursor(0, 31);
    u8g2.print("SSID:ESP8266-Config");
    u8g2.setCursor(0, 45);
    u8g2.print("192.168.4.1");
    u8g2.sendBuffer();
  }
}

void setup() {
  Serial.begin(115200);
  delay(50);

  u8g2.begin();

  Wire.begin(SDA_PIN, SCL_PIN);
  sht2x.begin();
  uint8_t stat = sht2x.getStatus();

  u8g2.enableUTF8Print();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.setFontDirection(0);
  u8g2.clearBuffer();

  u8g2.setCursor(0, 15);
  u8g2.print("SHT2X状态: ");
  u8g2.print(stat);

  u8g2.setCursor(0, 40);
  u8g2.print("WiFi连接中...");
  u8g2.sendBuffer();

  initStorage();
  initWebServer();
  connectWifi();
  initOta();
}

void loop() {
  ArduinoOTA.handle();
  handleWebConfig(); /* 始终处理web请求（配置模式下含DNS强制门户） */

  if (configMode) {
    return;
  }

  time_t now = time(nullptr);

  /* 每6小时重新同步一次NTP时间 */
  if (lastNtpSync == 0 || now - lastNtpSync >= 6 * 3600) {
    lastNtpSync = now; /* 先标记，避免失败时频繁重试 */
    syncNtp();
  }

  /* 每10分钟刷新一次天气 */
  if (lastWeatherFetch == 0 || now - lastWeatherFetch >= 10 * 60) {
    lastWeatherFetch = now;
    fetchWeather();
  }

  /* 每30秒刷新一次本地温度 */
  if (lastLocalTempFetch == 0 || now - lastLocalTempFetch >= 30) {
    lastLocalTempFetch = now;
    fetchLocalTemp();
  }

  updateClockDisplay();
}
