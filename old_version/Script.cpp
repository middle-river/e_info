#include "Script.h"
#ifdef ARDUINO
#define MBEDTLS_X509_ALLOW_UNSUPPORTED_CRITICAL_EXTENSION
#define MBEDTLS_TLS_DEFAULT_ALLOW_SHA1_IN_CERTIFICATES
#include "HTTPClient.h"
#endif

namespace {

#ifdef ARDUINO
void getHTTP(const String &url, String *page) {
  Serial.println("Fetching the URL: " + url);
  HTTPClient client;
  client.begin(url);
  client.addHeader("Accept", "*/*");
  const int res = client.GET();
  Serial.println("Response: " + String(res));
  if (res == HTTP_CODE_OK) {
    *page = client.getString();
    Serial.println("Page size: " + String(page->length()));
  }
}
#else
uint32_t fnv1Hash(uint8_t *data, size_t size) {
  uint32_t hash = 2166136261U;
  while (size--) hash = (16777619U * hash) ^ *data++;
  return hash;
}
#endif

String unescapeString(const String &str, size_t bgn, size_t end) {
  if (end <= bgn || end > str.length()) return "";

  constexpr int buf_size = 1024;
  static char buf_mem[buf_size];
  char *buf = buf_mem;
  for (size_t ptr = bgn; ptr < end && buf - buf_mem + 1 < buf_size; ptr++) {
    const char c = str[ptr];
    if (c == '\\' && ptr + 1 < end) {
      const char d = str[++ptr];
      if (d == 'x' && ptr + 2 < end) {
        int e0 = str[++ptr];
        int e1 = str[++ptr];
        if (e0 <= '9') e0 -= '0'; else e0 += 10 - 'a';
        if (e1 <= '9') e1 -= '0'; else e1 += 10 - 'a';
        *buf++ = ((e0 << 4) | e1);
      } else if (d == 'u' && ptr + 4 < end && buf - buf_mem + 4 < buf_size) {
        int e0 = str[++ptr];
        int e1 = str[++ptr];
        int e2 = str[++ptr];
        int e3 = str[++ptr];
        if (e0 <= '9') e0 -= '0'; else e0 += 10 - 'a';
        if (e1 <= '9') e1 -= '0'; else e1 += 10 - 'a';
        if (e2 <= '9') e2 -= '0'; else e2 += 10 - 'a';
        if (e3 <= '9') e3 -= '0'; else e3 += 10 - 'a';
        const int f = ((e0 << 12) | (e1 << 8) | (e2 << 4) | e3);
        if (f <= 0x7f) {
          *buf++ = (c & 0x7f);
        } else if (f <= 0x7ff) {
          *buf++ = (0xc0 | (f >> 6));
          *buf++ = (0x80 | (f & 0x3f));
        } else if (f <= 0xffff) {
          *buf++ = (0xe0 | (f >> 12));
          *buf++ = (0x80 | ((f >> 6) & 0x3f));
          *buf++ = (0x80 | (f & 0x3f));
        } else {
          *buf++ = (0xf0 | (f >> 18));
          *buf++ = (0x80 | ((f >> 12) & 0x3f));
          *buf++ = (0x80 | ((f >> 6) & 0x3f));
          *buf++ = (0x80 | (f & 0x3f));
        }
      } else {
        char x = d;
        if (d == 'b') x =  '\b';
        else if (d == 'f') x =  '\f';
        else if (d == 'n') x =  '\n';
        else if (d == 'r') x =  '\r';
        else if (d == 't') x =  '\t';
        *buf++ = x;
      }
    } else {
      *buf++ = c;
    }
  }
  *buf++ = '\0';
  const String result = String(buf_mem);
  return result;
}

class JSON {
 public:
  JSON(const String &data) : data_(data) {
    ptr_ = skipWhitespaces(0);
    const char c = data_[ptr_];
    const String s = data_.substring(ptr_, ptr_ + 4);
    if (c == '"') type = STRING;
    else if ((c >= '0' && c <= '9') || c == '.' || c == '-') type = NUMBER;
    else if (s == "null" || s == "true" || s == "fals") type = BOOLEAN;
    else if (c == '{') type = OBJECT;
    else if (c == '[') type = ARRAY;
    else type = ERROR;
  }

  String value() const {
    const size_t bgn = ptr_;
    const size_t end = skipObject(bgn);
    if (type == STRING) {
      return unescapeString(data_, bgn + 1, end - 1);
    } else if (type == NUMBER || type == BOOLEAN) {
      return data_.substring(bgn, end);
    } else {
      return "";
    }
  }

