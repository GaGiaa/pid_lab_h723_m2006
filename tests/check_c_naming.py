"""Check naming rules for the project's hand-written C code.

The checker intentionally scans only the project-owned files listed in
``TARGET_RELATIVE_PATHS``.  CubeMX-generated content in ``freertos.c`` is
excluded by retaining only its ``USER CODE`` regions.
"""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence


TARGET_RELATIVE_PATHS = (
    Path("App/Inc/vofa_justfloat.h"),
    Path("App/Src/vofa_justfloat.c"),
    Path("tests/vofa_justfloat_test.c"),
    Path("App/Inc/m2006_protocol.h"),
    Path("App/Src/m2006_protocol.c"),
    Path("App/Inc/m2006_driver.h"),
    Path("App/Src/m2006_driver.c"),
    Path("App/Inc/m2006_control_task.h"),
    Path("App/Src/m2006_control_task.c"),
    Path("App/Inc/vofa_timestamp_task.h"),
    Path("App/Src/vofa_timestamp_task.c"),
    Path("tests/m2006_protocol_test.c"),
    Path("Lib/pid_lib/include/pid.h"),
    Path("Lib/pid_lib/src/pid.c"),
    Path("tests/pid_test.c"),
    Path("Core/Src/freertos.c"),
)
FREERTOS_RELATIVE_PATH = Path("Core/Src/freertos.c")

MODULE_PREFIXES = (
    "vofa",
    "m2006",
    "pid",
)

LEGACY_IDENTIFIERS = {
    "__VOFA_JUSTFLOAT_H__": "VOFA_JUSTFLOAT_H",
    "VOFA_JUSTFLOAT_FRAME_SIZE": "VOFA_JUSTFLOAT_FRAME_SIZE_BYTES",
    "VofaJustFloatEncode1": "vofa_justfloat_encode_float",
    "vofaTimestampTaskHandle": "vofa_timestamp_task_handle",
    "vofaTimestampTask_attributes": "vofa_timestamp_task_attributes",
    "startVofaTimestampTask": "vofa_timestamp_task_entry",
    "vofaTimestamp": "vofa_timestamp",
}
LEGACY_IDENTIFIER_PATTERN = re.compile(
    r"\b(?:" + "|".join(map(re.escape, LEGACY_IDENTIFIERS)) + r")\b"
)

# These names are externally prescribed rather than project-owned.  Keep this
# table small and documented; a broad ignore pattern would hide new violations.
EXTERNAL_IDENTIFIER_EXCEPTIONS = {
    "MX_FREERTOS_Init": "CubeMX FreeRTOS initialization entry point",
    "defaultTask": "CubeMX default task runtime name",
    "startDefaultTask": "CubeMX default task entry point",
    "huart8": "CubeMX-generated UART8 handle",
    "HAL_FDCAN_RxFifo0Callback": "STM32 HAL FDCAN FIFO0 receive weak callback",
    "vApplicationStackOverflowHook": "FreeRTOS callback",
    "xTask": "FreeRTOS stack-overflow callback parameter",
    "pcTaskName": "FreeRTOS stack-overflow callback parameter",
    "main": "C program entry point",
}

LOWER_SNAKE_CASE_RE = re.compile(r"^[a-z][a-z0-9]*(?:_[a-z0-9]+)*$")
UPPER_SNAKE_CASE_RE = re.compile(r"^[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)*$")
FILE_NAME_RE = re.compile(r"^[a-z][a-z0-9]*(?:_[a-z0-9]+)*\.(?:c|h)$")

