// RUN: %check_clang_tidy -std=c++17-or-later %s modernize-use-scoped-lock %t -- \
// RUN:   -config="{CheckOptions: {modernize-use-scoped-lock.WarnOnUsingAndTypedef: false}}" \
// RUN:   -- -fno-delayed-template-parsing

namespace std {

struct mutex {
  void lock() {}
  void unlock() {}
};

template<class Lockable1, class Lockable2, class... LockableN >
void lock(Lockable1& lock1, Lockable2& lock2, LockableN&... lockn );

struct adopt_lock_t { };
std::adopt_lock_t adopt_lock {};

template <typename Mutex>
struct lock_guard {
  lock_guard(Mutex &m) { }
  lock_guard(Mutex &m, std::adopt_lock_t t) {}
  lock_guard( const lock_guard& ) = delete;
};

template <typename... MutexTypes>
struct scoped_lock {
  scoped_lock(MutexTypes&... m) {}
  scoped_lock(std::adopt_lock_t t, MutexTypes&... m) {}
};

} // namespace std

template <typename T>
using Lock = std::lock_guard<T>;

using LockM = std::lock_guard<std::mutex>;

typedef std::lock_guard<std::mutex> LockDef;

void PositiveUsingDecl() {
  using std::lock_guard;

  using LockMFun = std::lock_guard<std::mutex>;
  
  typedef std::lock_guard<std::mutex> LockDefFun;
}

template <typename T>
void PositiveUsingDeclTemplate() {
  using std::lock_guard;

  using LockFunT = std::lock_guard<T>;

  using LockMFunT = std::lock_guard<std::mutex>;

  typedef std::lock_guard<std::mutex> LockDefFunT;
}