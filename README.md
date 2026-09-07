# Clang Static Analyzer Code Examples

This repository contains code examples shows during the presentation "Static Analysis
with Clang".

##

A Clang Static Analyzer plugin implementing a checker for **MISRA C:2023 Rule 22.4**
(*There shall be no attempt to write to a stream which has been opened as read-only*).

The checker (`misra.Rule22_04`) tracks the mode string passed to `fopen` and reports
any `fprintf` on a stream opened read-only (`"r"` without `+`).

## Build

Requirements:

- CMake >= 4.0 and a build tool (Ninja or Make)
- A C++17 compiler
- Clang/LLVM **development** files, including the static analyzer headers and the
  CMake package config (`/usr/lib/cmake/clang`). On Arch this is the `clang` package;
  on Debian/Ubuntu `libclang-dev` + `llvm-dev`.

> The plugin must be built against the **same version of Clang** that will load
it; the analyzer plugin ABI is not stable across releases.

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

This produces `build/libMISRA.so`.

## Run

```sh
clang --analyze \
    -Xanalyzer -load -Xanalyzer build/libMISRA.so \
    -Xanalyzer -analyzer-checker=misra.Rule22_04 \
    -Xanalyzer -analyzer-output=text \
    Rule22_04Test.cpp
```

Expected output:

```
Rule22_04Test.cpp:15:3: warning: Trying to write to a read-only stream [misra.Rule22_04]
   15 |   fprintf(LogFile, "event=import\n");
      |   ^
Rule22_04Test.cpp:20:3: note: Calling 'runImport'
   20 |   runImport("audit.log", /*Resume=*/true);
      |   ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Rule22_04Test.cpp:11:19: note: Calling 'openLog'
   11 |   FILE *LogFile = openLog(Path, Resume);
      |                   ^~~~~~~~~~~~~~~~~~~~~
Rule22_04Test.cpp:4:22: note: 'Resume' is true
    4 |   const char *Mode = Resume ? "r" : "w";
      |                      ^~~~~~
Rule22_04Test.cpp:4:22: note: '?' condition is true
Rule22_04Test.cpp:5:10: note: File stream opened read-only
    5 |   return fopen(Path, Mode);
      |          ^~~~~~~~~~~~~~~~~
Rule22_04Test.cpp:5:10: note: Assuming that 'fopen' is successful
    5 |   return fopen(Path, Mode);
      |          ^~~~~~~~~~~~~~~~~
Rule22_04Test.cpp:11:19: note: Returning from 'openLog'
   11 |   FILE *LogFile = openLog(Path, Resume);
      |                   ^~~~~~~~~~~~~~~~~~~~~
Rule22_04Test.cpp:12:8: note: 'LogFile' is non-null
   12 |   if (!LogFile)
      |        ^~~~~~~
Rule22_04Test.cpp:12:3: note: Taking false branch
   12 |   if (!LogFile)
      |   ^
Rule22_04Test.cpp:15:3: note: Trying to write to a read-only stream
   15 |   fprintf(LogFile, "event=import\n");
      |   ^~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
1 warning generated.
```
