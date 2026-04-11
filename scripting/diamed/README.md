# Diamed Module (MVP Scaffold)

This folder contains the first scaffold for a new C-like live coding language for IanniX.
The design target is stage-safe live coding aligned with IanniX's deterministic visual score workflow.

## Files

- `DIAMED_SPEC_v0.1.md` — language and runtime design
- `diamed_compiler.h` — public compile API
- `diamed_compiler.cpp` — MVP compile pass with diagnostics

## Current status

- Wired in loader path for `.diamed` / `.dmd` files with stage diagnostics
- Successful compile now executes extracted `legacy("...")` commands via the existing IanniX command pipeline
- No AST/bytecode yet
- Provides useful diagnostics for common syntax faults:
  - missing semicolon
  - unbalanced braces/parentheses
  - malformed `legacy(...)`
- Live stage profile additionally checks:
  - realtime-unsafe constructs (`while(true)`, `for(;;)`, blocking calls)
  - non-deterministic randomness (`rand/random`) as warnings
  - patch size limits for performance safety

## Runtime policy

- `Diamed::compile(source, options)` compiles with configurable mode.
- `Diamed::compileForLiveStage(source)` enables strict stage profile.
- `CompileResult` exposes `executableCommands` extracted from `legacy(...)` statements.
- `NxDocument` uses live-stage compile for `.diamed` and `.dmd` files, forwards diagnostics to the editor status panel, and executes extracted commands through `Application::execute`.

## Integration roadmap

1. Add explicit `Language Mode` switch in `UiEditor` (Legacy / Diamed).
2. Expand executable mapping beyond `legacy(...)` into native Diamed IR instructions.
3. Add autocomplete based on spec keywords + builtins.
4. Incrementally replace heuristic parser with full lexer+AST.
