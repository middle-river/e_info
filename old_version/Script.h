// Script interpreter.

#ifndef SCRIPT_H_
#define SCRIPT_H_

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <fstream>
#include <iostream>
#include "test/String.h"
#endif

class Stack {
 public:
  Stack() : size_(0) {
  }

  void push(const String &v) {
    if (size_ < CAPACITY_) stack_[size_++] = v;
  }

  String pop() {
    return (size_ > 0) ? stack_[--size_] : String("");
  }

  const String &get(size_t n) const {
    return stack_[(n < size_) ? size_ - 1 - n : 0];
  }

  void put(size_t n, const String &v) {
    if (n < size_) stack_[size_ - 1 - n] = v;
  }

  int size() {
    return size_;
  }

private:
  constexpr static const int CAPACITY_ = 64;
  size_t size_;
  String stack_[CAPACITY_];
};

class Script {
 public:
  Script();
  ~Script();
  void initialize(const struct tm &time, float battery);
  void run(const String &buf);
  uint64_t getSleepTime();
  void getScreen(const uint8_t **text, const uint8_t **attr);

  constexpr static const int WIDTH = 16;
  constexpr static const int HEIGHT = 12;
  constexpr static const int PIXELS = 22;

 private:
  String getNextToken(const String &buf, size_t *ptr);
  bool getClause(const String &buf, size_t *ptr, String *code);

  struct tm time_;
  float battery_;
  Stack stack_;
  uint64_t sleep_time_;
  uint8_t text_[WIDTH * 2 * HEIGHT];
  uint8_t attr_[WIDTH * 2 * HEIGHT];
  constexpr static const int FUNCTIONS_SIZE_ = 64;
  struct {
    String name;
    String code;
  } funcs_[FUNCTIONS_SIZE_];
  int func_size_;
};

#endif
