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

using int8_t = signed char;
using uint8_t = unsigned char;
using uint64_t = unsigned long long;

// Test integer width and signedness
void test_integer_types() {
  int8_t i8 = 1;
  uint8_t u8 = 1;
  short i16 = 1;
  unsigned short u16 = 1;
  int i32 = 1;
  unsigned int u32 = 1;
  long long i64 = 1;
  uint64_t u64 = 1;

  log::info("Int8: %hhd", i8);    // OK
  log::info("Int8: %d", i8);      // expected 32-bit
  // CHECK-MESSAGES: :[[@LINE-1]]:25: warning: argument type <int8_t> does not match format specifier '%d'

  log::info("UInt8: %hhu", u8);   // OK
  log::info("UInt8: %d", u8);     // expected signed
  // CHECK-MESSAGES: :[[@LINE-1]]:26: warning: argument type <uint8_t> does not match format specifier '%d'

  log::info("Int16: %hd", i16);   // OK
  log::info("Int16: %d", i16);    // expected 32-bit
  // CHECK-MESSAGES: :[[@LINE-1]]:26: warning: argument type <short> does not match format specifier '%d'

  log::info("UInt16: %hu", u16);  // OK
  log::info("UInt16: %d", u16);   // expected signed 32-bit
  // CHECK-MESSAGES: :[[@LINE-1]]:27: warning: argument type <unsigned short> does not match format specifier '%d'

  log::info("Int32: %d", i32);    // OK
  log::info("UInt32: %u", u32);   // OK
  log::info("UInt32: %d", u32);   // expected signed
  // CHECK-MESSAGES: :[[@LINE-1]]:27: warning: argument type <unsigned int> does not match format specifier '%d'

  log::info("Int64: %lld", i64);  // OK
  log::info("Int64: %d", i64);    // expected 32-bit
  // CHECK-MESSAGES: :[[@LINE-1]]:26: warning: argument type <long long> does not match format specifier '%d'

  log::info("UInt64: %llu", u64); // OK
  log::info("UInt64: %d", u64);   // expected signed 32-bit
  // CHECK-MESSAGES: :[[@LINE-1]]:27: warning: argument type <uint64_t> does not match format specifier '%d'
}
// Test floating point types
void test_float_types() {
  float f = 1.0f;
  double d = 1.0;

  log::info("Float: %f", f);      // OK
  log::info("Float: %lf", f);     // expected double
  // CHECK-MESSAGES: :[[@LINE-1]]:27: warning: argument type <float> does not match format specifier '%lf'

  log::info("Double: %lf", d);    // OK
  log::info("Double: %f", d);     // expected float
  // CHECK-MESSAGES: :[[@LINE-1]]:27: warning: argument type <double> does not match format specifier '%f'

  log::info("LDouble: %f", d);   // expected float
  // CHECK-MESSAGES: :[[@LINE-1]]:28: warning: argument type <double> does not match format specifier '%f'
}

// Test character types
void test_char_types() {
  char c = 'a';
  wchar_t wc = L'a';
  int i = 65;

  log::info("Char: %c", c);       // OK
  log::info("Char: %c", wc);      // expected 8-bit
  // CHECK-MESSAGES: :[[@LINE-1]]:25: warning: argument type <wchar_t> does not match format specifier '%c'
  log::info("Char: %c", i);       // expected 8-bit
  // CHECK-MESSAGES: :[[@LINE-1]]:25: warning: argument type <int> does not match format specifier '%c'
}

// Test string types
void test_string_types() {
  const char* cstr;
  std::string str;
  int* ptr = nullptr;

  log::info("String: %s", cstr);  // OK
  log::info("String: %s", str);   // OK
  log::info("String: %s", ptr);   // expected char*
  // CHECK-MESSAGES: :[[@LINE-1]]:27: warning: argument type <int *> does not match format specifier '%s'
}

// Test pointer types
void test_pointer_types() {
  int* ptr = nullptr;
  const char* cstr;

  log::info("Pointer: %p", ptr);   // OK
  log::info("Pointer: %p", cstr);  // OK
  log::info("Pointer: %p", 42);    // expected pointer
  // CHECK-MESSAGES: :[[@LINE-1]]:28: warning: argument type <int> does not match format specifier '%p'
}

// Test multiple wrong values in format string
void test_multiple_wrong_values() {
  int i32 = 42;
  long long i64 = 42;
  float f = 3.14f;
  std::string str;
  
  // Multiple wrong types
  log::info("Values: %d %f %s", i64, i32, f);
  // CHECK-MESSAGES: :[[@LINE-1]]:33: warning: argument type <long long> does not match format specifier '%d'
  // CHECK-MESSAGES: :[[@LINE-2]]:38: warning: argument type <int> does not match format specifier '%f'
  // CHECK-MESSAGES: :[[@LINE-3]]:43: warning: argument type <float> does not match format specifier '%s'

  // Wrong types with length modifiers
  log::info("Values: %hd %lld %lf", i32, i32, f);
  // CHECK-MESSAGES: :[[@LINE-1]]:37: warning: argument type <int> does not match format specifier '%hd'
  // CHECK-MESSAGES: :[[@LINE-2]]:42: warning: argument type <int> does not match format specifier '%lld'
  // CHECK-MESSAGES: :[[@LINE-3]]:47: warning: argument type <float> does not match format specifier '%lf'

  // Mixed correct and wrong types
  log::info("Mix: %d %s %f %d", i32, str, i32, f);
  // CHECK-MESSAGES: :[[@LINE-1]]:43: warning: argument type <int> does not match format specifier '%f'
  // CHECK-MESSAGES: :[[@LINE-2]]:48: warning: argument type <float> does not match format specifier '%d'
}