HEADER_GUARD_PATTERN = re.compile(
    r"(?m)^\s*#\s*ifndef\s+(?P<name>[A-Za-z_]\w*)\s*$"
)
DEFINE_PATTERN = re.compile(r"(?m)^\s*#\s*define\s+(?P<name>[A-Za-z_]\w*)\b")
TASK_NAME_PATTERN = re.compile(r'\.name\s*=\s*"(?P<name>(?:\\.|[^"\\])*)"')
FUNCTION_PATTERN = re.compile(
    r"""
    ^[ \t]*
    (?P<storage>(?:(?:static|extern|inline|__weak)\s+)*)
    (?P<return_type>
        (?:(?:const|volatile)\s+)*
        (?:
            (?:struct|union|enum)\s+[A-Za-z_]\w*
            |(?:unsigned|signed)(?:\s+(?:char|short|int|long(?:\s+long)?))?
            |long\s+long|long|short|char|int|float|double|void
            |[A-Za-z_]\w*
        )
        (?:\s*\*+\s*(?:const|volatile)?)*
    )
    \s+
    (?P<name>[A-Za-z_]\w*)
    \s*\(
    (?P<parameters>[^(){};]*)
    \)\s*(?:\{|;)
    """,
    re.MULTILINE | re.VERBOSE,
)
VARIABLE_DECLARATION_PATTERN = re.compile(
    r"""
    ^[ \t]*
    (?P<qualifiers>(?:(?:static|extern|const|volatile|register|__weak)\s+)*)
    (?P<type>
        (?:struct|union|enum)\s+[A-Za-z_]\w*
        |(?:unsigned|signed)(?:\s+(?:char|short|int|long(?:\s+long)?))?
        |long\s+long|long|short|char|int|float|double
        |[A-Za-z_]\w*
    )
    \s+
    (?P<declarators>
        (?:\*+\s*)?[A-Za-z_]\w*(?:\s*\[[^\]\n]*\])?
        (?:\s*=\s*[^,;\n]+)?
        (?:
            \s*,\s*
            (?:\*+\s*)?[A-Za-z_]\w*(?:\s*\[[^\]\n]*\])?
            (?:\s*=\s*[^,;\n]+)?
        )*
    )
    \s*;
    """,
    re.MULTILINE | re.VERBOSE,
)
DECLARATOR_NAME_PATTERN = re.compile(r"(?:\*+\s*)?(?P<name>[A-Za-z_]\w*)")
PARAMETER_NAME_PATTERN = re.compile(
    r"(?P<name>[A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*$"
)
TYPEDEF_NAME_PATTERN = re.compile(
    r"(?m)^\s*typedef\s+(?:[^;{}\n]+\s+)?(?P<name>[A-Za-z_]\w*)\s*;"
)
TAGGED_TYPE_PATTERN = re.compile(
    r"(?m)^\s*(?:struct|union|enum)\s+(?P<name>[A-Za-z_]\w*)\b"
)
USER_CODE_BEGIN_PATTERN = re.compile(r"/\*\s*USER CODE BEGIN\b.*?\*/")
USER_CODE_END_PATTERN = re.compile(r"/\*\s*USER CODE END\b.*?\*/")
NON_FUNCTION_LEADING_KEYWORDS = {
    "break",
    "case",
    "continue",
    "do",
    "else",
    "for",
    "goto",
    "if",
    "return",
    "sizeof",
    "switch",
    "while",
}


@dataclass(frozen=True)
class NamingViolation:
    """One naming-rule violation, including a project-relative source location."""

    relative_path: str
    line: int
    rule: str
    identifier: str

    def format(self) -> str:
        return f"{self.relative_path}:{self.line}:{self.rule}:{self.identifier}"


def scan_project(project_root: str | Path) -> list[NamingViolation]:
    """Scan the four project-owned C files below ``project_root``.

    Missing expected files are violations so a renamed or deleted target cannot
    silently remove itself from the checker scope.
    """

    root = Path(project_root)
    violations: list[NamingViolation] = []

    for relative_file_path in TARGET_RELATIVE_PATHS:
        source_path = root / relative_file_path
        relative_path = _relative_path_text(relative_file_path)

        if not source_path.is_file():
            violations.append(
                NamingViolation(
                    relative_path=relative_path,
                    line=1,
                    rule="target_file_missing",
                    identifier=relative_path,
                )
            )
            continue

        source_text = source_path.read_text(encoding="utf-8")
        violations.extend(scan_source(relative_file_path, source_text))

    return _sorted_violations(violations)


def scan_source(relative_path: str | Path, source_text: str) -> list[NamingViolation]:
    """Scan one target-like source string without reading from the filesystem.

    This helper makes the rules testable against isolated fixtures.  A source
    named ``Core/Src/freertos.c`` is automatically limited to ``USER CODE``
    regions just as it is during project scans.
    """

    normalized_path = _relative_path_text(relative_path)
    violations = _scan_file_name(normalized_path)
    source_scope = source_text

    if normalized_path == _relative_path_text(FREERTOS_RELATIVE_PATH):
        source_scope, found_user_code = _extract_user_code_regions(source_text)
        if not found_user_code:
            violations.append(
                NamingViolation(
                    relative_path=normalized_path,
                    line=1,
                    rule="user_code_region_missing",
                    identifier="USER CODE",
                )
            )

    comment_free_source = _mask_comments(source_scope)
    declaration_source = _mask_string_and_character_literals(comment_free_source)

    violations.extend(_scan_legacy_identifiers(normalized_path, declaration_source))

    if normalized_path.endswith(".h"):
        violations.extend(
            _scan_header_guard(normalized_path, declaration_source)
        )

    violations.extend(
        _scan_macros(normalized_path, declaration_source)
    )
    violations.extend(
        _scan_functions(normalized_path, declaration_source)
    )
    violations.extend(
        _scan_variable_declarations(normalized_path, declaration_source)
    )
    violations.extend(_scan_type_declarations(normalized_path, declaration_source))
    violations.extend(
        _scan_task_names(normalized_path, comment_free_source)
    )

    return _sorted_violations(violations)


