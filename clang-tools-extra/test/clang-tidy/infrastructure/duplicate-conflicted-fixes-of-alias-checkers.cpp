// RUN: %check_clang_tidy %s cppcoreguidelines-pro-type-member-init,hicpp-member-init,modernize-use-emplace,hicpp-use-emplace %t -- \
//// RUN:     -config='{CheckOptions: { \
//// RUN:         cppcoreguidelines-pro-type-member-init.UseAssignment: true, \
//// RUN:     }}'

// CHECK-MESSAGES: warning: found alias checks: 'hicpp-member-init' is an alias of 'cppcoreguidelines-pro-type-member-init', 'modernize-use-emplace' is an alias of 'hicpp-use-emplace' [clang-tidy-config]
// CHECK-MESSAGES: note: please disable the alias check to avoid running duplicate checks

class Foo {
public:
  Foo() : _num1(0)
  // CHECK-MESSAGES: warning: constructor does not initialize these fields: _num2 [cppcoreguidelines-pro-type-member-init,hicpp-member-init]
  // CHECK-MESSAGES: note: cannot apply fix-it because an alias checker has suggested a different fix-it; please remove one of the checkers ('cppcoreguidelines-pro-type-member-init', 'hicpp-member-init') or ensure they are both configured the same
  {
    _num1 = 10;
  }

  int use_the_members() const {
    return _num1 + _num2;
  }

private:
  int _num1;
  int _num2;
  // CHECK-FIXES: int _num2;
};