  String get(int key) const {
    size_t ptr = ptr_ + 1;
    while (true) {
      const size_t bgn = skipWhitespaces(ptr);
      const size_t end = skipObject(bgn);
      if (end >= data_.length()) return "";
      if (key-- == 0) {
        return data_.substring(bgn, end);
      }
      ptr = skipWhitespaces(end);
      if (ptr >= data_.length() || data_[ptr] != ',') return "";
      ptr++;
    }
  }

  String get(const String &key) const {
    size_t ptr = ptr_ + 1;
    while (true) {
      const size_t kbgn = skipWhitespaces(ptr);
      if (kbgn >= data_.length() || data_[kbgn] != '"') return "";
      const size_t kend = skipObject(kbgn);
      ptr = skipWhitespaces(kend);
      if (ptr >= data_.length() || data_[ptr] != ':') return "";
      const size_t vbgn = skipWhitespaces(ptr + 1);
      const size_t vend = skipObject(vbgn);
      if (vend >= data_.length()) return "";
      if (unescapeString(data_, kbgn + 1, kend - 1) == key) {
        return data_.substring(vbgn, vend);
      }
      ptr = skipWhitespaces(vend);
      if (ptr >= data_.length() || data_[ptr] != ',') return "";
      ptr++;
    }
  }

  enum ValueType {STRING, NUMBER, BOOLEAN, OBJECT, ARRAY, ERROR};

  ValueType type;

 private:
  size_t skipWhitespaces(size_t ptr) const {
    for (; ptr < data_.length(); ptr++) {
      const char c = data_[ptr];
      if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
    }
    return ptr;
  }

  size_t skipObject(size_t ptr) const {
    const char c = data_[ptr];
    if (c == '"') {
      for (ptr++; ptr < data_.length(); ptr++) {
        const char d = data_[ptr];
        if (d == '"') {
          ptr++;
          break;
        }
        if (d == '\\' && ptr + 1 < data_.length()) ptr++;
      }
    } else if (c == '[') {
      for (ptr = ptr + 1; ptr < data_.length(); ) {
        ptr = skipWhitespaces(ptr);
        if (data_[ptr] == ']') {
          ptr++;
          break;
        }
        if (data_[ptr] == ',') {
          ptr++;
          continue;
        }
        ptr = skipObject(ptr);
      }
    } else if (c == '{') {
      for (ptr = ptr + 1; ptr < data_.length(); ) {
        ptr = skipWhitespaces(ptr);
        if (data_[ptr] == '}') {
          ptr++;
          break;
        }
        if (data_[ptr] == ',') {
          ptr++;
          continue;
        }
        ptr = skipObject(ptr);
        ptr = skipWhitespaces(ptr);
        if (ptr < data_.length() && data_[ptr] == ':') ptr++;
        ptr = skipWhitespaces(ptr);
        ptr = skipObject(ptr);
      }
    } else {
      for (ptr++; ptr < data_.length(); ptr++) {
        const char d = data_[ptr];
        if (!(d >= '0' && d <= '9') && !(d >= 'a' && d <= 'z') && d != '.' && d != '-') break;
      }
    }
    return ptr;
  }

  const String &data_;
  size_t ptr_;
};

}

Script::Script() : sleep_time_(24UL * 60 * 60 * 1000 * 1000), text_{0}, attr_{0}, func_size_(0) {
}

Script::~Script() {
}

void Script::initialize(const struct tm &time, float battery) {
  time_ = time;
  battery_ = battery;
}

