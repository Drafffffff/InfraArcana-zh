#!/usr/bin/env python3
"""Extract human-facing and technical text candidates from Infra Arcana v23 sources.

Output is a deduplicated JSON array with source, target, locations and kind. The
script intentionally records source locations so the catalog can later be used
for either table lookup or controlled source replacement.
"""
from __future__ import annotations

import argparse
import ast
import json
import re
import subprocess
import tarfile
import tempfile
import xml.etree.ElementTree as ET
import xml.parsers.expat as expat
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "source"
REPOSITORY = ROOT

STRING_RE = re.compile(r'(?:u8|u|U|L)?"(?:\\.|[^"\\])*"|R"([^ (\\\t\r\n]*)\((.*?)\)\1"', re.S)


def unquote(s: str) -> str:
    if s.startswith('R"'):
        m = re.fullmatch(r'R"([^ (\\\t\r\n]*)\((.*?)\)\1"', s, re.S)
        return m.group(2) if m else s
    if s.startswith(('u8"', 'u"', 'U"', 'L"')):
        s = s[s.index('"'):]
    # Python's literal parser decodes C/Python-compatible escapes while
    # preserving literal UTF-8 characters. unicode_escape over UTF-8 bytes
    # corrupts unescaped non-ASCII source text (mojibake).
    try:
        return ast.literal_eval(s)
    except (SyntaxError, ValueError):
        # C++ permits a few escape spellings that Python rejects. Preserve the
        # original Unicode text rather than corrupting it in a fallback path.
        return s[1:-1]


def kind_for(value: str, context: str = "") -> str:
    # Keep identifiers, file names, format fragments and parser vocabulary in
    # the catalog, marked technical so consumers never translate them.
    if re.fullmatch(r'(?:%?\{(?:COLOR_[A-Z0-9_]+|color_reset|reset_color|_)\})+', value):
        return "technical"
    if re.search(r'\b(?:%[0-9$+.#-]*[a-zA-Z]|\{\w+\}|\$\w+|<[^>]+>)', value):
        return "fragment"
    if value != value.strip() and re.search(r'[A-Za-z]', value):
        return "fragment"
    stripped = value.strip()
    path_like = bool(
        re.fullmatch(r'(?:[A-Za-z]:)?[/\\][\w.-]+(?:[/\\][\w.-]+)*(?:\.[A-Za-z0-9]{1,8})?', stripped)
        or re.fullmatch(r'\.{0,2}/[\w./-]+', stripped)
        or re.fullmatch(r'[\w.-]+(?:/[\w.-]+)+(?:\.[A-Za-z0-9]{1,8})?', stripped)
        or re.fullmatch(r'[\w.-]+(?:/[\w.-]+)*/', stripped)
    )
    if (not value or not re.search(r'[A-Za-z]', value)
            or path_like
            or re.fullmatch(r'(?:[\w.-]+)?\.[A-Za-z0-9]{1,5}', value.strip())
            or re.fullmatch(r'(?:COLOR|TILE|MON|ITEM|ACT|TER|SPELL|PROP|SKILL|ATTR|SFX|SOUND|MUSIC|FLAG|ID|CFG|INV|STAT|DIR|ROOM|MAP|EVENT|KEY)[A-Z0-9_]+', value)
            or re.fullmatch(r'[A-Z][A-Z0-9]+(?:_[A-Z0-9]+)+', value)
            or value in {"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X"}
            or value in {"true", "false", "nullptr"}
            or re.fullmatch(r'[a-z][a-z0-9]*(?:_[a-z0-9]+)+', value)
            or context in {"LICENSE.txt", "LICENSE-AUDIO.txt", "LICENSE-FONTS.txt",
                           "LICENSE-FONT-DEJAVU.txt", "LICENSE-FONT-SPECIAL-ELITE.txt"}):
        return "technical"
    return "ui"


def add(bag, value, location, context="", explicit_kind=None):
    if not value or not value.strip():
        return
    bag[value].append((location, explicit_kind or kind_for(value, context)))


