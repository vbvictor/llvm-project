// RUN: %check_clang_tidy -std=c++11-or-later %s misc-use-braced-initialization %t \
// RUN:   -- -- -I %S/../Inputs/Headers

#include <string>
#include <vector>

struct Simple {
  Simple(int);
  Simple(int, double);
  Simple(const Simple &);
};

struct Explicit {
  explicit Explicit(int);
};

struct WithInitializer {
  WithInitializer(std::initializer_list<int>);
  WithInitializer(int);
  WithInitializer(int, int);
};

struct Aggregate {
  int a, b;
};

struct Takes {
  Takes(Aggregate);
};

struct TwoAggs {
  TwoAggs(Aggregate, Aggregate);
};

struct CtorWithDefault {
  CtorWithDefault(int, int = 0);
};

struct Outer {
  struct Inner {
    Inner(int);
  };
};

namespace ns {
struct Ns {
  Ns(int);
};
} // namespace ns

#define MAKE_SIMPLE(x) Simple w(x)
#define WRAP_PARENS(x) (x)
#define TYPE_ALIAS Simple

void basic_single_arg() {
  Simple w(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: use braced initialization instead of parenthesized initialization [misc-use-braced-initialization]
  // CHECK-FIXES: Simple w{1};
}

void basic_multiple_args() {
  Simple w(1, 2.0);
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: use braced initialization instead of parenthesized initialization [misc-use-braced-initialization]
  // CHECK-FIXES: Simple w{1, 2.0};
}

void explicit_ctor() {
  Explicit e(42);
  // CHECK-MESSAGES: :[[@LINE-1]]:12: warning: use braced initialization
  // CHECK-FIXES: Explicit e{42};
}

void copy_construction() {
  Simple w1(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: use braced initialization
  // CHECK-FIXES: Simple w1{1};
  Simple w2(w1);
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: use braced initialization
  // CHECK-FIXES: Simple w2{w1};
}

void static_local() {
  static Simple sw(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:17: warning: use braced initialization
  // CHECK-FIXES: static Simple sw{1};
}

void const_variable() {
  const Simple cw(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:16: warning: use braced initialization
  // CHECK-FIXES: const Simple cw{1};
}

void default_args_ctor() {
  CtorWithDefault m(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:19: warning: use braced initialization
  // CHECK-FIXES: CtorWithDefault m{1};
}

void nested_type() {
  Outer::Inner oi(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:16: warning: use braced initialization
  // CHECK-FIXES: Outer::Inner oi{1};
}

void namespaced_type() {
  ns::Ns g(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: use braced initialization
  // CHECK-FIXES: ns::Ns g{1};
}

void for_loop_init() {
  for (Simple fw(1);;) {
    // CHECK-MESSAGES: :[[@LINE-1]]:15: warning: use braced initialization
    // CHECK-FIXES: for (Simple fw{1};;) {
    break;
  }
}

void expression_arg() {
  Simple w(1 + 2);
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: use braced initialization
  // CHECK-FIXES: Simple w{1 + 2};
}

void variable_arg(int x) {
  Simple w(x);
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: use braced initialization
  // CHECK-FIXES: Simple w{x};
}

void multiple_vars_in_scope() {
  Simple a(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: use braced initialization
  // CHECK-FIXES: Simple a{1};
  Simple b(2);
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: use braced initialization
  // CHECK-FIXES: Simple b{2};
}

// Macro wraps only the type name; parens are in user code, safe to fix.
void macro_type_only() {
  TYPE_ALIAS w(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:14: warning: use braced initialization
  // CHECK-FIXES: TYPE_ALIAS w{1};
}

void temporary_single_arg() {
  Simple(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: use braced initialization
  // CHECK-FIXES: Simple{1};
}

void temporary_multi_arg() {
  Simple(1, 2.0);
  // CHECK-MESSAGES: :[[@LINE-1]]:3: warning: use braced initialization
  // CHECK-FIXES: Simple{1, 2.0};
}

void temporary_cast_to_void() {
  (void)Simple(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:9: warning: use braced initialization
  // CHECK-FIXES: (void)Simple{1};
}

void direct_auto() {
  auto w(1);
}

void copy_init_rhs() {
  Simple w = Simple(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:14: warning: use braced initialization
  // CHECK-FIXES: Simple w = Simple{1};
}

void auto_copy_init() {
  auto w = Simple(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:12: warning: use braced initialization
  // CHECK-FIXES: auto w = Simple{1};
}

void already_braced() {
  Simple w{1};
}

void already_braced_temporary() {
  Simple{1};
}

struct HasDefault {
  HasDefault();
};

void default_construction() {
  HasDefault d;
}

// Types with std::initializer_list constructors are skipped because
// braced init would prefer the initializer_list overload.
void string_from_literal() {
  std::string s("hello");
}

void string_count_char() {
  std::string s(3, 'a');
}

void vector_single_arg() {
  std::vector<int> v(5);
}

void vector_count_value() {
  std::vector<int> v(5, 1);
}

void user_init_list_type() {
  WithInitializer mc(5);
}

void user_init_list_type_multi_arg() {
  WithInitializer mc(5, 3);
}

// initializer_list as second param is NOT an initializer-list constructor
// per [dcl.init.list], so converting () to {} is safe.
struct InitListSecondParam {
  InitListSecondParam(int, std::initializer_list<int>);
  InitListSecondParam(int, int);
};

void init_list_not_first_param() {
  InitListSecondParam x(1, 2);
  // CHECK-MESSAGES: :[[@LINE-1]]:23: warning: use braced initialization
  // CHECK-FIXES: InitListSecondParam x{1, 2};
}

// Pointer to initializer_list is not an initializer-list constructor.
struct InitListPointer {
  InitListPointer(std::initializer_list<int> *);
  InitListPointer(int);
};

void init_list_pointer() {
  InitListPointer x(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:19: warning: use braced initialization
  // CHECK-FIXES: InitListPointer x{1};
}

// Reference to initializer_list IS an initializer-list constructor.
struct InitListRef {
  InitListRef(const std::initializer_list<int> &);
  InitListRef(int);
};

void init_list_ref() {
  InitListRef x(1);
}

struct InitListRvalueRef {
  InitListRvalueRef(std::initializer_list<int> &&);
  InitListRvalueRef(int);
};

void init_list_rvalue_ref() {
  InitListRvalueRef x(1);
}

void vector_temporary() {
  std::vector<int>(5);
}

void macro_full_decl() {
  MAKE_SIMPLE(1);
}

// Parens come from macro expansion.
void macro_wraps_parens() {
  Simple w WRAP_PARENS(1);
}

template <typename T>
void template_dependent() {
  T t(1);
}

// Fix would modify template source, which could break other instantiations.
template <typename T>
void template_instantiated(int x) {
  T t(x);
}

template <typename T>
void template_instantiated2(T x) {
  auto t(x);
}

void force_instantiation() { 
  template_instantiated<Simple>(1);
  template_instantiated2<Simple>(1);
}

// Braced-init-list argument constructs an aggregate, safe to convert
// because Takes has no std::initializer_list constructor.
void braced_arg() {
  Takes tp({1, 2});
  // CHECK-MESSAGES: :[[@LINE-1]]:9: warning: use braced initialization
  // CHECK-FIXES: Takes tp{{[{][{]}}1, 2{{[}][}]}};
}

void vector_already_braced() {
  std::vector<int> v{1, 2, 3};
}

void new_expression() {
  Simple *p = new Simple(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:19: warning: use braced initialization
  // CHECK-FIXES: Simple *p = new Simple{1};
  (void)p;
}

void new_already_braced() {
  Simple *p = new Simple{1};
  (void)p;
}

void new_init_list_type() {
  std::vector<int> *p = new std::vector<int>(5);
  (void)p;
}

void scalar_int() {
  int x(42);
  // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: use braced initialization
  // CHECK-FIXES: int x{42};
}

void scalar_double() {
  double d(3.14);
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: use braced initialization
  // CHECK-FIXES: double d{3.14};
}

void scalar_expression(int a) {
  int y(a + 1);
  // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: use braced initialization
  // CHECK-FIXES: int y{a + 1};
}

void scalar_pointer() {
  int *p(nullptr);
  // CHECK-MESSAGES: :[[@LINE-1]]:8: warning: use braced initialization
  // CHECK-FIXES: int *p{nullptr};
}

void scalar_bool() {
  bool b(true);
  // CHECK-MESSAGES: :[[@LINE-1]]:8: warning: use braced initialization
  // CHECK-FIXES: bool b{true};
}

void scalar_already_braced() {
  int x{42};
}

void scalar_copy_init() {
  int x = 42;
}

void scalar_auto() {
  auto x(42);
}

// Comments between tokens should not prevent matching.
void class_comment_before_parens() {
  Simple w /*comment*/ (1);
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: use braced initialization
  // CHECK-FIXES: Simple w /*comment*/ {1};
}

void class_comment_inside_parens() {
  Simple w(/*comment*/ 1);
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: use braced initialization
  // CHECK-FIXES: Simple w{/*comment*/ 1};
}

void scalar_comment_before_parens() {
  int x /*comment*/ (42);
  // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: use braced initialization
  // CHECK-FIXES: int x /*comment*/ {42};
}

void scalar_comment_inside_parens() {
  int x(/*comment*/ 42);
  // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: use braced initialization
  // CHECK-FIXES: int x{/*comment*/ 42};
}

void scalar_comment_after_init() {
  int x(42 /*comment*/);
  // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: use braced initialization
  // CHECK-FIXES: int x{42 /*comment*/};
}

// Multiple declarations in a single statement.
void multi_decl_class() {
  Simple a(1), b(2);
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: use braced initialization
  // CHECK-MESSAGES: :[[@LINE-2]]:16: warning: use braced initialization
  // CHECK-FIXES: Simple a{1}, b{2};
}

void multi_decl_scalar() {
  int a(1), b(2);
  // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: use braced initialization
  // CHECK-MESSAGES: :[[@LINE-2]]:13: warning: use braced initialization
  // CHECK-FIXES: int a{1}, b{2};
}

Simple return_simple() {
  return Simple(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: use braced initialization
  // CHECK-FIXES: return Simple{1};
}

void func_arg(Simple);
void simple_as_argument() {
  func_arg(Simple(1));
  // CHECK-MESSAGES: :[[@LINE-1]]:12: warning: use braced initialization
  // CHECK-FIXES: func_arg(Simple{1});
}

void ternary_arg(bool c) {
  Simple s(c ? 1 : 2);
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: use braced initialization
  // CHECK-FIXES: Simple s{c ? 1 : 2};
}

// VarDecl::InitializationStyle coverage:
//
// CallInit — direct initialization with parentheses.
// Already covered by basic_single_arg, scalar_int, etc.
void init_style_call_init() {
  Simple s(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:10: warning: use braced initialization
  // CHECK-FIXES: Simple s{1};
  int i(42);
  // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: use braced initialization
  // CHECK-FIXES: int i{42};
}

// CInit — copy initialization with '='.
// The VarDecl has CInit style, so hasCallInitStyle does not match.
// The RHS temporary Simple(1) is still caught by the functional cast matcher.
void init_style_c_init() {
  Simple s = Simple(1);
  // CHECK-MESSAGES: :[[@LINE-1]]:14: warning: use braced initialization
  // CHECK-FIXES: Simple s = Simple{1};
  int i = 42;
}

// ListInit — direct list initialization with braces.
// Already using braces, nothing to do.
void init_style_list_init() {
  Simple s{1};
  int i{42};
}

// ParenListInit — C++20 parenthesized aggregate initialization.
// Tested in use-braced-initialization-cxx20.cpp.

// Braced-init-list args: safe when the type has no initializer_list ctor.
void braced_constructed_arg() {
  Takes tp(Aggregate{1, 2});
  // CHECK-MESSAGES: :[[@LINE-1]]:9: warning: use braced initialization
  // CHECK-FIXES: Takes tp{Aggregate{1, 2}};
}

void multiple_braced_args() {
  TwoAggs t({1, 2}, {3, 4});
  // CHECK-MESSAGES: :[[@LINE-1]]:11: warning: use braced initialization
  // CHECK-FIXES: TwoAggs t{{[{][{]}}1, 2}, {3, 4{{[}][}]}};
}

void temporary_braced_arg() {
  (void)Takes({1, 2});
  // CHECK-MESSAGES: :[[@LINE-1]]:9: warning: use braced initialization
  // CHECK-FIXES: (void)Takes{{[{][{]}}1, 2{{[}][}]}};
}

void new_braced_arg() {
  Takes *p = new Takes({1, 2});
  // CHECK-MESSAGES: :[[@LINE-1]]:18: warning: use braced initialization
  // CHECK-FIXES: Takes *p = new Takes{{[{][{]}}1, 2{{[}][}]}};
  (void)p;
}

// Type with initializer_list ctor is skipped even with braced-init-list
// args — constructsTypeWithInitListCtor guard is sufficient.
void init_list_type_braced_arg() {
  WithInitializer wi({1, 2, 3});
}
