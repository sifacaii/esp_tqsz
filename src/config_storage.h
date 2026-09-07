#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include <Arduino.h>
#include <EEPROM.h>

#define EEPROM_SIZE 512
#define WIFI_SSID_ADDR 0
#define WIFI_PASS_ADDR 32
#define WIFI_MAX_LEN 32

/* 读取EEPROM中指定地址的字符串 */
String readEepromString(int addr, int maxLen) {
  char buf[maxLen + 1];
  int len = 0;
  for (int i = 0; i < maxLen; i++) {
    char c = EEPROM.read(addr + i);
    if (c == '\0' || c == 0xFF) {
      break;
    }
    buf[len++] = c;
  }
  buf[len] = '\0';
  return String(buf);
}

/* 写入字符串到EEPROM（含结束符），最大 maxLen+1 字节 */
void saveEepromString(int addr, const String &s, int maxLen) {
  int i = 0;
  for (; i < maxLen && i < (int)s.length(); i++) {
    EEPROM.write(addr + i, (uint8_t)s[i]);
  }
  for (; i < maxLen + 1; i++) {
    EEPROM.write(addr + i, '\0');
  }
}

void initStorage() {
  EEPROM.begin(EEPROM_SIZE);
}

void loadWiFiConfig(String &ssid, String &pass) {
  ssid = readEepromString(WIFI_SSID_ADDR, WIFI_MAX_LEN);
  pass = readEepromString(WIFI_PASS_ADDR, WIFI_MAX_LEN);
}

void saveWiFiConfig(const String &ssid, const String &pass) {
  saveEepromString(WIFI_SSID_ADDR, ssid, WIFI_MAX_LEN);
  saveEepromString(WIFI_PASS_ADDR, pass, WIFI_MAX_LEN);
  EEPROM.commit();
}

#endif
