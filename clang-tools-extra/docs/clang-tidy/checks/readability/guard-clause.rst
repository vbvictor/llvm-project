.. title:: clang-tidy - readability-guard-clause

readability-guard-clause
========================

Detects opportunities to introduce guard clauses to reduce nesting and
improve code readability. The check provides automatic fix-its to apply the
transformation.

A guard clause is a conditional statement at the beginning of a function or
loop that uses an early exit (``return`` or ``continue``) to handle special
cases, allowing the main logic to remain at a reduced indentation level.

The check identifies two patterns:

1. **Functions with deeply nested if statements**: When an ``if`` statement
   wraps most of a function's logic, it suggests using an inverted condition
   with an early ``return`` instead.

2. **Loops with deeply nested if statements**: When an ``if`` statement wraps
   most of a loop's logic, it suggests using an inverted condition with
   ``continue`` instead.

Examples
--------

Function with nested logic
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The check will flag this code:

.. code-block:: c++

    void processData(Data* data) {
      if (data != nullptr) {
        // Many lines of processing logic
        data->validate();
        data->transform();
        data->save();
      }
    }

And suggest refactoring it to use a guard clause:

.. code-block:: c++

    void processData(Data* data) {
      if (data == nullptr)
        return;

      // Many lines of processing logic
      data->validate();
      data->transform();
      data->save();
    }

Loop with nested logic
^^^^^^^^^^^^^^^^^^^^^^

The check will flag this code:

.. code-block:: c++

    void processItems(const std::vector<Item>& items) {
      for (const auto& item : items) {
        if (item.isValid()) {
          // Many lines of processing logic
          item.process();
          item.validate();
          item.save();
        }
      }
    }

And suggest refactoring it to use a guard clause:

.. code-block:: c++

    void processItems(const std::vector<Item>& items) {
      for (const auto& item : items) {
        if (!item.isValid())
          continue;

        // Many lines of processing logic
        item.process();
        item.validate();
        item.save();
      }
    }

When the check does not trigger
--------------------------------

The check will not trigger in the following cases:

* The ``if`` statement has a non-empty ``else`` clause
* The ``if`` statement's then branch has fewer lines than the threshold
  (see :option:`MinimumLines`), unless it's the only statement in the scope
* The ``if`` statement is ``constexpr`` or ``consteval`` (these have
  different semantics and should not be refactored)
* Multiple ``if`` statements exist in sequence (no single dominant branch)

Options
-------

.. option:: MinimumLines

   Minimum number of lines required in the ``if`` statement's then branch
   for the check to trigger. The default value is ``5``. This ensures the
   check only flags cases where introducing a guard clause would meaningfully
   reduce nesting.

   Note that if the ``if`` statement is the only statement in the scope,
   it always triggers regardless of this setting.

   For example, with ``MinimumLines`` set to ``10``:

   .. code-block:: c++

      void example() {
        if (condition) {
          // This will trigger because it has 10+ lines
          statement1();
          statement2();
          statement3();
          statement4();
          statement5();
          statement6();
          statement7();
          statement8();
          statement9();
          statement10();
        }
      }

      void another_example() {
        if (condition) {
          // This won't trigger (only 3 lines)
          statement1();
          statement2();
        }
      }
