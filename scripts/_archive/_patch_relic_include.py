#!/usr/bin/env python3
from pathlib import Path

p = Path(r"D:\UE5 demo\GameXXK\Source\GameXXK\Private\GameXXKCardBattleAdapter.cpp")
raw = p.read_bytes()
old = b'#include "GameXXKRelicRules.h"\r\n' if b"\r\n" in raw else b'#include "GameXXKRelicRules.h"\n'
new = old + b'#include "GameXXKRelicCatalog.h"\n'
count = raw.count(old)
assert count == 1, f"expected 1, got {count}"
p.write_bytes(raw.replace(old, new, 1))
print("include added")
