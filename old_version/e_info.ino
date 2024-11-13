// E-Info: Information Display System
// 2020-05-18  T. Nakagawa

#include <Preferences.h>
#include <WiFi.h>
#include <soc/rtc_cntl_reg.h>
#include "EPDClass.h"
#include "Script.h"

extern "C" int rom_phy_get_vdd33();

constexpr static int SHUTDOWN_VOLTAGE = 2.7;
constexpr static int BUFFER_SIZE = 32 * 1024;
constexpr int BUSY_PIN = 16;
constexpr int RST_PIN = 17;
constexpr int DC_PIN = 22;
constexpr int CS_PIN = 5;
constexpr int VDD_PIN = 2;
constexpr int DCDC_PIN = 4;
constexpr char NTP_SERVER[] = "ntp.nict.jp";

static Preferences preferences;
static EPDClass epd(BUSY_PIN, RST_PIN, DC_PIN, CS_PIN);
static Script script;
static uint8_t buffer[BUFFER_SIZE];
static uint8_t screen_blk[5808];
static uint8_t screen_red[5808];
static float voltage;

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Disable brown-out detection.

  Serial.begin(115200);
  while (!Serial) ;
  Serial.println("E-info firmware");

  voltage = getVoltage();
  Serial.println("Battery voltage: " + String(voltage));
  if (voltage < SHUTDOWN_VOLTAGE) shutdown();

  const int h = hallRead();
  Serial.println("Hall sensor: " + String(h));
  if (h < 10 || h > 40) config();

  run();
}

void loop() {
}

float getVoltage() {
  btStart();
  delay(1000);
  float vdd = 0.0;
  for (int i = 0; i < 100; i++) {
    delay(10);
    vdd += rom_phy_get_vdd33();
  }
  btStop();
  vdd /= 100.0;
  vdd = -0.0000135277 * vdd * vdd + 0.0128399 * vdd + 0.474502;
  return vdd;
}

void shutdown() {
  Serial.println("Battery voltage is low.");

  // Draw an empty battery icon.
  for (int i = 0; i < 32; i++) {
    screen_blk[22 * (i + 40) + 5 + 4] = 0x80;
    screen_blk[22 * (i + 40) + 21 - 5 - 4] = 0x01;
  }
  for (int i = 0; i < 264 - 40 - 40 - 32; i++) {
    screen_blk[22 * (i + 40 + 32) + 5] = 0x80;
    screen_blk[22 * (i + 40 + 32) + 21 - 5] = 0x01;
  }
  for (int i = 0; i < 4; i++) {
    screen_blk[22 * 40 + 5 + 4 + i] = 0xff;
    screen_blk[22 * (40 + 32) + 5 + i] = 0xff;
    screen_blk[22 * (40 + 32) + 21 - 5 - i] = 0xff;
  }
  for (int i = 0; i < 22 - 5 - 5; i++) {
    screen_blk[22 * (264 - 40 - 1) + 5 + i] = 0xff;
  }
  epd_power(true);
  epd.begin();
  epd.write(screen_blk, screen_red);
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
      const int size = (line.startsWith("POST ")) ? inputPost(&client, buffer, BUFFER_SIZE - 1) : 0;
      Serial.println("Data size: " + String(size));
      String message;
      if (line.startsWith("POST /ssid.cgi ")) {
        if (size > 0) {
          buffer[size] = '\0';
          preferences.putString("SSID", (char *)buffer);
          Serial.println("SSID: " + String((char *)buffer));
          message = "Succeeded to update SSID.";
        } else {
          message = "Failed to update SSID.";
        }
      } else if (line.startsWith("POST /pass.cgi ")) {
        if (size > 0) {
          buffer[size] = '\0';
          preferences.putString("PASS", (char *)buffer);
          Serial.println("PASS: " + String((char *)buffer));
          message = "Succeeded to update PASS.";
        } else {
          message = "Failed to update PASS.";
        }
      } else if (line.startsWith("POST /font.cgi ")) {
        if (size == 11264) {
          preferences.putBytes("FONT", buffer, size);
          if (preferences.getBytesLength("FONT") == size) {
            message = "Succeeded to update FONT.";
          } else {
            message = "Failed to write FONT.";
          }
        } else {
          message = "Failed to update FONT: " + String(size);
        }
      } else if (line.startsWith("POST /code.cgi ")) {
        if (size > 0) {
          buffer[size] = '\0';
          preferences.putBytes("CODE", buffer, size + 1);
          if (preferences.getBytesLength("CODE") == size + 1) {
            message = "Succeeded to update CODE: " + String(size);
          } else {
            message = "Failed to write CODE.";
          }
        } else {
          message = "Failed to update CODE.";
        }
      }

      client.println("<!DOCTYPE html>");
      client.println("<head><title>E-Info</title></head>");
      client.println("<body>");
      client.println("<h1>E-Info</h1>");
      client.println("<form action=\"ssid.cgi\" method=\"post\" enctype=\"multipart/form-data\">SSID: <input type=\"text\" name=\"data\" size=\"10\"> <input type=\"submit\"></form>");
      client.println("<form action=\"pass.cgi\" method=\"post\" enctype=\"multipart/form-data\">PASS: <input type=\"text\" name=\"data\" size=\"10\"> <input type=\"submit\"></form>");
      client.println("<form action=\"font.cgi\" method=\"post\" enctype=\"multipart/form-data\">FONT: <input type=\"file\" name=\"data\"> <input type=\"submit\"></form>");
      client.println("<form action=\"code.cgi\" method=\"post\" enctype=\"multipart/form-data\">CODE: <input type=\"file\" name=\"data\"> <input type=\"submit\"></form>");
      client.println("<p>" + message + "</p>");
      client.println("</body>");
      client.println("</html>");
      client.stop();
    }
  }
}

