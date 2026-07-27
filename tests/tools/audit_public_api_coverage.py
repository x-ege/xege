#!/usr/bin/env python3
"""Static audit of EGE's exported free-function coverage.

This intentionally reports evidence instead of declaring behavioral coverage:
branches, hardware, and platform paths still need focused runtime tests.  The
audit checks direct name references, call arities, compiler-selected overload
signatures, and keeps the two public headers in sync.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Optional


ROOT = Path(__file__).resolve().parents[2]
PUBLIC_HEADERS = (ROOT / "include" / "ege.h", ROOT / "include" / "ege.zh_CN.h")
OVERLOAD_TEST = ROOT / "tests" / "tests" / "public_api_overload_test.cpp"
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}
MANUAL_ONLY: dict[str, str] = {}
Signature = tuple[str, str, tuple[str, ...]]


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


def without_default(argument: str) -> str:
    depth = 0
    for index, current in enumerate(argument):
        if current in "([{":
            depth += 1
        elif current in ")]}" and depth > 0:
            depth -= 1
        elif current == "=" and depth == 0:
            return argument[:index].strip()
    return argument.strip()


def normalize_type(source: str) -> str:
    normalized = " ".join(source.split())
    normalized = re.sub(r"\s*([*&])\s*", r"\1", normalized)
    return normalized


def parameter_type(argument: str) -> str:
    argument = without_default(argument)
    if argument == "...":
        return argument
    array_match = re.search(r"\b[A-Za-z_]\w*(\s*\[[^\]]*\])\s*$", argument)
    if array_match:
        return normalize_type(argument[:array_match.start()] + array_match.group(1))
    name_match = re.search(r"\b[A-Za-z_]\w*\s*$", argument)
    if name_match is None:
        raise ValueError(f"cannot identify parameter name in {argument!r}")
    return normalize_type(argument[:name_match.start()])


def exported_signatures(header: Path) -> set[Signature]:
    source = code_only(header.read_text(encoding="utf-8"))
    signatures: set[Signature] = set()
    pattern = re.compile(
        r"(?m)^[ \t]*(?P<return>[A-Za-z_][\w:<>\s*&]*?)"
        r"\s+EGEAPI\s+(?P<name>[A-Za-z_]\w*)\s*\("
    )
    for match in pattern.finditer(source):
        closing = matching_paren(source, match.end() - 1)
        if closing is None:
            continue
        return_type = re.sub(r"^inline\s+", "", match.group("return").strip())
        arguments = split_arguments(source[match.end():closing])
        signatures.add((
            match.group("name"),
            normalize_type(return_type),
            tuple(parameter_type(argument) for argument in arguments),
        ))
    return signatures


def overload_signature_evidence(path: Path) -> set[Signature]:
    source = code_only(path.read_text(encoding="utf-8"))
    signatures: set[Signature] = set()
    for match in re.finditer(r"\bEGE_EXPECT_OVERLOAD\s*\(", source):
        closing = matching_paren(source, match.end() - 1)
        if closing is None:
            continue
        arguments = split_arguments(source[match.end():closing])
        if len(arguments) != 3:
            continue
        return_type, name, parameters = arguments
        parameters = parameters.strip()
        if not (parameters.startswith("(") and parameters.endswith(")")):
            continue
        signatures.add((
            name.strip(),
            normalize_type(return_type),
            tuple(normalize_type(parameter)
                  for parameter in split_arguments(parameters[1:-1])),
        ))
    return signatures


def signature_record(signature: Signature) -> dict[str, object]:
    name, return_type, parameters = signature
    return {
        "name": name,
        "return_type": return_type,
        "parameter_types": list(parameters),
    }


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
    header_signatures = {
        path.name: exported_signatures(path) for path in PUBLIC_HEADERS
    }
    canonical = header_sets[PUBLIC_HEADERS[0].name]
    canonical_declarations = header_declarations[PUBLIC_HEADERS[0].name]
    canonical_signatures = header_signatures[PUBLIC_HEADERS[0].name]
    overload_evidence = overload_signature_evidence(OVERLOAD_TEST)
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
        "signatures": {
            name: len(signatures)
            for name, signatures in header_signatures.items()
        },
        "signatures_match": (
            header_signatures[PUBLIC_HEADERS[0].name]
            == header_signatures[PUBLIC_HEADERS[1].name]
        ),
        "signature_header_only": {
            PUBLIC_HEADERS[0].name: [
                signature_record(signature)
                for signature in sorted(
                    header_signatures[PUBLIC_HEADERS[0].name]
                    - header_signatures[PUBLIC_HEADERS[1].name]
                )
            ],
            PUBLIC_HEADERS[1].name: [
                signature_record(signature)
                for signature in sorted(
                    header_signatures[PUBLIC_HEADERS[1].name]
                    - header_signatures[PUBLIC_HEADERS[0].name]
                )
            ],
        },
        "exact_declaration_covered": len(canonical_signatures & overload_evidence),
        "uncovered_exact_declarations": [
            signature_record(signature)
            for signature in sorted(canonical_signatures - overload_evidence)
        ],
        "stale_exact_declaration_evidence": [
            signature_record(signature)
            for signature in sorted(overload_evidence - canonical_signatures)
        ],
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
    signatures = result["signatures"]
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
    print(f"exact signatures: {signatures} (match={result['signatures_match']})")
    print(f"declaration arity evidence: {declaration_covered}/{declaration_total} "
          f"({declaration_covered / declaration_total:.1%})")
    exact_covered = result["exact_declaration_covered"]
    exact_total = signatures[PUBLIC_HEADERS[0].name]
    print(f"exact declaration evidence: {exact_covered}/{exact_total} "
          f"({exact_covered / exact_total:.1%})")
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
        for label in ("uncovered_exact_declarations",
                      "stale_exact_declaration_evidence"):
            values = result[label]
            if values:
                print(f"\n{label}:")
                for signature in values:
                    parameters = ", ".join(signature["parameter_types"])
                    print(f"  {signature['return_type']} {signature['name']}({parameters})")
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
                 result["signatures_match"] and
                 not result["uncovered_declaration_arities"] and
                 not result["uncovered_exact_declarations"] and
                 not result["stale_exact_declaration_evidence"] and
                 not result["unclassified_without_direct_test"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