void Script::run(const String &buf) {
  size_t ptr = 0;
  while (true) {
    const String token = getNextToken(buf, &ptr);
    if (token == "") break;

    if (token[0] == '#') {  // Comment.
      // NOP
    } else if (token.length() >= 2 && token[0] == '"') {  // String literal.
      stack_.push(unescapeString(token, 1, token.length() - 1));
    } else if ((token[0] >= '0' && token[0] <= '9') || token[0] == '-') {  // Number literal.
      stack_.push(token);
    } else if (token == "{" && stack_.size() >= 1) {
      const String name = stack_.pop();
      funcs_[func_size_].name = name;
      String &code = funcs_[func_size_].code;
      if (func_size_ < FUNCTIONS_SIZE_) func_size_++;
      String t;
      while ((t = getNextToken(buf, &ptr)) != "" && t != "}") {
        code.concat(t);
        code.concat("\t");
      }
    } else if (token == "?" && stack_.size() >= 1) {
      const int cond = stack_.pop().toInt();
      String code0, code1;
      if (!getClause(buf, &ptr, &code0)) {
        getClause(buf, &ptr, &code1);
      }
     if (cond) run(code0); else if (code1.length()) run(code1);
    } else if (token == "dup" && stack_.size() >= 1) {
      const String x = stack_.pop();
      stack_.push(x);
      stack_.push(x);
    } else if (token == "drop" && stack_.size() >= 1) {
      stack_.pop();
    } else if (token == "swap" && stack_.size() >= 2) {
      const String y = stack_.pop();
      const String x = stack_.pop();
      stack_.push(y);
      stack_.push(x);
    } else if (token == "read" && stack_.size() >= 1) {
      const int n = stack_.pop().toInt();
      stack_.push(stack_.get(n));
    } else if (token == "write" && stack_.size() >= 2) {
      const int n = stack_.pop().toInt();
      const String v = stack_.pop();
      stack_.put(n, v);
    } else if (token == "add" && stack_.size() >= 2) {
      const int y = stack_.pop().toInt();
      const int x = stack_.pop().toInt();
      stack_.push(String(x + y));
    } else if (token == "sub" && stack_.size() >= 2) {
      const int y = stack_.pop().toInt();
      const int x = stack_.pop().toInt();
      stack_.push(String(x - y));
    } else if (token == "mul" && stack_.size() >= 2) {
      const int y = stack_.pop().toInt();
      const int x = stack_.pop().toInt();
      stack_.push(String(x * y));
    } else if (token == "div" && stack_.size() >= 2) {
      const int y = stack_.pop().toInt();
      const int x = stack_.pop().toInt();
      stack_.push(String(x / y));
    } else if (token == "mod" && stack_.size() >= 2) {
      const int y = stack_.pop().toInt();
      const int x = stack_.pop().toInt();
      stack_.push(String(x % y));
    } else if (token == "and" && stack_.size() >= 2) {
      const String y = stack_.pop();
      const String x = stack_.pop();
      stack_.push(String((x != "0" && y != "0") ? "1" : "0"));
    } else if (token == "or" && stack_.size() >= 2) {
      const String y = stack_.pop();
      const String x = stack_.pop();
      stack_.push((x != "0" || y != "0") ? "1" : "0");
    } else if (token == "lt" && stack_.size() >= 2) {
      const int y = stack_.pop().toInt();
      const int x = stack_.pop().toInt();
      stack_.push((x < y) ? "1" : "0");
    } else if (token == "le" && stack_.size() >= 2) {
      const int y = stack_.pop().toInt();
      const int x = stack_.pop().toInt();
      stack_.push((x <= y) ? "1" : "0");
    } else if (token == "gt" && stack_.size() >= 2) {
      const int y = stack_.pop().toInt();
      const int x = stack_.pop().toInt();
      stack_.push((x > y) ? "1" : "0");
    } else if (token == "ge" && stack_.size() >= 2) {
      const int y = stack_.pop().toInt();
      const int x = stack_.pop().toInt();
      stack_.push((x >= y) ? "1" : "0");
    } else if (token == "eq" && stack_.size() >= 2) {
      const int y = stack_.pop().toInt();
      const int x = stack_.pop().toInt();
      stack_.push((x == y) ? "1" : "0");
    } else if (token == "ne" && stack_.size() >= 2) {
      const int y = stack_.pop().toInt();
      const int x = stack_.pop().toInt();
      stack_.push((x != y) ? "1" : "0");
    } else if (token == "length" && stack_.size() >= 1) {
      const String x = stack_.pop();
      stack_.push(String(x.length()));
    } else if (token == "concat" && stack_.size() >= 2) {
      const String y = stack_.pop();
      const String x = stack_.pop();
      stack_.push(x + y);
    } else if (token == "substr" && stack_.size() >= 3) {
      const int length = stack_.pop().toInt();
      const int start = stack_.pop().toInt();
      const String str = stack_.pop();
      stack_.push(str.substring(start, start +length));
    } else if (token == "find" && stack_.size() >= 2) {
      const String sub = stack_.pop();
      const String str = stack_.pop();
      stack_.push(String(str.indexOf(sub)));
    } else if (token == "rfind" && stack_.size() >= 2) {
      const String sub = stack_.pop();
      const String str = stack_.pop();
      stack_.push(String(str.lastIndexOf(sub)));
    } else if (token == "replace" && stack_.size() >= 3) {
      const String after = stack_.pop();
      const String before = stack_.pop();
      String str = stack_.pop();
      str.replace(before, after);
      stack_.push(str);
    } else if (token == "format" && stack_.size() >= 4) {
      const int dec = stack_.pop().toInt();
      const int len = stack_.pop().toInt();
      const int sign = stack_.pop().toInt();
      const float num = stack_.pop().toFloat();
      String result;
      for (int i = 0; i < len; i++) result.concat(" ");
      if (sign && num >= 0) result.concat("+");
      if (dec > 0) {
        result.concat(String(num, dec));
      } else {
        result.concat(String((int)num));
      }
      result = result.substring(result.length() - len);
      stack_.push(result);
    } else if (token == "extract" && stack_.size() >= 3) {
      const String suffix = stack_.pop();
      const String prefix = stack_.pop();
      const String str = stack_.pop();
      int bgn = str.indexOf(prefix);
      if (bgn < 0) bgn = str.length(); else bgn += prefix.length();
      const int end = str.indexOf(suffix, bgn);
      if (end < 0) {
        stack_.push("");
      } else {
        stack_.push(str.substring(bgn, end));
      }
    } else if (token == "lookup" && stack_.size() >= 2) {
      const String key = stack_.pop();
      const String str = stack_.pop();
      const JSON json(str);
      if (json.type == JSON::OBJECT) {
        stack_.push(json.get(key));
      } else if (json.type == JSON::ARRAY) {
        stack_.push(json.get(key.toInt()));
      } else {
        stack_.push(json.value());
      }
    } else if (token == "print" && stack_.size() >= 4) {
      const int y = stack_.pop().toInt();
      const int x = stack_.pop().toInt();
      const int attr = stack_.pop().toInt();
      const String str = stack_.pop();
      for (size_t i = 0; i < str.length(); i++) {
        if (x + i >= WIDTH * 2) break;
        text_[WIDTH * 2 * y + x + i] = str[i];
        attr_[WIDTH * 2 * y + x + i] = attr;
      }
    } else if (token == "wget" && stack_.size() >= 1) {
      const String url = stack_.pop();
#ifdef ARDUINO
      String page;
      getHTTP(url, &page);
      stack_.push(page);
#else
      const uint32_t hash = fnv1Hash((uint8_t *)url.string().data(), url.length());
      const std::string file = "cache_" + std::to_string(hash) + ".dat";
      std::string buf;
      std::ifstream ifs(file);
      if (ifs) {
        std::getline(ifs, buf, '\0');
      } else {
        std::cerr << "Cache not found: '" << file << "' for '" << url.string() << "'\n";
      }
      stack_.push(String(buf));
#endif
    } else if (token == "date") {
      stack_.push(String(time_.tm_wday));
      stack_.push(String(time_.tm_mday));
      stack_.push(String(time_.tm_mon + 1));
      stack_.push(String(time_.tm_year + 1900));
    } else if (token == "battery") {
      stack_.push(String(battery_, 2));
    } else if (token == "wakeup" && stack_.size() >= 2) {
      const int min = stack_.pop().toInt();
      const int hour = stack_.pop().toInt();
      int duration = (hour - time_.tm_hour) * 60 + (min - time_.tm_min);
      if (duration < 0) duration += 24 * 60;
      sleep_time_ = (uint64_t)duration * 60 * 1000 * 1000;
    } else if (token == "return") {
      break;
    } else {
      for (int i = func_size_ - 1; i >= 0; i--) {
        if (token == funcs_[i].name) {
          run(funcs_[i].code);
          break;
        }
#ifndef ARDUINO
        if (i == 0) std::cerr << "Invalid token: " << token.string() << "\n";
#endif
      }
    }
  }
}