int inputPost(WiFiClient *client, uint8_t *buf, int buf_size) {
  String boundary;
  bool header = false;
  while (client->connected() && client->available()) {
    String line = client->readStringUntil('\n');
    line.trim();
    if (line.startsWith("--")) {
      boundary = line;
      Serial.println("Boundary: " + boundary);
    } else if (boundary.length() && line.indexOf("name=\"data\"") >= 0) {
      header = true;
      Serial.println("Header: " + line);
    } else if (header && line == "") {
      break;
    }
  }
  boundary = "\r\n" + boundary + "--\r\n";
  size_t ofst = 0;
  while (client->connected() && client->available()) {
    const size_t avil = buf_size - ofst;
    size_t size = client->readBytes(buf + ofst, avil);
    ofst += size;
    if (ofst >= buf_size - 1) return 0;  // Too large input data.
  }
  buf[ofst] = '\0';
  if (String((char *)(buf + ofst - boundary.length())) != boundary) return 0;
  return (ofst - boundary.length());
}

void run() {
  Serial.println("Entering the run mode.");
  preferences.begin("e_info", true);

  // Enable WiFi.
  WiFi.mode(WIFI_STA);
  String ssid = preferences.getString("SSID");
  String pass = preferences.getString("PASS");
  ssid.trim();
  pass.trim();
  Serial.println("SSID: " + ssid);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.print("Connecting WiFi.");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("done");

  // Get time with NTP.
  configTime(9 * 3600L, 0, NTP_SERVER);
  struct tm time;
  getLocalTime(&time);
  Serial.println("Time: " + String(time.tm_hour) + ":" + String(time.tm_min));

  // Run the script.
  Serial.println("Running the code: " + String(preferences.getBytesLength("CODE") - 1));
  script.initialize(time, voltage);
  preferences.getBytes("CODE", buffer, BUFFER_SIZE);
  script.run((char *)buffer);

  // Disable WiFi.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // Output to the EPD.
  Serial.println("Drawing EPD.");
  const uint8_t *text;
  const uint8_t *attr;
  script.getScreen(&text, &attr);
  drawScreen(text, attr, screen_blk, screen_red);
  epd_power(true);
  epd.begin();
  epd.write(screen_blk, screen_red);
  epd.sleep();
  epd.end();
  epd_power(false);

  // Deep sleep.
  Serial.println("Sleeping: ");
  uint64_t sleep = script.getSleepTime();
  if (sleep < 1000 * 1000 * 60) sleep += 1000 * 1000 * 60;
  Serial.println("Sleep (hour): " + String(float(sleep) / 1000 / 1000 / 60 / 60));
  esp_sleep_enable_timer_wakeup(sleep);
  esp_deep_sleep_start();
}

void drawScreen(const uint8_t *text, const uint8_t *attr, uint8_t *buf_blk, uint8_t *buf_red) {
  uint16_t *font = (uint16_t *)buffer; 
  preferences.getBytes("FONT", font, 11264);
  uint16_t *bmp_blk = (uint16_t *)(buffer + 11264);
  uint16_t *bmp_red = (uint16_t *)(buffer + 11264 + 2 * Script::WIDTH);

  for (int ty = 0; ty < Script::HEIGHT; ty++) {
    for (int h = 0; h < Script::PIXELS; h++) {
      int tx = 0;
      // Copy the font bitmap for one line.
      for (int w = 0; w < Script::WIDTH; w++) {
        const uint8_t a = attr[2 * Script::WIDTH * ty + tx];
        const uint8_t c = text[2 * Script::WIDTH * ty + tx++];
        uint16_t f;
        if (a & 0x04) {
          const uint8_t cc = 0xd0 + ((c - 0x20) >> 1);
          const uint8_t d = text[2 * Script::WIDTH * ty + tx++];
          const uint8_t dd = 0xd0 + ((d - 0x20) >> 1);
          if (c & 0x01) {
            f = (font[Script::PIXELS * cc + h] << 6);
          } else {
            f = (font[Script::PIXELS * cc + h] & 0xf800);;
          }
          if (d & 0x01) {
            f |= (font[Script::PIXELS * dd + h] & 0x03e0);
          } else {
            f |= (font[Script::PIXELS * dd + h] >> 6);;
          }
        } else {
          f = font[Script::PIXELS * c + h];
        }
        if (a & 0x02) {
          f = ~f;
        }
        if (a & 0x01) {
          bmp_blk[w] = 0x00;
          bmp_red[w] = (f & 0xffe0);
        } else {
          bmp_blk[w] = (f & 0xffe0);
          bmp_red[w] = 0x00;
        }
      }

      // Render the font to the screen buffer.
      int ptr = (EPDClass::WIDTH / 8) * (Script::PIXELS * ty + h);
      int size = 8;
      uint8_t acc_blk = 0x00;
      uint8_t acc_red = 0x00;
      for (int w = 0; w < Script::WIDTH; w++) {
        acc_blk |= (uint8_t)(bmp_blk[w] >> (16 - size));
        acc_red |= (uint8_t)(bmp_red[w] >> (16 - size));
        buf_blk[ptr] = acc_blk;
        buf_red[ptr] = acc_red;
        ptr++;
        if (11 - size >= 8) {
          buf_blk[ptr] = (uint8_t)((bmp_blk[w] >> (16 - size - 8)) & 0x00ff);
          buf_red[ptr] = (uint8_t)((bmp_red[w] >> (16 - size - 8)) & 0x00ff);
          ptr++;
          size += 8;
        }
        acc_blk = (uint8_t)((bmp_blk[w] << size) >> 8);
        acc_red = (uint8_t)((bmp_red[w] << size) >> 8);
        size = 8 - (11 - size);
      }
    }
  }
}
