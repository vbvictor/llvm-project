.. title:: clang-tidy - performance-for-range-copy

performance-for-range-copy
==========================

Finds C++11 for ranges where the loop variable is copied in each iteration but
it would suffice to obtain it by const reference.

The check is only applied to loop variables of types that are expensive to copy
which means they are not trivially copyable or have a non-trivial copy
constructor or destructor.

To ensure that it is safe to replace the copy with a const reference the
following heuristic is employed:

1. The loop variable is const qualified.
2. The loop variable is not const, but only const methods or operators are
   invoked on it, or it is used as const reference or value argument in
   constructors or function calls.

Options
-------

.. option:: WarnOnAllAutoCopies

   When `true`, warns on any use of ``auto`` as the type of the range-based for
   loop variable. Default is `false`.

.. option:: AllowConstOverloads

   When `true`, a call to a non-const member function is treated as
   non-mutating if the same class also declares a const-qualified overload
   with the same name, same parameter types, and same value return type
   (for example ``int get()`` paired with ``int get() const``). This allows
   the check to suggest a ``const`` reference in cases where the loop
   variable is only used to call such overload pairs. Default is `true`.

   Pair detection is intentionally narrow to avoid false positives:

   - Only methods declared **directly on the same class** are considered;
     overloads brought in via base classes are only matched when made visible
     by a ``using`` declaration.
   - The two overloads must have **identical parameter types** (deleted
     overloads are ignored). Other method qualifiers, such as ``volatile``,
     must match between the two overloads.
   - **Only value return types are considered.** Reference- and
     pointer-returning pairs such as ``T&`` / ``const T&``, ``T*`` /
     ``const T*``, or ``T&&`` / ``const T&&`` are *not* treated as
     paired, because the non-const overload can expose a mutable reference
     or pointer that the caller can use to mutate the object — e.g.
     ``c.get() = x;``, ``opt->setX(...)`` via ``std::optional::operator->``,
     or ``*p = x;`` via ``operator*``. The analyzer does not track
     mutations through such escape paths, so the non-const call is kept
     as a conservative mutation indicator.

.. option:: AllowedTypes

   A semicolon-separated list of names of types allowed to be copied in each
   iteration. Regular expressions are accepted, e.g. ``[Rr]ef(erence)?$``
   matches every type with suffix ``Ref``, ``ref``, ``Reference`` and
   ``reference``. The default is empty. If a name in the list contains the
   sequence `::`, it is matched against the qualified type name
   (i.e. ``namespace::Type``), otherwise it is matched against only the
   type name (i.e. ``Type``).
