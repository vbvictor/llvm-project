// Verify that setting `AllowConstOverloads` to false disables the new
// behavior: a non-const member call that has a matching const overload is
// once again treated as mutating, so no warning fires.
//
// RUN: %check_clang_tidy %s misc-const-correctness %t -- \
// RUN:   -config="{CheckOptions: {\
// RUN:     misc-const-correctness.AllowConstOverloads: false, \
// RUN:     misc-const-correctness.AnalyzeParameters: false, \
// RUN:     misc-const-correctness.WarnPointersAsValues: false, \
// RUN:     misc-const-correctness.WarnPointersAsPointers: false \
// RUN:   }}" -- -fno-delayed-template-parsing

struct WithOverloads {
  int get() const;
  int get();
};

// With AllowConstOverloads = false, the call to `get()` is treated as
// mutating, so no `can be declared 'const'` warning is emitted.
void negative_value_return() {
  WithOverloads w;
  w.get();
}

struct OnlyNonConst {
  int get();
};

void still_negative_no_const_overload() {
  OnlyNonConst x;
  x.get();
}