uint64_t Script::getSleepTime() {
  return sleep_time_;
}

void Script::getScreen(const uint8_t **text, const uint8_t **attr) {
  *text = text_;
  *attr = attr_;
#ifndef ARDUINO
  std::cerr << "----- Stack dump begin -----\n";
  while (stack_.size()) {
    std::cerr << stack_.pop().string() << "\n\n";
  }
  std::cerr << "----- Stack dump end -----\n";
#endif
}

String Script::getNextToken(const String &buf, size_t *ptr) {
  const size_t bgn = *ptr;
  if (*ptr >= buf.length()) return String("");

  for (; *ptr < buf.length(); ++*ptr) {
    const char c = buf[*ptr];
    if (c == '\t' || c == '\n' || c == '\r') break;
  }
  const String token = buf.substring(bgn, (int)*ptr);
  if (*ptr < buf.length()) ++*ptr;

  return (token.length() > 0) ? token : getNextToken(buf, ptr);
}

bool Script::getClause(const String &buf, size_t *ptr, String *code) {
  String token;
  int nest = 0;
  while ((token = getNextToken(buf, ptr)) != "") {
    if (token == "?") {
      nest++;
    } else if (token == ":") {
      if (nest == 0) return false;
    } else if (token == ";") {
      if (nest == 0) return true;
      nest--;
    }
    code->concat(token);
    code->concat("\t");
  }
  return true;
}
