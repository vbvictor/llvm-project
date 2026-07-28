// RUN: grep -Ev "// *[A-Z-]+:" %s > %t-input.cpp
// RUN: clang-tidy %t-input.cpp -checks='-*,bugprone-assignment-in-if-condition' -export-sarif=%t.sarif -- > %t.msg 2>&1
// RUN: FileCheck -input-file=%t.sarif -check-prefix=CHECK-SARIF %s
void f() {
  int x;
  if (x = 1) {}
}

// CHECK-SARIF: "$schema": "https://docs.oasis-open.org/sarif/sarif/v2.1.0/cos02/schemas/sarif-schema-2.1.0.json",
// CHECK-SARIF:      "artifacts": [
// CHECK-SARIF:            "uri": "file://{{.*}}-input.cpp"
// CHECK-SARIF:          "roles": [
// CHECK-SARIF-NEXT:       "resultFile"
// CHECK-SARIF:      "results": [
// CHECK-SARIF-NEXT:   {
// CHECK-SARIF:              "importance": "important",
// CHECK-SARIF:                  "text": "if it should be an assignment, move it out of the 'if' condition"
// CHECK-SARIF:                    "uri": "file://{{.*}}-input.cpp"
// CHECK-SARIF:                "region": {
// CHECK-SARIF-NEXT:             "endColumn": 9,
// CHECK-SARIF-NEXT:             "startColumn": 9,
// CHECK-SARIF-NEXT:             "startLine": 3
// CHECK-SARIF:              "importance": "important",
// CHECK-SARIF:                  "text": "if it is meant to be an equality check, change '=' to '=='"
// CHECK-SARIF:        "level": "warning",
// CHECK-SARIF:        "locations": [
// CHECK-SARIF:              "uri": "file://{{.*}}-input.cpp"
// CHECK-SARIF:              "region": {
// CHECK-SARIF-NEXT:           "endColumn": 12,
// CHECK-SARIF-NEXT:           "endLine": 3,
// CHECK-SARIF-NEXT:           "startColumn": 7,
// CHECK-SARIF-NEXT:           "startLine": 3
// CHECK-SARIF:        "message": {
// CHECK-SARIF-NEXT:     "text": "an assignment within an 'if' condition is bug-prone"
// CHECK-SARIF:        "ruleId": "bugprone-assignment-in-if-condition",
// CHECK-SARIF-NEXT:   "ruleIndex": 0
// CHECK-SARIF:      "tool": {
// CHECK-SARIF-NEXT:   "driver": {
// CHECK-SARIF:            "fullName": "clang-tidy",
// CHECK-SARIF-NEXT:       "informationUri": "https://clang.llvm.org/docs/UsersManual.html",
// CHECK-SARIF-NEXT:       "language": "en-US",
// CHECK-SARIF-NEXT:       "name": "clang-tidy",
// CHECK-SARIF-NEXT:       "rules": [
// CHECK-SARIF-NEXT:         {
// CHECK-SARIF:                "helpUri": "https://clang.llvm.org/extra/clang-tidy/checks/bugprone/assignment-in-if-condition.html",
// CHECK-SARIF-NEXT:           "id": "bugprone-assignment-in-if-condition",
// CHECK-SARIF-NEXT:           "name": "bugprone-assignment-in-if-condition"
// CHECK-SARIF: "version": "2.1.0"
