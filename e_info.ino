// E-Info: Information Display System
// 2020-05-23  T. Nakagawa

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <soc/rtc_cntl_reg.h>
#include "EPDClass.h"

extern "C" int rom_phy_get_vdd33();

constexpr float SHUTDOWN_VOLTAGE = 2.7f;
constexpr int BUSY_PIN = 16;
constexpr int RST_PIN = 17;
constexpr int DC_PIN = 22;
constexpr int CS_PIN = 5;
constexpr int VDD_PIN = 2;
constexpr int DCDC_PIN = 4;
constexpr uint32_t font[10] = {0x00eaaae0, 0x00444440, 0x00e8e2e0, 0x00e2e2e0, 0x0022eaa0, 0x00e2e8e0, 0x00eae8e0, 0x00222ae0, 0x00eaeae0, 0x00e2eae0};  // 4x8 font data for digits 0-9.
constexpr uint8_t scaling[16] = {0x00, 0x03, 0x0c, 0x0f, 0x30, 0x33, 0x3c, 0x3f, 0xc0, 0xc3, 0xcc, 0xcf, 0xf0, 0xf3, 0xfc, 0xff};  // Table for magnifying 4 bit vector to 8 bit.

Preferences preferences;
EPDClass epd(BUSY_PIN, RST_PIN, DC_PIN, CS_PIN);
uint8_t buf_blk[5808];
uint8_t buf_red[5808];

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Disable brown-out detection.

  Serial.begin(115200);
  while (!Serial) ;
  Serial.println("E-info firmware");

  const float voltage = getVoltage();
  Serial.println("Battery voltage: " + String(voltage));
  if (voltage < SHUTDOWN_VOLTAGE) shutdown();

  const int h = hallRead();
  Serial.println("Hall sensor: " + String(h));
  if (h < 10 || h > 40) config();
  preferences.begin("e_info", true);

  // Enable WiFi.
  WiFi.mode(WIFI_STA);
  WiFi.begin(preferences.getString("SSID").c_str(), preferences.getString("PASS").c_str());
  Serial.print("Connecting WiFi.");
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() >= 30000) suspend();
    delay(500);
    Serial.print(".");
  }
  Serial.println("done");

  // Get time with NTP.
  const String ntps = preferences.getString("NTPS");
  Serial.println("NTP server: " + ntps);
  configTime(9 * 3600L, 0, ntps.c_str());
  struct tm time;
  getLocalTime(&time);
  Serial.println("Time: " + String(time.tm_hour) + ":" + String(time.tm_min));

  Serial.println("Obtaining the data.");
  for (int retry = 0; ; retry++) {
    if (retry == 3) suspend();
    if (read_data("blk.pbm", buf_blk)) break;
    delay(3000);
  }
  for (int retry = 0; ; retry++) {
    if (retry == 3) suspend();
    if (read_data("red.pbm", buf_red)) break;
    delay(3000);
  }

  // Disable WiFi.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // Draw battery information in top-right 16x16 area.
  Serial.println("Drawing battery information.");
  const int digit0 = (int)(voltage * 10.0) % 10;
  const int digit1 = (int)(voltage * 100.0 + 0.5) % 10;
  uint32_t font0 = font[digit0];
  uint32_t font1 = font[digit1];
  uint32_t ptr = EPDClass::WIDTH / 8 - 2;
  for (int i = 0; i < 8; i++) {
    const uint8_t bmp0 = scaling[font0 & 0xf];
    const uint8_t bmp1 = scaling[font1 & 0xf];
    buf_blk[ptr] = bmp0;
    buf_blk[ptr + 1] = bmp1;
    buf_red[ptr] = ~bmp0;
    buf_red[ptr + 1] = ~bmp1;
    ptr += EPDClass::WIDTH / 8;
    buf_blk[ptr] = bmp0;
    buf_blk[ptr + 1] = bmp1;
    buf_red[ptr] = ~bmp0;
    buf_red[ptr + 1] = ~bmp1;
    ptr += EPDClass::WIDTH / 8;
    font0 >>= 4;
    font1 >>= 4;
  }

  // Output to the EPD.
  Serial.println("Drawing EPD.");
  epd_power(true);
  epd.begin();
  epd.write(buf_blk, buf_red);
  epd.sleep();
  epd.end();
  epd_power(false);

  // Deep sleep.
  const int slph = preferences.getString("SLPH").toInt();
  const int slpm = preferences.getString("SLPM").toInt();
  int sleep = (slph - time.tm_hour) * 60 + (slpm - time.tm_min);
  if (sleep < 0) sleep += 24 * 60;
  if (sleep < 3) sleep = 3;
  Serial.println("Sleep (hour): " + String(float(sleep) / 60.0));
  esp_sleep_enable_timer_wakeup((uint64_t)sleep * 60 * 1000 * 1000);
  esp_deep_sleep_start();
}

void loop() {
}

void suspend() {
  Serial.println("Suspended.");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  esp_sleep_enable_timer_wakeup((uint64_t)(1 * 60) * 60 * 1000 * 1000);
  esp_deep_sleep_start();
}

float getVoltage() {
  btStart();
  const int v = rom_phy_get_vdd33();
  btStop();
  const float vdd =  0.0005045f * v + 0.3368f;
  return vdd;
}

