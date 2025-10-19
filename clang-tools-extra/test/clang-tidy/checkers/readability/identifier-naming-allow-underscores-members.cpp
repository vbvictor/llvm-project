// RUN: %check_clang_tidy %s readability-identifier-naming %t -- \
// RUN:   -config='{CheckOptions: { \
// RUN:     readability-identifier-naming.PrivateMemberCase: lower_case, \
// RUN:     readability-identifier-naming.PrivateMemberAllowTrailingUnderscore: true, \
// RUN:     readability-identifier-naming.PublicMemberCase: CamelCase, \
// RUN:     readability-identifier-naming.PublicMemberAllowLeadingUnderscore: true, \
// RUN:     readability-identifier-naming.ProtectedMemberCase: lower_case, \
// RUN:     readability-identifier-naming.ProtectedMemberAllowLeadingUnderscore: true, \
// RUN:     readability-identifier-naming.ProtectedMemberAllowTrailingUnderscore: true, \
// RUN:   }}'

class TestClass {
private:
  int private_member_;
  // No warning - trailing underscore allowed for private members

  int private_member;
  // No warning - correct style

  int _private_member;
  // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: invalid case style for private member '_private_member' [readability-identifier-naming]
  // CHECK-FIXES: int private_member;
  // Leading underscore not allowed for private members

  int PrivateMember;
  // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: invalid case style for private member 'PrivateMember' [readability-identifier-naming]
  // CHECK-FIXES: int private_member;

public:
  int _PublicMember;
  // No warning - leading underscore allowed for public members

  int PublicMember;
  // No warning - correct style

  int public_member;
  // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: invalid case style for public member 'public_member' [readability-identifier-naming]
  // CHECK-FIXES: int PublicMember;

  int PublicMember_;
  // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: invalid case style for public member 'PublicMember_' [readability-identifier-naming]
  // CHECK-FIXES: int PublicMember;
  // Trailing underscore not allowed for public members

protected:
  int _protected_member_;
  // No warning - both leading and trailing underscores allowed for protected

  int protected_member_;
  // No warning - trailing underscore allowed

  int _protected_member;
  // No warning - leading underscore allowed

  int protected_member;
  // No warning - correct style

  int ProtectedMember;
  // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: invalid case style for protected member 'ProtectedMember' [readability-identifier-naming]
  // CHECK-FIXES: int protected_member;
};
