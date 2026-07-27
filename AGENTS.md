# Xege Agent Guide

Xege is a C++/CMake graphics library whose existing Windows API and behavior
must remain compatible while macOS and Linux support is developed.

## Sources of Truth

- `BUILD.md`: platform dependencies, CMake options, and command-line builds.
- `tests/README.md`: test inventory, TDD rules, CTest commands, sanitizers, and
  headless Linux execution.
- `.vscode/tasks.json`: canonical local tasks for configuring, building, and
  running demos.
- `demo/README.md`: demo naming and organization.

Do not copy detailed commands from these files into agent instructions. Update
the owning file when a workflow changes.

## Compatibility Contract

- Windows defaults to GDI. OpenGL is a build-time opt-in, and an application
  selects it with `INIT_OPENGL`.
- macOS and Linux default to native OpenGL.
- Bundled GLFW is the default on macOS and Linux. Linux defaults to X11;
  Wayland is explicit opt-in.
- Unix native executables have no `.exe` suffix. `tasks.sh` maps the historical
  `.exe` names used by VS Code tasks to native demo binaries.
- Preserve public Win32-compatible types, constants, overloads, and default
  behavior. A backend-specific fix must not silently change another backend.

## Codebase Invariants

- Public declarations live in `include/`; implementations live in `src/` and
  normally use the `ege` namespace while the legacy public API stays global.
- Keep `include/ege.h` and the Chinese-annotated `include/ege.zh_CN.h`
  API-compatible and update them together.

## Build and Run

- When a VS Code task runner is available, invoke the appropriate task from
  `.vscode/tasks.json` instead of reconstructing its command.
- Otherwise run `bash -l tasks.sh --help` and use the matching `tasks.sh`
  action. On Windows, run this script with Git Bash or another POSIX shell.
- The VS Code task catalog currently covers configuration, builds, and demos;
  it does not run the CTest suite. Use `tests/README.md` for test commands.
- Never reuse a build directory after changing generator, toolchain, backend,
  or architecture.

## Validation

Use TDD for behavior changes: add the smallest deterministic regression first,
confirm that it fails for the intended reason, fix the real API/backend path,
then run the focused test followed by the functional suite and demo build.

- Rendering or image I/O: assert pixels and image metadata. File-format tests
  must save to a temporary file and load it again.
- Window, input, or event loop: run the relevant backend test and process-exit
  test. Automated tests must not wait for manual input.
- Camera: keep frame-copy coverage hardware-independent; run provider lifecycle
  and fixture capture coverage where supported.
- Public API: run `public_headers` and verify both public headers.
- Platform defaults: configure from an empty build directory without passing
  the option under test; explicit CI flags must not hide a wrong default.
- Shared rendering, lifecycle, or API changes: cover Windows GDI, Windows
  OpenGL, and affected macOS/Linux native OpenGL paths.
- CMake or workflow changes: identify every affected platform/backend CI job.

Performance tests are labeled `performance` and are separate from the normal
functional gate. Follow `tests/README.md` for the exact focused, functional,
performance, sanitizer, and headless commands.
