// RUN: %check_clang_tidy %s bugprone-pass-log-params %t

namespace std {

template <typename T>
struct basic_string {
  const char* c_str() const;
  const char* data() const;
};

typedef basic_string<char> string;

}

namespace log {

template<typename... Ts>
void info(const Ts&... args) {}

}

namespace http {
namespace log {
template<typename... Ts>
void info(const Ts&... args) {}
}
}

// Test basic format string parameter count matching
void test_parameter_count() {
  std::string str;
  log::info("Test: %s");  // missing argument
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: format string requires 1 arguments but 0 were provided

  log::info("Test: %s %d", str);  // missing argument
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: format string requires 2 arguments but 1 were provided

  log::info("Test: %s", str, 42);  // extra argument
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: format string requires 1 arguments but 2 were provided
}

// Test type checking
void test_type_checking() {
  std::string str;
  int num = 42;
  float f = 3.14f;
  
  log::info("Int: %d", str);  // wrong type
  // CHECK-MESSAGES: :[[@LINE-1]]:24: warning: argument type <std::string> does not match format specifier '%d'

  log::info("Float: %f", num);  // wrong type
  // CHECK-MESSAGES: :[[@LINE-1]]:26: warning: argument type <int> does not match format specifier '%f'

  log::info("String: %s", f);  // wrong type
  // CHECK-MESSAGES: :[[@LINE-1]]:27: warning: argument type <float> does not match format specifier '%s'
}

void test_unknown_log_like_function() {
  std::string str;
  http::log::info("Test: %s", str); // not a default log-like function
}