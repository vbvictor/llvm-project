.. title:: clang-tidy - modernize-use-to-underlying

modernize-use-to-underlying
============================

Finds and replaces ``static_cast`` of scoped enums to their underlying type
with ``std::to_underlying`` (in C++23 and later) or suggests using 
``std::underlying_type_t`` (in C++11 through C++20). This improves code 
robustness by automatically adapting to changes in the enum's underlying type.

The check detects cases where an enum value is cast to an integral type using
``static_cast`` and suggests using a more robust alternative that adapts to
underlying type changes.

Example
-------

.. code-block:: c++

  enum class EType : int {
    FIRST = 0,
    SECOND = 1
  };

  void process(EType e) {
    // Before (brittle if underlying type changes):
    auto value = static_cast<int>(e);
    
    // After (C++23):
    auto value = std::to_underlying(e);
    
    // After (C++11-C++20):
    auto value = static_cast<std::underlying_type_t<EType>>(e);
  }

Why This Check Matters
-----------------------

Using ``static_cast`` to convert an enum to its underlying type is brittle:

1. **Silent bugs on type changes**: If the enum's underlying type is changed
   (e.g., from ``int`` to ``unsigned char``), the compiler won't warn about 
   the mismatch since an explicit cast was used.

2. **Type safety**: When the type of the variable changes without updating
   the cast, no compiler warning is issued:

   .. code-block:: c++

     enum Enum : int { A, B, C };
     enum AnotherEnum : int { D, E, F };
     
     void foo(AnotherEnum e) {
       // Bug: casting wrong enum type, but no warning
       auto i = static_cast<std::underlying_type_t<Enum>>(e);
     }

3. **Clarity**: Using ``std::to_underlying`` makes the intent clear - you want
   the underlying value, regardless of what type that is.

Exceptions
----------

The check does not flag unscoped enums without an explicitly specified 
underlying type, as their underlying type is implementation-defined:

.. code-block:: c++

  enum PlainEnum { A, B, C };  // Underlying type is implementation-defined
  
  void foo(PlainEnum e) {
    auto i = static_cast<int>(e);  // Not flagged
  }

Options
-------

.. option:: UnderlyingFunction

   Specifies a custom function name to use instead of ``std::to_underlying``.
   This is useful for projects that cannot use C++23 but have their own
   implementation of ``to_underlying``. Default is empty (use standard library).

   Example configuration:

   .. code-block:: yaml

     CheckOptions:
       - key:             modernize-use-to-underlying.UnderlyingFunction
         value:           'my_project::to_underlying'

.. option:: UnderlyingHeader

   Specifies the header file to include for the custom underlying function.
   Only used when ``UnderlyingFunction`` is set. Can be either a system header
   (e.g., ``<my_utils.h>``) or a project header (e.g., ``"utils/enum.h"``).
   Default is empty.

   Example configuration:

   .. code-block:: yaml

     CheckOptions:
       - key:             modernize-use-to-underlying.UnderlyingHeader
         value:           '"my_project/enum_utils.h"'

.. option:: IncludeStyle

   A string specifying which include-style is used, `llvm` or `google`. Default
   is `llvm`.

Custom Implementation Example
------------------------------

For projects not yet on C++23, you can provide your own ``to_underlying``:

.. code-block:: c++

  // In your project's enum_utils.h
  namespace my_project {
    template<typename E>
    constexpr auto to_underlying(E e) noexcept {
      return static_cast<std::underlying_type_t<E>>(e);
    }
  }

Then configure the check:

.. code-block:: yaml

  CheckOptions:
    - key:             modernize-use-to-underlying.UnderlyingFunction
      value:           'my_project::to_underlying'
    - key:             modernize-use-to-underlying.UnderlyingHeader
      value:           '"enum_utils.h"'