def _scan_legacy_identifiers(
    relative_path: str,
    comment_free_source: str,
) -> list[NamingViolation]:
    violations: list[NamingViolation] = []

    for match in LEGACY_IDENTIFIER_PATTERN.finditer(comment_free_source):
        violations.append(
            NamingViolation(
                relative_path=relative_path,
                line=_line_number(comment_free_source, match.start()),
                rule="legacy_identifier",
                identifier=match.group(),
            )
        )

    return violations


def _scan_file_name(relative_path: str) -> list[NamingViolation]:
    file_name = Path(relative_path).name
    if FILE_NAME_RE.fullmatch(file_name):
        return []

    return [
        NamingViolation(
            relative_path=relative_path,
            line=1,
            rule="file_name_style",
            identifier=file_name,
        )
    ]


def _scan_header_guard(
    relative_path: str,
    declaration_source: str,
) -> list[NamingViolation]:
    match = HEADER_GUARD_PATTERN.search(declaration_source)
    if match is None:
        return [
            NamingViolation(
                relative_path=relative_path,
                line=1,
                rule="header_guard_missing",
                identifier=Path(relative_path).name,
            )
        ]

    guard_name = match.group("name")
    line = _line_number(declaration_source, match.start("name"))
    violations: list[NamingViolation] = []

    if guard_name.startswith("_") or "__" in guard_name:
        violations.append(
            NamingViolation(
                relative_path=relative_path,
                line=line,
                rule="header_guard_reserved_identifier",
                identifier=guard_name,
            )
        )

    violations.extend(_identifier_violations(relative_path, line, guard_name, "macro"))
    if not _macro_has_module_prefix(guard_name):
        violations.append(
            NamingViolation(
                relative_path=relative_path,
                line=line,
                rule="macro_module_prefix",
                identifier=guard_name,
            )
        )

    define_match = next(
        (
            candidate
            for candidate in DEFINE_PATTERN.finditer(declaration_source, match.end())
            if candidate.group("name") == guard_name
        ),
        None,
    )
    if define_match is None:
        violations.append(
            NamingViolation(
                relative_path=relative_path,
                line=line,
                rule="header_guard_define_missing",
                identifier=guard_name,
            )
        )

    return violations


def _scan_macros(
    relative_path: str,
    declaration_source: str,
) -> list[NamingViolation]:
    header_guard_match = (
        HEADER_GUARD_PATTERN.search(declaration_source)
        if relative_path.endswith(".h")
        else None
    )
    header_guard_name = (
        header_guard_match.group("name") if header_guard_match is not None else None
    )
    violations: list[NamingViolation] = []

    for match in DEFINE_PATTERN.finditer(declaration_source):
        macro_name = match.group("name")
        if macro_name == header_guard_name:
            continue

        line = _line_number(declaration_source, match.start("name"))
        if macro_name in LEGACY_IDENTIFIERS:
            violations.append(
                NamingViolation(
                    relative_path=relative_path,
                    line=line,
                    rule="legacy_identifier",
                    identifier=macro_name,
                )
            )
            continue

        if macro_name.startswith("_") or "__" in macro_name:
            violations.append(
                NamingViolation(
                    relative_path=relative_path,
                    line=line,
                    rule="reserved_identifier",
                    identifier=macro_name,
                )
            )
            continue

        if not UPPER_SNAKE_CASE_RE.fullmatch(macro_name):
            violations.append(
                NamingViolation(
                    relative_path=relative_path,
                    line=line,
                    rule="macro_style",
                    identifier=macro_name,
                )
            )
            continue

        if not _macro_has_module_prefix(macro_name):
            violations.append(
                NamingViolation(
                    relative_path=relative_path,
                    line=line,
                    rule="macro_module_prefix",
                    identifier=macro_name,
                )
            )

    return violations


