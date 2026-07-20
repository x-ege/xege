#!/usr/bin/env python3
"""Static lower-bound audit of EGE's exported free-function coverage.

This intentionally reports evidence instead of declaring behavioral coverage:
an identifier used by a test may still leave type-distinct overloads with the
same arity, branches, or platform paths untested.  The audit checks both name
references and whether every unique declaration accepts at least one argument
count used by a test, and keeps the two public headers in sync.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Optional


ROOT = Path(__file__).resolve().parents[2]
PUBLIC_HEADERS = (ROOT / "include" / "ege.h", ROOT / "include" / "ege.zh_CN.h")
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
MANUAL_ONLY: dict[str, str] = {}


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


def matching_paren(source: str, opening: int) -> Optional[int]:
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "(":
            depth += 1
        elif source[index] == ")":
            depth -= 1
            if depth == 0:
                return index
    return None


def split_arguments(source: str) -> list[str]:
    if not source.strip() or source.strip() == "void":
        return []

    arguments: list[str] = []
    start = 0
    depth = 0
    for index, current in enumerate(source):
        if current in "([{":
            depth += 1
        elif current in ")]}" and depth > 0:
            depth -= 1
        elif current == "," and depth == 0:
            arguments.append(source[start:index].strip())
            start = index + 1
    arguments.append(source[start:].strip())
    return arguments


def exported_declarations(header: Path) -> set[tuple[str, str, int, Optional[int]]]:
    source = code_only(header.read_text(encoding="utf-8"))
    declarations: set[tuple[str, str, int, Optional[int]]] = set()
    for match in re.finditer(r"\bEGEAPI\s+([A-Za-z_]\w*)\s*\(", source):
        closing = matching_paren(source, match.end() - 1)
        if closing is None:
            continue
        parameters = split_arguments(source[match.end():closing])
        normalized = ", ".join(" ".join(parameter.split()) for parameter in parameters)
        variadic = bool(parameters and parameters[-1] == "...")
        fixed_parameters = parameters[:-1] if variadic else parameters
        required = sum("=" not in parameter for parameter in fixed_parameters)
        maximum = None if variadic else len(fixed_parameters)
        declarations.add((match.group(1), normalized, required, maximum))
    return declarations


def source_text(directory: Path) -> str:
    parts: list[str] = []
    for path in sorted(directory.rglob("*")):
        if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES:
            parts.append(code_only(path.read_text(encoding="utf-8", errors="replace")))
    return "\n".join(parts)


def referenced_names(names: set[str], source: str) -> set[str]:
    return {name for name in names if re.search(rf"\b{re.escape(name)}\s*\(", source)}


def referenced_arities(names: set[str], source: str) -> dict[str, set[int]]:
    arities: dict[str, set[int]] = {name: set() for name in names}
    for name in names:
        for match in re.finditer(rf"\b{re.escape(name)}\s*\(", source):
            closing = matching_paren(source, match.end() - 1)
            if closing is not None:
                arities[name].add(len(split_arguments(source[match.end():closing])))
    return arities


def declaration_record(
    declaration: tuple[str, str, int, Optional[int]], calls: set[int]
) -> dict[str, object]:
    name, parameters, required, maximum = declaration
    return {
        "name": name,
        "parameters": parameters,
        "required_arguments": required,
        "maximum_arguments": maximum,
        "observed_test_arities": sorted(calls),
    }


def audit() -> dict[str, object]:
    header_sets = {path.name: exported_names(path) for path in PUBLIC_HEADERS}
    header_declarations = {
        path.name: exported_declarations(path) for path in PUBLIC_HEADERS
    }
    canonical = header_sets[PUBLIC_HEADERS[0].name]
    canonical_declarations = header_declarations[PUBLIC_HEADERS[0].name]
    test_source = source_text(ROOT / "tests")
    tests = referenced_names(canonical, test_source)
    demos = referenced_names(canonical, source_text(ROOT / "demo"))
    test_arities = referenced_arities(canonical, test_source)
    uncovered_declarations = []
    for declaration in sorted(canonical_declarations):
        name, _parameters, required, maximum = declaration
        calls = test_arities[name]
        if not any(
            arity >= required and (maximum is None or arity <= maximum)
            for arity in calls
        ):
            uncovered_declarations.append(declaration_record(declaration, calls))
    without_direct_test = canonical - tests
    return {
        "headers": {name: len(names) for name, names in header_sets.items()},
        "headers_match": header_sets[PUBLIC_HEADERS[0].name] == header_sets[PUBLIC_HEADERS[1].name],
        "declarations": {
            name: len(declarations)
            for name, declarations in header_declarations.items()
        },
        "declarations_match": (
            header_declarations[PUBLIC_HEADERS[0].name]
            == header_declarations[PUBLIC_HEADERS[1].name]
        ),
        "declaration_header_only": {
            PUBLIC_HEADERS[0].name: [
                declaration_record(declaration, set())
                for declaration in sorted(
                    header_declarations[PUBLIC_HEADERS[0].name]
                    - header_declarations[PUBLIC_HEADERS[1].name]
                )
            ],
            PUBLIC_HEADERS[1].name: [
                declaration_record(declaration, set())
                for declaration in sorted(
                    header_declarations[PUBLIC_HEADERS[1].name]
                    - header_declarations[PUBLIC_HEADERS[0].name]
                )
            ],
        },
        "declaration_arity_covered": len(canonical_declarations) - len(uncovered_declarations),
        "uncovered_declaration_arities": uncovered_declarations,
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
    declarations = result["declarations"]
    direct = result["direct_test"]
    demo_only = result["demo_only"]
    unreferenced = result["unreferenced"]
    manual_only = result["manual_only"]
    unclassified = result["unclassified_without_direct_test"]
    total = headers[PUBLIC_HEADERS[0].name]
    print(f"public headers: {headers} (match={result['headers_match']})")
    declaration_total = declarations[PUBLIC_HEADERS[0].name]
    declaration_covered = result["declaration_arity_covered"]
    print(f"unique declarations: {declarations} (match={result['declarations_match']})")
    print(f"declaration arity evidence: {declaration_covered}/{declaration_total} "
          f"({declaration_covered / declaration_total:.1%})")
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
        uncovered_declarations = result["uncovered_declaration_arities"]
        if uncovered_declarations:
            print("\nuncovered_declaration_arities:")
            for declaration in uncovered_declarations:
                maximum = declaration["maximum_arguments"]
                maximum_text = "variadic" if maximum is None else str(maximum)
                print(f"  {declaration['name']}({declaration['parameters']}): "
                      f"required={declaration['required_arguments']}, max={maximum_text}, "
                      f"test calls={declaration['observed_test_arities']}")


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
                 result["declarations_match"] and
                 not result["uncovered_declaration_arities"] and
                 not result["unclassified_without_direct_test"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
