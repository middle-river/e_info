#include <cstdio>
#include <iostream>
#include <time.h>
#include "../Script.h"

int main(void) {
  time_t timer;
  time(&timer);
  const struct tm *time = localtime(&timer);

  std::string buf;
  std::getline(std::cin, buf, '\0');

  Script script;
  script.initialize(*time, 3.14);
  script.run(buf);

  std::cout << "Sleep time: " << script.getSleepTime() << "\n";
  const uint8_t *text, *attr;
  script.getScreen(&text, &attr);
  std::cout << "TEXT:\n";
  for (int i = 0; i < Script::HEIGHT; i++) {
    for (int j = 0; j < Script::WIDTH * 2; j++) {
      const unsigned char c = text[i * Script::WIDTH * 2 + j];
      if (c == '\0') {
        std::cout << ' ';
      } else if (c < 0x80) {
        std::cout << c;
      } else {
        char buf[10];
        sprintf(buf, "\\x%02x", c);
        std::cout << buf;
      }
    }
    std::cout << "\n";
  }
  std::cout << "ATTR:\n";
  for (int i = 0; i < Script::HEIGHT; i++) {
    for (int j = 0; j < Script::WIDTH * 2; j++) {
      std::cout << (char)((int)'0' + attr[i * Script::WIDTH * 2 + j]);
    }
    std::cout << "\n";
  }

  return 0;
}