def _scan_functions(
    relative_path: str,
    declaration_source: str,
) -> list[NamingViolation]:
    violations: list[NamingViolation] = []

    for match in FUNCTION_PATTERN.finditer(declaration_source):
        return_type = match.group("return_type").strip().split()[0]
        if return_type in NON_FUNCTION_LEADING_KEYWORDS:
            continue

        function_name = match.group("name")
        line = _line_number(declaration_source, match.start("name"))
        is_external = function_name in EXTERNAL_IDENTIFIER_EXCEPTIONS
        is_static = "static" in match.group("storage").split()

        if not is_external:
            violations.extend(
                _identifier_violations(
                    relative_path,
                    line,
                    function_name,
                    "identifier",
                )
            )
            if (
                not is_static
                and function_name != "main"
                and function_name not in LEGACY_IDENTIFIERS
                and not _identifier_has_module_prefix(function_name)
            ):
                violations.append(
                    NamingViolation(
                        relative_path=relative_path,
                        line=line,
                        rule="public_function_module_prefix",
                        identifier=function_name,
                    )
                )

            parameter_start = match.start("parameters")
            for parameter_name, parameter_offset in _parameter_names(
                match.group("parameters")
            ):
                violations.extend(
                    _identifier_violations(
                        relative_path,
                        _line_number(
                            declaration_source,
                            parameter_start + parameter_offset,
                        ),
                        parameter_name,
                        "identifier",
                    )
                )

    return violations


def _scan_type_declarations(
    relative_path: str,
    declaration_source: str,
) -> list[NamingViolation]:
    violations: list[NamingViolation] = []

    for match in TYPEDEF_NAME_PATTERN.finditer(declaration_source):
        type_name = match.group("name")
        violations.extend(
            _identifier_violations(
                relative_path,
                _line_number(declaration_source, match.start("name")),
                type_name,
                "identifier",
            )
        )

    for match in TAGGED_TYPE_PATTERN.finditer(declaration_source):
        type_name = match.group("name")
        violations.extend(
            _identifier_violations(
                relative_path,
                _line_number(declaration_source, match.start("name")),
                type_name,
                "identifier",
            )
        )

    return violations


def _scan_variable_declarations(
    relative_path: str,
    declaration_source: str,
) -> list[NamingViolation]:
    violations: list[NamingViolation] = []

    for match in VARIABLE_DECLARATION_PATTERN.finditer(declaration_source):
        declared_type = match.group("type").strip().split()[0]
        if declared_type in NON_FUNCTION_LEADING_KEYWORDS:
            continue

        line = _line_number(declaration_source, match.start("declarators"))
        for variable_name in _declarator_names(match.group("declarators")):
            if variable_name in EXTERNAL_IDENTIFIER_EXCEPTIONS:
                continue
            violations.extend(
                _identifier_violations(
                    relative_path,
                    line,
                    variable_name,
                    "identifier",
                )
            )

    return violations


def _scan_task_names(
    relative_path: str,
    comment_free_source: str,
) -> list[NamingViolation]:
    violations: list[NamingViolation] = []

    for match in TASK_NAME_PATTERN.finditer(comment_free_source):
        task_name = match.group("name")
        line = _line_number(comment_free_source, match.start("name"))
        if task_name in EXTERNAL_IDENTIFIER_EXCEPTIONS:
            continue

        if task_name in LEGACY_IDENTIFIERS:
            violations.append(
                NamingViolation(
                    relative_path=relative_path,
                    line=line,
                    rule="legacy_identifier",
                    identifier=task_name,
                )
            )
            continue

        if not LOWER_SNAKE_CASE_RE.fullmatch(task_name):
            violations.append(
                NamingViolation(
                    relative_path=relative_path,
                    line=line,
                    rule="task_name_style",
                    identifier=task_name,
                )
            )
            continue

        if not _identifier_has_module_prefix(task_name):
            violations.append(
                NamingViolation(
                    relative_path=relative_path,
                    line=line,
                    rule="task_name_module_prefix",
                    identifier=task_name,
                )
            )

    return violations


def _macro_has_module_prefix(identifier: str) -> bool:
    """Return True when the identifier starts with an uppercase module prefix."""
    return any(
        identifier.startswith(module_prefix.upper() + "_")
        for module_prefix in MODULE_PREFIXES
    )


def _identifier_has_module_prefix(identifier: str) -> bool:
    """Return True when the identifier starts with a lowercase module prefix."""
    return any(
        identifier.startswith(module_prefix + "_")
        for module_prefix in MODULE_PREFIXES
    )