def cpp_extract(bag, path: Path):
    text = path.read_text(encoding="utf-8", errors="replace")
    # Mask comments while preserving byte positions and newlines; strings are
    # extracted from the original text, so comment markers inside strings stay.
    masked = list(text)
    i = 0
    state = "code"
    while i < len(text):
        if state == "code":
            if text.startswith('//', i): state = "line"; masked[i:i+2] = '  '; i += 2; continue
            if text.startswith('/*', i): state = "block"; masked[i:i+2] = '  '; i += 2; continue
            # Skip ordinary and raw literals when looking for comment openers.
            m = STRING_RE.match(text, i)
            if m: i = m.end(); continue
            i += 1
        elif state == "line":
            if text[i] == '\n': state = "code"
            else: masked[i] = ' '
            i += 1
        else:
            if text.startswith('*/', i): masked[i:i+2] = '  '; state = "code"; i += 2
            else:
                if text[i] != '\n': masked[i] = ' '
                i += 1
    clean = ''.join(masked)
    rel = path.relative_to(SOURCE).as_posix()
    i = 0
    while i < len(clean):
        # Character literals can contain a double quote, which must not start a
        # string token (notably the common C++ spelling '\"').
        if clean[i] == "'":
            # C++ digit separators (e.g. 10'000 or 0xCA'FE) are not the
            # beginning of a character literal. Treat them as ordinary code.
            if (i > 0 and i + 1 < len(clean)
                    and re.match(r'[A-Za-z0-9]', clean[i - 1])
                    and re.match(r'[A-Za-z0-9]', clean[i + 1])):
                i += 1
                continue
            j = i + 1
            while j < len(clean):
                if clean[j] == "\\": j += 2; continue
                if clean[j] == "'": j += 1; break
                j += 1
            i = j
            continue
        m = STRING_RE.match(clean, i) if clean[i] == '"' or (clean[i] in 'uUL' and re.match(r'(?:u8|u|U|L)?"', clean[i:])) or clean.startswith('R"', i) else None
        if not m:
            i += 1
            continue
        start, end = m.start(), m.end()
        value = unquote(text[start:end])
        # Adjacent C++ string literals are one runtime string. Preserve source
        # location at the first token and combine before catalog deduplication.
        while True:
            j = end
            while j < len(clean) and clean[j].isspace(): j += 1
            nxt = STRING_RE.match(clean, j) if j < len(clean) and (clean[j] == '"' or clean.startswith('R"', j) or (clean[j] in 'uUL' and re.match(r'(?:u8|u|U|L)?"', clean[j:]))) else None
            if not nxt:
                break
            value += unquote(text[nxt.start():nxt.end()])
            end = nxt.end()
        if value.startswith('#include') or not re.search(r'[A-Za-z]', value):
            i = end
            continue
        line = text.count('\n', 0, start) + 1
        surroundings = clean[max(0, start - 70):min(len(clean), end + 24)]
        prefix = clean[max(0, start - 100):start]
        if rel in {"src/actor_data.cpp", "src/xml.cpp", "src/debug.cpp"}:
            # These literals are parser/schema vocabulary or diagnostics, not rendered copy.
            string_kind = "technical"
        elif rel == "src/item_curse.cpp" and value in {"PROPHECY", "THOG", "XCTHOL"}:
            # These are player-facing curse names/labels, despite enum-like casing.
            string_kind = "ui"
        elif rel == "src/map_mode_gui.cpp" and value == "Alt":
            # Sidebar label for the alternate weapon slot.
            string_kind = "ui"
        elif rel == "src/room.cpp" and value == "ROOM N/A":
            # Debug-only enum fallback; this is never shown in the game UI.
            string_kind = "technical"
        elif ((rel == "src/saving.cpp" and value in {"T", "F"})
              or (rel == "src/item_explosive.cpp" and value in {"Z", "***F"})
              or (rel == "src/attack_ranged.cpp" and value in {
                  " - ACTOR HIT: ", " - OBSTRUCTED AT IDX: ", " - PATH IDX: ",
                  " - TERRAIN HIT: ", "AIM LVL: ", "AIM POS: "})
              or (rel == "src/attack_data.cpp" and value == " projectile not at aim position, setting minimum hit chance")
              or (rel == "src/debug.cpp" and value == "Check that failed:")
              or (rel == "src/colors.cpp" and value == "ID: \"")
              or (rel == "src/audio.cpp" and value in {"Mix_GetError(): ", "l=", "r="})
              or (rel == "src/view_actor_descr.cpp" and value == "%{reset_color}.")
              or (rel == "src/version.cpp" and value.startswith("(c) "))
              or (rel == "src/main.cpp" and value.startswith("--"))
              or (rel == "installed_files/user_data.ini")
              or (rel == "installed_files/contact.txt")
              or (rel == "include/debug.hpp" and value in {"() [BEGIN]", "() [END]"})
              or (rel == "include/msg_log.hpp" and value == "(x")):
            # Save booleans and dynamite fuse markers are serialized/rendered codes.
            string_kind = "technical"
        elif (re.search(r'\b(?:TRACE(?:_\w+)?|LOG_(?:DEBUG|INFO|WARN|ERROR)|DEBUG_LOG|PANIC|ASSERT)\b[^;{}]*$', clean[max(clean.rfind(';', 0, start), clean.rfind('}', 0, start), 0):start])
                or re.search(r'xml::(?:first_child|next_sibling|has_child|get_attribute(?:_str|_int|_bool)?|get_text(?:_str|_int|_bool)?)\s*\([^;{}]*$', clean[max(clean.rfind(';', 0, start), clean.rfind('}', 0, start), 0):start])
                or re.search(r'\b(?:ini|config)\s*\[\s*$', prefix)
                or (rel == "src/paths.cpp" and value in {"test", "paths", "save", "config", "highscores"})
                or (rel == "src/ability_values.cpp" and value in {"melee", "ranged", "dodging", "stealth", "searching"})
                or (rel == "src/config.cpp" and line == 1367 and value == "x")
                or (rel == "src/config.cpp" and value == r"^(\d+)x(\d+)")):
            string_kind = "technical"
        else:
            string_kind = kind_for(value)
        add(bag, value, f"{rel}:{line}", explicit_kind=string_kind)
        i = end


