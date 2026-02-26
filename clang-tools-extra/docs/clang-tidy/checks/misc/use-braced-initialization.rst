.. title:: clang-tidy - misc-use-braced-initialization

misc-use-braced-initialization
==============================

Suggests replacing parenthesized initialization with braced initialization for
variable declarations.

The rules for ``{}`` initialization are simpler, more general, less
ambiguous, and safer than for other forms of initialization.

Braced initialization has several advantages over parenthesized
initialization:

- **Prevents the most vexing parse.** ``Widget w()`` declares a
  function, not a variable. ``Widget w{}`` is unambiguous.
- **Catches narrowing conversions.** ``int x{3.14}`` is a
  compile-time error, while ``int x(3.14)`` silently truncates.
- **Uniform syntax.** Braces work consistently for aggregates,
  containers, and constructors, giving a single initialization
  style across all types.

For example:

.. code-block:: c++

  struct Matrix {
    Matrix(int rows, int cols);
  };

  // Variable declarations:
  Matrix m(3, 4);          // -> Matrix m{3, 4};
  int n(42);               // -> int n{42};

  // Copy initialization:
  Matrix m = Matrix(3, 4); // -> Matrix m = Matrix{3, 4};

  // Temporary objects:
  use(Matrix(3, 4));       // -> use(Matrix{3, 4});

  // New expressions:
  auto *p = new Matrix(3, 4); // -> auto *p = new Matrix{3, 4};

Limitations
-----------

The check skips cases where changing from ``()`` to ``{}`` would
alter program semantics:

- Types that have any constructor accepting
  ``std::initializer_list``, since braced initialization would
  prefer that overload and silently change semantics. This covers
  standard containers like ``std::vector`` and ``std::string``.
- Constructor calls whose arguments already contain braced
  initializer lists.
- Direct-initialized ``auto`` variables, where deduction rules
  may differ between C++ standards.
- Expressions in macro expansions.

.. note::

  Braced initialization prohibits implicit narrowing conversions.
  In some cases the suggested fix may introduce a compiler error
  when an argument type is wider than the parameter type. For
  example:

  .. code-block:: c++

    struct Foo {
      Foo(unsigned int n);
    };

    size_t n = 10;
    Foo f(n);  // OK: implicit narrowing allowed with ()
    Foo f{n};  // error: narrowing from size_t to unsigned int

  This produces ``-Wc++11-narrowing`` (or a hard error depending
  on compiler and standard mode). Add an explicit cast (e.g.,
  ``static_cast<unsigned int>(n)``) to fix it. This is intentional
  — braced initialization surfaces these implicit conversions so
  they can be reviewed.

References
----------

This check corresponds to the CERT C++ Core Guidelines rule
`C++ Core Guidelines ES.23
<https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es23-prefer-the--initializer-syntax>`_.