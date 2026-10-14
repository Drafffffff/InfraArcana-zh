#!/usr/bin/env python3
"""Merge reviewed batch JSON files into catalog.json by exact source string."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CATALOG = ROOT / "catalog.json"


def main():
    rows = json.loads(CATALOG.read_text(encoding="utf-8"))
    by_source = {row["source"]: row for row in rows}
    imported = 0
    conflicts = []
    for batch in sorted(ROOT.glob("agent-*/batch-*.json")):
        batch_rows = json.loads(batch.read_text(encoding="utf-8"))
        for item in batch_rows:
            source = item["source"]
            target = item.get("target", "")
            if source not in by_source or not target:
                continue
            row = by_source[source]
            if row["kind"] == "technical":
                row["target"] = source
                continue
            if row["target"] not in ("", source, target) and row["target"] != target:
                conflicts.append((source, row["target"], target, str(batch)))
                continue
            row["target"] = target
            imported += 1
    if conflicts:
        raise SystemExit(f"Refusing {len(conflicts)} conflicting translations; first: {conflicts[0]!r}")
    temporary = CATALOG.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(rows, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(CATALOG)
    translated = sum(row["kind"] != "technical" and row["target"] not in ("", row["source"]) for row in rows)
    pending = sum(row["kind"] != "technical" and row["target"] == "" for row in rows)
    print(f"Merged {imported} batch rows; {translated} translated, {pending} pending of {translated + pending} player-facing strings.")


if __name__ == "__main__":
    main()
