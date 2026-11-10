```{title} clang-tidy - misc-no-recursion
```

# misc-no-recursion

Finds strongly connected functions (by analyzing the call graph for
SCC's (Strongly Connected Components) that are loops),
diagnoses each function in the cycle,
and displays one example of a possible call graph loop (recursion).

References:

- CERT C++ Coding Standard rule [DCL56-CPP. Avoid cycles during initialization of static objects](https://cmu-sei.github.io/secure-coding-standards/sei-cert-cpp-coding-standard/rules/declarations-and-initialization-dcl/dcl56-cpp/).
- JPL Institutional Coding Standard for the C Programming Language
  (JPL DOCID D-60411) rule `2.4 Do not use direct or indirect recursion`.
- OpenCL Specification, Version 1.2 rule [6.9 Restrictions: i. Recursion is not supported.](https://www.khronos.org/registry/OpenCL/specs/opencl-1.2.pdf).

## Recursion through several translation units

The call graph that the check analyzes is the one of the translation unit, so
it does not show a cycle that goes through functions defined in other
translation units. The check can find those cycles when it is given the result
of the recursion analysis of the Scalable Static Analysis Framework (SSAF),
which joins the call graphs of all the translation units of a program.

First, extract the call graph of every translation unit, with
`--ssaf-compilation-unit-id=` naming the translation unit. By default,
clang-tidy names it by the path of its main file, so use that:

```console
$ clang -fsyntax-only --ssaf-extract-summaries=CallGraph \
    --ssaf-tu-summary-file=a.json --ssaf-compilation-unit-id=/src/a.cpp a.cpp
$ clang -fsyntax-only --ssaf-extract-summaries=CallGraph \
    --ssaf-tu-summary-file=b.json --ssaf-compilation-unit-id=/src/b.cpp b.cpp
```

Then link the summaries into one link unit, which is named by the output file
(here `prog`), and analyze it:

```console
$ clang-ssaf-linker a.json b.json -o prog.json
$ clang-ssaf-analyzer -a RecursionAnalysisResult prog.json -o wpa.json
```

Finally, give the result to clang-tidy with the options of clang that name it.
They go after `--`, or in `--extra-arg`, which also serves `run-clang-tidy`:

```console
$ clang-tidy -checks=-*,misc-no-recursion a.cpp -- \
    --ssaf-global-scope-analysis-result=wpa.json --ssaf-link-unit-id=prog
```

The check then also reports the functions of the translation unit that are on
a cycle through other translation units. Their call sites in the other
translation units are not known, so the example call chain names the functions,
and where they are defined. The cycles within one translation unit are reported
as before. Without the result, nothing changes.

If a translation unit was extracted under another name than the path of its
main file, `--ssaf-compilation-unit-id=` gives that name to clang-tidy.

## Limitations

- The check does not handle calls done through function pointers
- The check does not handle C++ destructors