def xml_extract(bag, path: Path):
    rel = path.relative_to(SOURCE).as_posix()
    try:
        root = ET.parse(path).getroot()
        parser = expat.ParserCreate()
        stack = []
        lines = {}
        def on_start(tag, attrs):
            part = tag
            identity = attrs.get("id") or attrs.get("name")
            if identity:
                part += f"[@id={identity}]"
            stack.append(part)
            lines["/" + "/".join(stack)] = parser.CurrentLineNumber
        def on_end(_tag):
            stack.pop()
        parser.StartElementHandler = on_start
        parser.EndElementHandler = on_end
        parser.Parse(path.read_bytes(), True)
    except ET.ParseError:
        return
    except expat.ExpatError:
        return
    def walk(node, parent=""):
        here = f"{parent}/{node.tag}"
        identity = node.attrib.get('id') or node.attrib.get('name')
        if identity:
            here += f"[@id={identity}]"
        loc = f"{rel}:{lines.get(here, 1)}:{here}"
        if node.text and node.text.strip():
            visible = "/text" in here and "installed_files/data/colors/" not in rel
            add(bag, node.text, loc, explicit_kind=kind_for(node.text) if visible else "technical")
        for key, value in node.attrib.items():
            if key not in {"id", "name"} and value.strip():
                add(bag, value, f"{loc}/@{key}", explicit_kind="technical")
        for child in node:
            walk(child, here)
        if node.tail and node.tail.strip():
            add(bag, node.tail, f"{loc}/tail")
    walk(root)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--output", type=Path, default=ROOT / "translations/catalog.json")
    ap.add_argument("--ref", default="upstream-v23.0.0", help="Git ref to extract from (default: upstream-v23.0.0)")
    args = ap.parse_args()
    bag = defaultdict(list)
    # Materialize the selected ref to a temporary tree so concurrent i18n code
    # edits in the working tree cannot contaminate the source catalog.
    with tempfile.TemporaryDirectory(prefix="infra-arcana-catalog-") as tmp:
        archive = subprocess.check_output(["git", "-C", str(REPOSITORY), "archive", args.ref])
        with tarfile.open(fileobj=__import__('io').BytesIO(archive)) as tf:
            tf.extractall(tmp)
        source_root = Path(tmp) / "source"
        global SOURCE
        prior_source = SOURCE
        SOURCE = source_root
        try:
            for base in (SOURCE / "src", SOURCE / "include"):
                for path in sorted(base.rglob("*")):
                    if path.suffix in {".cpp", ".hpp", ".h", ".cc", ".c"}:
                        cpp_extract(bag, path)
            for path in sorted((SOURCE / "installed_files").rglob("*")):
                if not path.is_file() or path.suffix in {".ogg", ".png", ".jpg", ".ttf", ".otf"}:
                    continue
                rel = path.relative_to(SOURCE).as_posix()
                if path.suffix == ".xml":
                    xml_extract(bag, path)
                elif path.suffix in {".txt", ".ini"}:
                    if path.name.startswith("LICENSE") or "installed_files/data/map/" in rel:
                        continue
                    for n, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                        value = line.strip()
                        if value and not value.startswith(("#", ";")):
                            technical_text = (
                                path.name in {"user_data.ini", "contact.txt"}
                                or (path.name == "manual.txt" and value in {"b j n", "h- -l", "y k u"})
                            )
                            add(bag, value, f"{rel}:{n}", path.name,
                                explicit_kind="technical" if technical_text else None)
        finally:
            SOURCE = prior_source
    rows = []
    for source in sorted(bag, key=lambda s: (s.casefold(), s)):
        locations = sorted({loc for loc, _ in bag[source]})
        kinds = {kind for _, kind in bag[source]}
        kind = "ui" if "ui" in kinds else "fragment" if "fragment" in kinds else "technical"
        technical_locations = sorted({loc for loc, item_kind in bag[source] if item_kind == "technical"})
        translatable_locations = sorted({loc for loc, item_kind in bag[source] if item_kind != "technical"})
        rows.append({
            "source": source,
            "target": source if kind == "technical" else "",
            "locations": locations,
            "kind": kind,
            "technical_locations": technical_locations,
            "translatable_locations": translatable_locations,
        })
    args.output.parent.mkdir(parents=True, exist_ok=True)
    temporary = args.output.with_suffix(args.output.suffix + ".tmp")
    temporary.write_text(json.dumps(rows, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(args.output)
    counts = {k: sum(row["kind"] == k for row in rows) for k in ("ui", "fragment", "technical")}
    print(f"Wrote {len(rows)} strings to {args.output}: {counts}")


if __name__ == "__main__":
    main()
