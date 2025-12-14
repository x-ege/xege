# EGE (Easy Graphics Engine) Development Guide

## Core Architecture

- **Namespace:** All implementations in `ege` namespace, public API in global namespace
- **Build:** `tasks.sh` is the main build script supporting Windows, Linux, and macOS. Use `bash -l tasks.sh <args...>`

## Core Rules

- **Demo Naming:** Read `demo/README.md` before adding demos
- `include/` contains only public API declarations; implementations in `src/`
- `include/ege.h` is the main header. `ege.zh_CN.h` is its Chinese-annotated version; keep them in sync

## Build Command Guidelines

On Windows, use Git Bash (or POSIX shell) for shell scripts. Avoid PowerShell or CMD.