def _identifier_violations(
    relative_path: str,
    line: int,
    identifier: str,
    style: str,
) -> list[NamingViolation]:
    if identifier in LEGACY_IDENTIFIERS:
        return [
            NamingViolation(
                relative_path=relative_path,
                line=line,
                rule="legacy_identifier",
                identifier=identifier,
            )
        ]

    if identifier.startswith("_") or "__" in identifier:
        return [
            NamingViolation(
                relative_path=relative_path,
                line=line,
                rule="reserved_identifier",
                identifier=identifier,
            )
        ]

    if style == "macro":
        is_valid = UPPER_SNAKE_CASE_RE.fullmatch(identifier) is not None
        rule = "macro_style"
    else:
        is_valid = LOWER_SNAKE_CASE_RE.fullmatch(identifier) is not None
        rule = "identifier_style"

    if is_valid:
        return []

    return [
        NamingViolation(
            relative_path=relative_path,
            line=line,
            rule=rule,
            identifier=identifier,
        )
    ]


def _parameter_names(parameter_text: str) -> Iterable[tuple[str, int]]:
    parameter_offset = 0

    for parameter in parameter_text.split(","):
        stripped_parameter = parameter.strip()
        if not stripped_parameter or stripped_parameter == "void" or stripped_parameter == "...":
            parameter_offset += len(parameter) + 1
            continue

        match = PARAMETER_NAME_PATTERN.search(parameter)
        if match is not None:
            candidate = match.group("name")
            if candidate not in {"const", "volatile", "restrict"}:
                yield candidate, parameter_offset + match.start("name")

        parameter_offset += len(parameter) + 1


def _declarator_names(declarator_text: str) -> Iterable[str]:
    for declarator in declarator_text.split(","):
        match = DECLARATOR_NAME_PATTERN.match(declarator.strip())
        if match is not None:
            yield match.group("name")


def _extract_user_code_regions(source_text: str) -> tuple[str, bool]:
    selected_lines: list[str] = []
    in_user_code_region = False
    found_user_code = False

    for source_line in source_text.splitlines(keepends=True):
        if USER_CODE_BEGIN_PATTERN.search(source_line):
            in_user_code_region = True
            found_user_code = True
            selected_lines.append(_blank_line(source_line))
        elif USER_CODE_END_PATTERN.search(source_line):
            in_user_code_region = False
            selected_lines.append(_blank_line(source_line))
        elif in_user_code_region:
            selected_lines.append(source_line)
        else:
            selected_lines.append(_blank_line(source_line))

    return "".join(selected_lines), found_user_code


def _mask_comments(source_text: str) -> str:
    masked_characters = list(source_text)
    index = 0

    while index < len(source_text):
        if source_text.startswith("//", index):
            end_index = source_text.find("\n", index)
            if end_index == -1:
                end_index = len(source_text)
            _blank_range(masked_characters, index, end_index)
            index = end_index
        elif source_text.startswith("/*", index):
            end_index = source_text.find("*/", index + 2)
            end_index = len(source_text) if end_index == -1 else end_index + 2
            _blank_range(masked_characters, index, end_index)
            index = end_index
        elif source_text[index] in {'"', "'"}:
            index = _skip_literal(source_text, index)
        else:
            index += 1

    return "".join(masked_characters)


def _mask_string_and_character_literals(source_text: str) -> str:
    masked_characters = list(source_text)
    index = 0

    while index < len(source_text):
        if source_text[index] in {'"', "'"}:
            end_index = _skip_literal(source_text, index)
            _blank_range(masked_characters, index, end_index)
            index = end_index
        else:
            index += 1

    return "".join(masked_characters)


def _skip_literal(source_text: str, start_index: int) -> int:
    quote = source_text[start_index]
    index = start_index + 1

    while index < len(source_text):
        if source_text[index] == "\\":
            index += 2
        elif source_text[index] == quote:
            return index + 1
        else:
            index += 1

    return len(source_text)


def _blank_range(characters: list[str], start_index: int, end_index: int) -> None:
    for index in range(start_index, end_index):
        if characters[index] not in {"\r", "\n"}:
            characters[index] = " "


def _blank_line(source_line: str) -> str:
    return "".join(
        character if character in {"\r", "\n"} else " "
        for character in source_line
    )


def _line_number(source_text: str, index: int) -> int:
    return source_text.count("\n", 0, index) + 1


def _relative_path_text(relative_path: str | Path) -> str:
    return Path(relative_path).as_posix()


def _sorted_violations(
    violations: Iterable[NamingViolation],
) -> list[NamingViolation]:
    return sorted(
        set(violations),
        key=lambda violation: (
            violation.relative_path,
            violation.line,
            violation.rule,
            violation.identifier,
        ),
    )


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Check naming rules in project-owned C source files."
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="project root to scan (defaults to this repository)",
    )
    arguments = parser.parse_args(argv)

    violations = scan_project(arguments.root)
    if violations:
        for violation in violations:
            print(violation.format())
        return 1

    print("C naming check: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
