#!/usr/bin/env python3
"""Apply Luna-authored localized expectations without changing test logic."""
from pathlib import Path
import json,subprocess,collections
from apply_catalog import literals
R=Path(__file__).resolve().parents[1]
rows=json.loads((R/'translations/test-expectations.json').read_text())['mappings']
by=collections.defaultdict(dict)
for row in rows:by[row['test']][row['source']]=row['target']
count=0
for name,mapping in by.items():
 rel='source/test/test_cases/src/'+name
 text=subprocess.check_output(['git','-C',str(R),'show','upstream-v23.0.0:'+rel]).decode()
 edits=[(a,b,json.dumps(mapping[v],ensure_ascii=False)) for a,b,v in literals(text) if v in mapping]
 for a,b,v in reversed(edits):text=text[:a]+v+text[b:]
 text=text.replace('        REQUIRE(starts_with_any_of(history[0].text(), possible_messages));', '        INFO(\"Actual localized message: \" << history[0].text());\n        REQUIRE(starts_with_any_of(history[0].text(), possible_messages));')
 (R/rel).write_text(text);count+=len(edits)
print(json.dumps({'localized_expected_literals':count}))
