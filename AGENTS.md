# Repository Guidelines

## Project Structure & Module Organization

`compiler/` holds the future Bit compiler. Keep public headers in `compiler/include/`, stage-specific implementation in `compiler/src/` (`lex/`, `parse/`, `ast/`, `sema/`, `irgen/`, `driver/`, `support/`, `main/`), and compiler tests in `compiler/tests/`.  
`docs/` contains design references such as `syntax.md`, `type-system.md`, and `ir-strategy.md`; update these alongside behavior changes.  
`runtime/`, `std/`, and `examples/` are reserved for the runtime, standard library, and sample programs. Put helper scripts in `scripts/` and vendored code in `third_party/`.

## Build, Test, and Development Commands

This repository does not yet ship a checked-in build system. Until `CMakeLists.txt` and test runners are added, keep development commands simple and reproducible.

- `clang file.c $(llvm-config --cflags --ldflags --libs core) -o out`
  Build a local LLVM C API smoke test or prototype.
- `./out`
  Run the compiled test binary locally.
- `git status --short`
  Review working tree changes before submitting.

When adding a new command flow, document it here and keep it runnable from the repository root.

## Coding Style & Naming Conventions

Use 4-space indentation and keep source ASCII unless a file already requires Unicode. Prefer small, stage-local modules over large shared files.  
Use lowercase directory and file names. For C/C++ code, use `snake_case` for functions, variables, and helpers; reserve `PascalCase` only for type-like constructs if the codebase adopts them consistently. Match existing document naming such as `type-system.md` and `memory-model.md`.

## Testing Guidelines

Place tests under `compiler/tests/`, grouped by compiler stage or feature. Name tests so their purpose is obvious, for example `lex_numbers.*` or `parse_if_expr.*`. Favor narrow tests that isolate one language rule or IR behavior. If a change affects syntax, typing, or IR generation, add or update at least one corresponding test.

## Commit & Pull Request Guidelines

The repository currently has no established Git history, so use a simple convention: imperative, scoped commit subjects such as `lex: add integer token scan` or `docs: clarify memory model`.  
Pull requests should include a short problem statement, the approach taken, and exact verification commands run. Link related issues when available, and include sample input/output when changing diagnostics, parsing, or generated IR.
