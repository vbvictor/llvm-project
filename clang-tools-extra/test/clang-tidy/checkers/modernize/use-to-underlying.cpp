// RUN: %check_clang_tidy -std=c++11 %s modernize-use-to-underlying %t -- -- -I%S/Inputs

// CHECK-FIXES: #include <type_traits>

namespace std {
template<typename T> struct underlying_type { typedef int type; };
template<typename T> using underlying_type_t = typename underlying_type<T>::type;
} // namespace std

// Scoped enum with explicit underlying type
enum class Color : int {
  Red = 0,
  Green = 1,
  Blue = 2
};

// Scoped enum with different underlying type
enum class Size : unsigned char {
  Small = 0,
  Medium = 1,
  Large = 2
};

// Unscoped enum with explicit underlying type
enum PlainEnum : int {
  First = 1,
  Second = 2
};

// Unscoped enum without explicit underlying type (should not trigger)
enum ImplicitEnum {
  Alpha,
  Beta,
  Gamma
};

void test_basic() {
  Color c = Color::Red;
  Size s = Size::Small;
  PlainEnum p = First;
  ImplicitEnum i = Alpha;

  // Scoped enum to matching underlying type
  auto color_val = static_cast<int>(c);
  // CHECK-MESSAGES: :[[@LINE-1]]:20: warning: use std::underlying_type_t instead of static_cast to convert enum to its underlying type [modernize-use-to-underlying]
  // CHECK-FIXES: auto color_val = static_cast<std::underlying_type_t<Color>>(c);

  // Scoped enum to different underlying type
  auto size_val = static_cast<unsigned char>(s);
  // CHECK-MESSAGES: :[[@LINE-1]]:19: warning: use std::underlying_type_t instead of static_cast to convert enum to its underlying type [modernize-use-to-underlying]
  // CHECK-FIXES: auto size_val = static_cast<std::underlying_type_t<Size>>(s);

  // Unscoped enum with explicit underlying type
  auto plain_val = static_cast<int>(p);
  // CHECK-MESSAGES: :[[@LINE-1]]:20: warning: use std::underlying_type_t instead of static_cast to convert enum to its underlying type [modernize-use-to-underlying]
  // CHECK-FIXES: auto plain_val = static_cast<std::underlying_type_t<PlainEnum>>(p);

  // Unscoped enum without explicit underlying type - should NOT trigger
  auto implicit_val = static_cast<int>(i);
  // No warning expected
}

void test_different_types() {
  Color c = Color::Blue;
  
  // Casting to different type than underlying
  auto as_long = static_cast<long>(c);
  // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: static_cast to a different type than the underlying type; consider using std::underlying_type_t first [modernize-use-to-underlying]
  // CHECK-FIXES: auto as_long = static_cast<long>(static_cast<std::underlying_type_t<Color>>(c));

  auto as_char = static_cast<char>(c);
  // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: static_cast to a different type than the underlying type; consider using std::underlying_type_t first [modernize-use-to-underlying]
  // CHECK-FIXES: auto as_char = static_cast<char>(static_cast<std::underlying_type_t<Color>>(c));
}

void test_complex_expressions() {
  Color colors[] = {Color::Red, Color::Green, Color::Blue};
  
  // Array element
  auto val1 = static_cast<int>(colors[0]);
  // CHECK-MESSAGES: :[[@LINE-1]]:15: warning: use std::underlying_type_t instead of static_cast to convert enum to its underlying type [modernize-use-to-underlying]
  // CHECK-FIXES: auto val1 = static_cast<std::underlying_type_t<Color>>(colors[0]);

  // Function call result
  auto get_color = []() { return Color::Green; };
  auto val2 = static_cast<int>(get_color());
  // CHECK-MESSAGES: :[[@LINE-1]]:15: warning: use std::underlying_type_t instead of static_cast to convert enum to its underlying type [modernize-use-to-underlying]
  // CHECK-FIXES: auto val2 = static_cast<std::underlying_type_t<enum Color>>(get_color());
}

// Test that we don't flag casts between enum types
enum class OtherEnum : int {
  X = 10,
  Y = 20
};

void test_enum_to_enum() {
  Color c = Color::Red;
  
  // This should not trigger - casting between enum types
  auto other = static_cast<OtherEnum>(c);
  // No warning expected
}

// Test with const and references
void test_const_and_refs() {
  const Color c = Color::Green;
  Color& c_ref = const_cast<Color&>(c);
  const Color& c_const_ref = c;
  
  auto val1 = static_cast<int>(c);
  // CHECK-MESSAGES: :[[@LINE-1]]:15: warning: use std::underlying_type_t instead of static_cast to convert enum to its underlying type [modernize-use-to-underlying]
  // CHECK-FIXES: auto val1 = static_cast<std::underlying_type_t<const Color>>(c);
  
  auto val2 = static_cast<int>(c_ref);
  // CHECK-MESSAGES: :[[@LINE-1]]:15: warning: use std::underlying_type_t instead of static_cast to convert enum to its underlying type [modernize-use-to-underlying]
  // CHECK-FIXES: auto val2 = static_cast<std::underlying_type_t<Color>>(c_ref);
  
  auto val3 = static_cast<int>(c_const_ref);
  // CHECK-MESSAGES: :[[@LINE-1]]:15: warning: use std::underlying_type_t instead of static_cast to convert enum to its underlying type [modernize-use-to-underlying]
  // CHECK-FIXES: auto val3 = static_cast<std::underlying_type_t<const Color>>(c_const_ref);
}


// Negative tests - these should not trigger warnings
void negative_tests() {
  int x = 42;
  
  // Not an enum
  auto val1 = static_cast<long>(x);
  
  // C-style cast (not static_cast)
  Color c = Color::Blue;
  auto val2 = (int)c;
  
  // reinterpret_cast
  auto val3 = reinterpret_cast<int*>(&c);
  
  // dynamic_cast (doesn't apply to enums anyway)
  struct Base { virtual ~Base() {} };
  struct Derived : Base {};
  Base* b = new Derived();
  auto d = dynamic_cast<Derived*>(b);
  delete b;
}