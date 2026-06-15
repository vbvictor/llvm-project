// Verify that setting `AllowConstOverloads` to false disables the new
// behavior: a non-const member call that has a matching const overload is
// once again treated as mutating, so no warning fires.
//
// RUN: %check_clang_tidy %s performance-for-range-copy %t -- \
// RUN:     -config="{CheckOptions: {performance-for-range-copy.AllowConstOverloads: false}}" \
// RUN:     -- -fno-delayed-template-parsing

template <typename T>
struct Iterator {
  void operator++() {}
  const T &operator*() {
    static T *TT = new T();
    return *TT;
  }
  bool operator!=(const Iterator &) { return false; }
};

template <typename T>
struct View {
  T begin() { return T(); }
  T begin() const { return T(); }
  T end() { return T(); }
  T end() const { return T(); }
};

struct WithOverloads {
  WithOverloads();
  WithOverloads(const WithOverloads &);
  ~WithOverloads();
  WithOverloads &operator=(const WithOverloads &);
  int get() const;
  int get();
};

// With AllowConstOverloads = false, the non-const `get()` is considered
// mutating despite the matching const overload, so no warning is emitted.
void negativeValueReturn() {
  for (auto W : View<Iterator<WithOverloads>>()) {
    W.get();
  }
}
