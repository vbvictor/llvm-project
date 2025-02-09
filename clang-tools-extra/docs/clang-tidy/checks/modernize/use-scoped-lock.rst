.. title:: clang-tidy - modernize-use-scoped-lock

modernize-use-scoped-lock
=========================

Finds uses of ``std::lock_guard`` and suggests replacing them with C++17's more
flexible and safer alternative ``std::scoped_lock``. The check will
automatically transform only single declarations of ``std::lock_guard`` and
emit warnings for multiple declarations of ``std::lock_guard`` that can be
replaced with a single declaration of ``std::scoped_lock``.

Examples
--------

Single ``std::lock_guard`` declaration:

.. code-block:: c++

  std::mutex M;
  std::lock_guard<std::mutex> L(M);


Transforms to:

.. code-block:: c++

  std::mutex m;
  std::scoped_lock L(M);

Single ``std::lock_guard`` declaration with ``std::adopt_lock``:

.. code-block:: c++

  std::mutex M;
  std::lock(M);
  std::lock_guard<std::mutex> L(M, std::adopt_lock);


Transforms to:

.. code-block:: c++

  std::mutex M;
  std::lock(M);
  std::scoped_lock L(std::adopt_lock, M);

Multiple ``std::lock_guard`` declarations only emit warnings:

.. code-block:: c++

  std::mutex M1, M2;
  std::lock(M1, M2);
  std::lock_guard Lock1(M, std::adopt_lock); // warning: use single 'std::scoped_lock' instead of multiple 'std::lock_guard'
  std::lock_guard Lock2(M, std::adopt_lock); // note: additional 'std::lock_guard' declared here


Limitations
-----------

The check will not emit warnings if ``std::lock_guard`` is used implicitly via
``using``, ``typedef`` or ``template``.

.. code-block:: c

  template <template <typename> typename Lock>
  void TemplatedLock() {
    std::mutex m;
    Lock<std::mutex> l(m); // no warning
  }

  void UsingLock() {
    using Lock = std::lock_guard<std::mutex>;
    std::mutex m;
    Lock l(m); // no warning
  }

.. option:: WarnOnlyMultipleLocks

  When `true`, the check will only emit warnings if the there are multiple
  consecutive ``std::lock_guard`` declarations that can be replaced with a
  single ``std::scoped_lock`` declaration. Default is `false`.
