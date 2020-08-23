// Library for Waveshare 2.7-inch e-Paper (B)

#ifndef EPDCLASS_H_
#define EPDCLASS_H_

#include <Arduino.h>
#include <SPI.h>

class EPDClass {
public:
  static constexpr const int WIDTH = 176;
  static constexpr const int HEIGHT = 264;

  EPDClass(int busy_pin, int rst_pin, int dc_pin, int csb_pin) : busy_pin_(busy_pin), rst_pin_(rst_pin), dc_pin_(dc_pin), csb_pin_(csb_pin) {
  }

  ~EPDClass() {
  }

  void begin() {
    pinMode(busy_pin_, INPUT); 
    pinMode(rst_pin_, OUTPUT);
    pinMode(dc_pin_, OUTPUT);
    pinMode(csb_pin_, OUTPUT);
    SPI.begin();
    SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0));

    // Reset
    digitalWrite(rst_pin_, LOW);
    delay(200);
    digitalWrite(rst_pin_, HIGH);
    delay(200);   

    // Power on
    command(0x04, 0, nullptr);
    while (digitalRead(busy_pin_) == LOW) delay(100);

    // Panel setting.
    command(0x00, 1, (const uint8_t []){0xaf});

    // PLL control
    command(0x30, 1, (const uint8_t []){0x3a});

    // Power setting.
    command(0x01, 5, (const uint8_t []){0x02, 0x00, 0x2b, 0x2b, 0x09});  // Disable internal DC-DC for VDH/VDL.

    // Booster soft start
    command(0x06, 3, (const uint8_t []){0x07, 0x07, 0x17});

    // Power optimization
    command(0xf8, 2, (const uint8_t []){0x60, 0xa5});
    command(0xf8, 2, (const uint8_t []){0x89, 0xa5});
    command(0xf8, 2, (const uint8_t []){0x90, 0x00});
    command(0xf8, 2, (const uint8_t []){0x93, 0x2a});
    command(0xf8, 2, (const uint8_t []){0x73, 0x41});

    // VCM_DC setting register
    command(0x82, 1, (const uint8_t []){0x12});

    // VCOM and data interval setting
    command(0x50, 1, (const uint8_t []){0x87});

    // LUT for VCOM
    command(0x20, 44, (const uint8_t []){0x00, 0x00, 0x00, 0x1a, 0x1a, 0x00, 0x00, 0x01, 0x00, 0x0a, 0x0a, 0x00, 0x00, 0x08, 0x00, 0x0e, 0x01, 0x0e, 0x01, 0x10, 0x00, 0x0a, 0x0a, 0x00, 0x00, 0x08, 0x00, 0x04, 0x10, 0x00, 0x00, 0x05, 0x00, 0x03, 0x0e, 0x00, 0x00, 0x0a, 0x00, 0x23, 0x00, 0x00, 0x00, 0x01});

    // White to white LUT
    command(0x21, 42, (const uint8_t []){0x90, 0x1a, 0x1a, 0x00, 0x00, 0x01, 0x40, 0x0a, 0x0a, 0x00, 0x00, 0x08, 0x84, 0x0e, 0x01, 0x0e, 0x01, 0x10, 0x80, 0x0a, 0x0a, 0x00, 0x00, 0x08, 0x00, 0x04, 0x10, 0x00, 0x00, 0x05, 0x00, 0x03, 0x0e, 0x00, 0x00, 0x0a, 0x00, 0x23, 0x00, 0x00, 0x00, 0x01});

    // Black to white LUT
    command(0x22, 42, (const uint8_t []){0xa0, 0x1a, 0x1a, 0x00, 0x00, 0x01, 0x00, 0x0a, 0x0a, 0x00, 0x00, 0x08, 0x84, 0x0e, 0x01, 0x0e, 0x01, 0x10, 0x90, 0x0a, 0x0a, 0x00, 0x00, 0x08, 0xb0, 0x04, 0x10, 0x00, 0x00, 0x05, 0xb0, 0x03, 0x0e, 0x00, 0x00, 0x0a, 0xc0, 0x23, 0x00, 0x00, 0x00, 0x01});

    // White to Black LUT
    command(0x23, 42, (const uint8_t []){0x90, 0x1a, 0x1a, 0x00, 0x00, 0x01, 0x40, 0x0a, 0x0a, 0x00, 0x00, 0x08, 0x84, 0x0e, 0x01, 0x0e, 0x01, 0x10, 0x80, 0x0a, 0x0a, 0x00, 0x00, 0x08, 0x00, 0x04, 0x10, 0x00, 0x00, 0x05, 0x00, 0x03, 0x0e, 0x00, 0x00, 0x0a, 0x00, 0x23, 0x00, 0x00, 0x00, 0x01});

    // Black to Black LUT
    command(0x24, 42, (const uint8_t []){0x90, 0x1a, 0x1a, 0x00, 0x00, 0x01, 0x20, 0x0a, 0x0a, 0x00, 0x00, 0x08, 0x84, 0x0e, 0x01, 0x0e, 0x01, 0x10, 0x10, 0x0a, 0x0a, 0x00, 0x00, 0x08, 0x00, 0x04, 0x10, 0x00, 0x00, 0x05, 0x00, 0x03, 0x0e, 0x00, 0x00, 0x0a, 0x00, 0x23, 0x00, 0x00, 0x00, 0x01});

    // Partial display refresh
    command(0x16, 1, (const uint8_t []){0x00});
  }

  void end() {
    SPI.endTransaction();
    SPI.end();
  }

  void write(const uint8_t *buf_black, const uint8_t *buf_red) {
    // TCON resolution
    command(0x61, 4, (const uint8_t []){WIDTH >> 8, WIDTH & 0xff, HEIGHT >> 8, HEIGHT & 0xff});

    // Data start transmission 1
    command(0x10, WIDTH * HEIGHT / 8, buf_black);
    delay(2);

    // Data start transmission 2
    command(0x13, WIDTH * HEIGHT / 8, buf_red);
    delay(2);

    // Display refresh
    command(0x12, 0, nullptr);
    while (digitalRead(busy_pin_) == LOW) delay(100);
  }

  void sleep(void) {
    // Deep sleep
    command(0x07, 1, (const uint8_t []){0xa5});
  }

private:
  int busy_pin_;
  int rst_pin_;
  int dc_pin_;
  int csb_pin_;

  void command(uint8_t cmd, int size, const uint8_t *data) {
    digitalWrite(dc_pin_, LOW);
    digitalWrite(csb_pin_, LOW);
    SPI.transfer(cmd);
    digitalWrite(csb_pin_, HIGH);

    for (int i = 0; i < size; i++) {
      digitalWrite(dc_pin_, HIGH);
      digitalWrite(csb_pin_, LOW);
      SPI.transfer(data[i]);
      digitalWrite(csb_pin_, HIGH);
    }
  }
};

#endif