void shutdown() {
  Serial.println("Battery voltage is low.");

  // Draw an empty battery icon.
  for (int i = 0; i < 32; i++) {
    buf_blk[22 * (i + 40) + 5 + 4] = 0x80;
    buf_blk[22 * (i + 40) + 21 - 5 - 4] = 0x01;
  }
  for (int i = 0; i < 264 - 40 - 40 - 32; i++) {
    buf_blk[22 * (i + 40 + 32) + 5] = 0x80;
    buf_blk[22 * (i + 40 + 32) + 21 - 5] = 0x01;
  }
  for (int i = 0; i < 4; i++) {
    buf_blk[22 * 40 + 5 + 4 + i] = 0xff;
    buf_blk[22 * (40 + 32) + 5 + i] = 0xff;
    buf_blk[22 * (40 + 32) + 21 - 5 - i] = 0xff;
  }
  for (int i = 0; i < 22 - 5 - 5; i++) {
    buf_blk[22 * (264 - 40 - 1) + 5 + i] = 0xff;
  }
  epd_power(true);
  epd.begin();
  epd.write(buf_blk, buf_red);
  epd.sleep();
  epd.end();
  epd_power(false);

  Serial.println("Sleeping.");
  esp_deep_sleep_start();  // Sleep indefinitely.
}

void epd_power(bool enable) {
  if (enable) {
    pinMode(VDD_PIN, OUTPUT);
    pinMode(DCDC_PIN, OUTPUT);
    digitalWrite(VDD_PIN, HIGH);
    digitalWrite(DCDC_PIN, HIGH);
  } else {
    pinMode(VDD_PIN, INPUT);
    pinMode(DCDC_PIN, INPUT);
    digitalWrite(VDD_PIN, LOW);
    digitalWrite(DCDC_PIN, LOW);
  }
}

void config() {
  Serial.println("Entering the configuration mode.");
  preferences.begin("e_info", false);
  Serial.println("Free entries: " + String(preferences.freeEntries()));
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32", "12345678");
  delay(100);
  WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));

  WiFiServer server(80);
  server.begin();
  while (true) {
    WiFiClient client = server.available();
    if (client) {
      const String line = client.readStringUntil('\n');
      Serial.println("Accessed: " + line);
      String message;
      if (line.startsWith("GET /configure.cgi?")) {
        String key;
        String val;
        String buf = line.substring(19);
        int pos = buf.indexOf(" ");
        if (pos < 0) pos = 0;
        buf = buf.substring(0, pos);
        buf.concat("&");
        while (buf.length()) {
          int pos = buf.indexOf("&");
          const String param = buf.substring(0, pos);
          buf = buf.substring(pos + 1);
          pos = param.indexOf("=");
          if (pos < 0) continue;
          if (param.substring(0, pos) == "key") key = url_decode(param.substring(pos + 1));
          else if (param.substring(0, pos) == "val") val = url_decode(param.substring(pos + 1));
        }
        key.trim();
        val.trim();
        Serial.println("key=" + key + ", val=" + val);
        if (key.length()) {
          preferences.putString(key.c_str(), val);
          if (preferences.getString(key.c_str()) == val) {
            message = "Succeeded to update: " + key;
          } else {
            message = "Failed to write: " + key;
          }
        } else {
          message = "Key was not found.";
        }
      }

      client.println("<!DOCTYPE html>");
      client.println("<head><title>E-Info</title></head>");
      client.println("<body>");
      client.println("<h1>E-Info</h1>");
      client.println("<form action=\"configure.cgi\" method=\"get\">Key: <input type=\"text\" name=\"key\" size=\"10\"> Value: <input type=\"text\" name=\"val\" size=\"100\"> <input type=\"submit\"></form>");
      client.println("<p>" + message + "</p>");
      client.println("</body>");
      client.println("</html>");
      client.stop();
    }
  }
}

String url_decode(const String &str) {
  String result;
  for (int i = 0; i < str.length(); i++) {
    const char c = str[i];
    if (c == '+') {
      result.concat(" ");
    } else if (c == '%' && i + 2 < str.length()) {
      const char c0 = str[++i];
      const char c1 = str[++i];
      unsigned char d = 0;
      d += (c0 <= '9') ? c0 - '0' : (c0 <= 'F') ? c0 - 'A' + 10 : c0 - 'a' + 10;
      d <<= 4;
      d += (c1 <= '9') ? c1 - '0' : (c1 <= 'F') ? c1 - 'A' + 10 : c1 - 'a' + 10;
      result.concat((char)d);
    } else {
      result.concat(c);
    }
  }
  return result;
}

bool read_data(const String &file, uint8_t *data) {
  const String url = preferences.getString("DURL");
  HTTPClient client;
  Serial.println("Fetching the URL: " + url + file);
  client.begin(url + file);
  const int res = client.GET();
  Serial.println("Response: " + String(res));
  if (res != HTTP_CODE_OK) return false;
  const String &page = client.getString();
  Serial.println("Data size: " + String(page.length()));
  if (page.length() != 5808 + 11) return false;
  Serial.println("Data ID: " + page.substring(0, 11));
  if (page.substring(0, 11) != "P4\n176 264\n") return false;
  Serial.println("Copying the data.");
  memcpy(data, page.c_str() + 11, 5808);  // getBytes() cannot copy binary data since it uses strncpy().
  return true;
}
