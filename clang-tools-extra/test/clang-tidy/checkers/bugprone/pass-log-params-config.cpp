// RUN: %check_clang_tidy %s bugprone-pass-log-params %t \
// RUN: -config='{CheckOptions: \
// RUN: {bugprone-pass-log-params.LogLikeFunctions: "http::log::info"}} --'


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
void test_found_http_log_like_function() {
  std::string str;
  http::log::info("Test: %s");  // missing argument
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: format string requires 1 arguments but 0 were provided
}

// Test type checking
void test_default_log_like_functions_disabled() {
  float f = 3.14f;
  log::info("String: %s", f);  // print no diagnostics
}