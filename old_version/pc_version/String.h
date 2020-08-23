// Arduino String class.

#ifndef STRING_H_
#define STRING_H_

#include <cstdio>
#include <string>

class String {
 public:
  String() {
  }

  String(const std::string &s) : str_(s) {
  }

  String(const char *s) : str_(s) {
  }

  String(int x) {
    str_ = std::to_string(x);
  }

  String(float x, int dec) {
    char buf[100];
    sprintf(buf, "%.*f", dec, x);
    str_ = std::string(buf);
  }

  String substring(int from, int to=-1) const {
    if (to < 0) return String(str_.substr(from));
    else return String(str_.substr(from, to - from));
  }

  int toInt() const {
    return std::stoi(str_);
  }

  float toFloat() const {
    return std::stof(str_);
  }

  int indexOf(const String &val, size_t pos=0) const {
    const auto n = str_.find(val.str_, pos);
    return (n == std::string::npos) ? -1 : n;
  }

  int lastIndexOf(const String &val) const {
    const auto n = str_.rfind(val.str_);
    return (n == std::string::npos) ? -1 : n;
  }

  void replace(const String &x, const String &y) {
    std::string result(str_);
    size_t pos;
    while ((pos = result.find(x.str_)) != std::string::npos) {
      result.replace(pos, x.str_.length(), y.str_);
    }
    str_ = result;
  }

  void concat(const String &x) {
    str_ += x.str_;
  }

  std::string string() const {
    return str_;
  }

  bool operator==(const String &x) const {
    return (str_ == x.str_);
  }

  bool operator!=(const String &x) const {
    return (str_ != x.str_);
  }

  String operator+(const String &x) const {
    return (str_ + x.str_);
  }

  size_t length() const {
    return str_.length();
  }

  char operator[](int x) const {
    return str_[x];
  }

 private:
  std::string str_;
};

#endif
