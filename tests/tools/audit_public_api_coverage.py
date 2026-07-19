#!/usr/bin/env python3
"""Name-level audit of EGE's exported free-function coverage.

This intentionally reports evidence instead of declaring behavioral coverage:
an identifier used by a test may still leave overloads, branches, or platform
paths untested.  The audit is useful for finding exported names that have no
direct test or demo caller and for keeping the two public headers in sync.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
PUBLIC_HEADERS = (ROOT / "include" / "ege.h", ROOT / "include" / "ege.zh_CN.h")
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
MANUAL_ONLY = {
    "attachHWND": "requires a real host HWND and embedding lifecycle",
    "inputbox_getline": "opens a modal dialog and requires user input",
    "clear_console": "mutates the calling process console",
    "close_console": "mutates the calling process console",
    "getch_console": "blocks on interactive console input",
    "hide_console": "mutates the calling process console window",
    "init_console": "allocates or attaches a process console",
    "kbhit_console": "depends on interactive console input",
    "show_console": "mutates the calling process console window",
}


def code_only(text: str) -> str:
    """Replace comments and string/character literals while preserving lines."""

    result: list[str] = []
    index = 0
    state = "code"
    quote = ""
    while index < len(text):
        current = text[index]
        following = text[index + 1] if index + 1 < len(text) else ""

        if state == "code":
            if current == "/" and following == "/":
                result.extend("  ")
                index += 2
                state = "line_comment"
                continue
            if current == "/" and following == "*":
                result.extend("  ")
                index += 2
                state = "block_comment"
                continue
            if current in {'"', "'"}:
                quote = current
                result.append(" ")
                index += 1
                state = "literal"
                continue
            result.append(current)
            index += 1
            continue

        if state == "line_comment":
            result.append("\n" if current == "\n" else " ")
            index += 1
            if current == "\n":
                state = "code"
            continue

        if state == "block_comment":
            if current == "*" and following == "/":
                result.extend("  ")
                index += 2
                state = "code"
            else:
                result.append("\n" if current == "\n" else " ")
                index += 1
            continue

        if current == "\\" and following:
            result.append(" ")
            result.append("\n" if following == "\n" else " ")
            index += 2
        elif current == quote:
            result.append(" ")
            index += 1
            state = "code"
        else:
            result.append("\n" if current == "\n" else " ")
            index += 1

    return "".join(result)


def exported_names(header: Path) -> set[str]:
    source = code_only(header.read_text(encoding="utf-8"))
    return set(re.findall(r"\bEGEAPI\s+([A-Za-z_]\w*)\s*\(", source))


def source_text(directory: Path) -> str:
    parts: list[str] = []
    for path in sorted(directory.rglob("*")):
        if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES:
            parts.append(code_only(path.read_text(encoding="utf-8", errors="replace")))
    return "\n".join(parts)


def referenced_names(names: set[str], source: str) -> set[str]:
    return {name for name in names if re.search(rf"\b{re.escape(name)}\s*\(", source)}


def audit() -> dict[str, object]:
    header_sets = {path.name: exported_names(path) for path in PUBLIC_HEADERS}
    canonical = header_sets[PUBLIC_HEADERS[0].name]
    tests = referenced_names(canonical, source_text(ROOT / "tests"))
    demos = referenced_names(canonical, source_text(ROOT / "demo"))
    without_direct_test = canonical - tests
    return {
        "headers": {name: len(names) for name, names in header_sets.items()},
        "headers_match": header_sets[PUBLIC_HEADERS[0].name] == header_sets[PUBLIC_HEADERS[1].name],
        "header_only": {
            PUBLIC_HEADERS[0].name: sorted(header_sets[PUBLIC_HEADERS[0].name] - header_sets[PUBLIC_HEADERS[1].name]),
            PUBLIC_HEADERS[1].name: sorted(header_sets[PUBLIC_HEADERS[1].name] - header_sets[PUBLIC_HEADERS[0].name]),
        },
        "direct_test": sorted(tests),
        "demo_only": sorted(demos - tests),
        "unreferenced": sorted(canonical - tests - demos),
        "manual_only": {
            name: MANUAL_ONLY[name]
            for name in sorted(without_direct_test & MANUAL_ONLY.keys())
        },
        "unclassified_without_direct_test": sorted(
            without_direct_test - MANUAL_ONLY.keys()
        ),
    }


def print_text(result: dict[str, object], include_names: bool) -> None:
    headers = result["headers"]
    direct = result["direct_test"]
    demo_only = result["demo_only"]
    unreferenced = result["unreferenced"]
    manual_only = result["manual_only"]
    unclassified = result["unclassified_without_direct_test"]
    total = headers[PUBLIC_HEADERS[0].name]
    print(f"public headers: {headers} (match={result['headers_match']})")
    print(f"direct test references: {len(direct)}/{total} ({len(direct) / total:.1%})")
    print(f"demo-only references: {len(demo_only)}/{total} ({len(demo_only) / total:.1%})")
    print(f"no test/demo reference: {len(unreferenced)}/{total} ({len(unreferenced) / total:.1%})")
    print(f"intentional manual-only interfaces: {len(manual_only)}/{total} "
          f"({len(manual_only) / total:.1%})")
    print(f"unclassified without direct test: {len(unclassified)}")
    if include_names:
        for label, names in (("direct_test", direct), ("demo_only", demo_only),
                             ("unreferenced", unreferenced)):
            print(f"\n{label} ({len(names)}):")
            print("  " + ", ".join(names) if names else "  (none)")
        if not result["headers_match"]:
            print("\nheader-only names:")
            for header, names in result["header_only"].items():
                print(f"  {header}: {', '.join(names) if names else '(none)'}")
        print("\nmanual_only:")
        for name, reason in manual_only.items():
            print(f"  {name}: {reason}")
        if unclassified:
            print("\nunclassified_without_direct_test:")
            print("  " + ", ".join(unclassified))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    parser.add_argument("--summary", action="store_true", help="omit identifier lists")
    args = parser.parse_args()

    result = audit()
    if args.json:
        print(json.dumps(result, ensure_ascii=False, indent=2))
    else:
        print_text(result, not args.summary)
    return 0 if (result["headers_match"] and
                 not result["unclassified_without_direct_test"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
