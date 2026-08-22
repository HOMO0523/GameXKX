# GameXXK Graduation Showcase Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a double-clickable offline HTML graduation showcase that explains GameXXK’s three core pillars, Slate/UMG architecture, numeric-testing system, and all 173 currently obtainable cards with exact effects, values, and per-card design rationale.

**Architecture:** A small standard-library Python pipeline parses the code-generated Markdown card catalog, filters the 173 active cards, merges reviewed design-note JSON, renders semantic static HTML, and copies deterministic local media. Plain CSS and vanilla JavaScript provide the water-ink archive visual system, search/filtering, deep links, responsive tables, and print support without becoming runtime dependencies for reading the content.

**Tech Stack:** Python 3.13 standard library, Pillow 12.3 for deterministic image derivatives, semantic HTML5, CSS3, vanilla JavaScript, Node.js with Playwright for offline browser checks, Unreal Automation evidence already present in the repository.

---

## Execution constraints

- Work in `D:\UE5 demo\GameXXK` on `main`; do not create a worktree.
- Do not use UnrealBridge.
- Preserve every unrelated dirty file and user-tuned UE asset. Every commit uses `git commit --only` with explicit paths.
- The final deliverable is local-only. Do not initialize Sites, add `.openai/hosting.json`, or deploy it.
- Runtime gameplay files are read-only for this task. The site may quote paths and consume generated documentation, but it must not change card rules, balance, saves, maps, Slate widgets, sprites, PaperZD assets, cameras, or HD2D placement.
- Pure presentation and media extraction do not use TDD. Parser, validation, renderer, and interaction behavior do use test-first development.
- Use these task-specific runtime variables in PowerShell:

```powershell
$showcasePython = 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe'
$showcaseNode = 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe'
$env:NODE_PATH = 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\node_modules'
```

## File structure

### Source content

- Create `docs/graduation-showcase/site-content.json`: approved title, three pillars, game loop, Slate/UMG chapter, numeric-test chapter, historical evidence labels, and production narrative.
- Create `docs/graduation-showcase/role-profiles.json`: one profile for hero, six permanent-partner roles, six quest NPCs, and boss rewards.
- Create `docs/graduation-showcase/card-notes/hero.json`: design notes for catalog rows 001–036.
- Create `docs/graduation-showcase/card-notes/partners-blade-guard.json`: rows 061–096.
- Create `docs/graduation-showcase/card-notes/partners-healer-hunter.json`: rows 097–132.
- Create `docs/graduation-showcase/card-notes/partners-sorcerer-formation.json`: rows 133–168.
- Create `docs/graduation-showcase/card-notes/npcs-boss.json`: rows 037–060 and 194–198.
- Create `docs/graduation-showcase/media-manifest.json`: exact screenshot and character-source mappings.

### Generator

- Create `scripts/graduation_showcase/__init__.py`: public package exports.
- Create `scripts/graduation_showcase/model.py`: immutable card and note dataclasses plus `ShowcaseDataError`.
- Create `scripts/graduation_showcase/catalog.py`: Markdown-row parser, catalog parser, active-card filter, buckets, and count validation.
- Create `scripts/graduation_showcase/content.py`: JSON loaders and strict content/note/profile validation.
- Create `scripts/graduation_showcase/media.py`: copy screenshots, crop transparent role portraits, and write a hash report.
- Create `scripts/graduation_showcase/render.py`: semantic HTML/card-table/cards-data renderers.
- Create `scripts/graduation_showcase/build.py`: command-line orchestration and build manifest.
- Create `scripts/graduation_showcase/templates/index.template.html`: page shell and semantic landmarks.
- Create `scripts/graduation_showcase/templates/styles.css`: the approved water-ink visual system and print rules.
- Create `scripts/graduation_showcase/templates/app.js`: progressive search, filters, counts, clear action, and hash focus.

### Tests and evidence

- Create `scripts/test_graduation_showcase_catalog.py`: parser, active-set, buckets, and count tests.
- Create `scripts/test_graduation_showcase_content.py`: profile and 173-note coverage tests.
- Create `scripts/test_graduation_showcase_render.py`: required sections, semantic structure, offline URLs, assets, and deterministic output tests.
- Create `scripts/test_graduation_showcase_browser.mjs`: Playwright search/filter/deep-link/JavaScript-disabled/offline checks.
- Create `Saved/HarnessReports/graduation-showcase/`: screenshots, print PDF, media report, browser report, and Luna review.

### Deliverable

- Create `Deliverables/GameXXK_Graduation_Showcase/index.html`.
- Create `Deliverables/GameXXK_Graduation_Showcase/assets/styles.css`.
- Create `Deliverables/GameXXK_Graduation_Showcase/assets/app.js`.
- Create `Deliverables/GameXXK_Graduation_Showcase/assets/cards-data.js`.
- Create `Deliverables/GameXXK_Graduation_Showcase/assets/build-manifest.json`.
- Create `Deliverables/GameXXK_Graduation_Showcase/assets/images/`.
- Create `Deliverables/GameXXK_Graduation_Showcase/assets/evidence/2026-08-11-198-card-review-status.md`.
- Create `Deliverables/GameXXK_Graduation_Showcase/使用说明.txt`.
- Create `Deliverables/GameXXK_Graduation_Showcase.zip` only after the directory passes every gate.

---

## Required preflight: refresh the generated card documentation

Before Task 1, connect with `UnrealMCPClient` from `scripts/ue_mcp_client.py`. If MCP is unavailable, stop and ask the user to open the existing editor/MCP entry; do not force-close or restart a possibly dirty editor. When connected:

1. Call `save_dirty_packages()`.
2. Call `clear_log_buffer()`.
3. Execute `Automation RunTests GameXXK.Data.CardDocumentation` through `execute_console_command()`.
4. Poll `get_recent_log_lines(5000)` for up to 180 seconds until the log contains both `GameXXK.Data.CardDocumentation` and `Result={Success}`; fail on `Result={Fail}` or timeout.
5. Verify `docs/design/2026-08-11-full-card-catalog.md` still parses as exactly 198 rows before continuing.

This is documentation refresh only, not compile proof. Do not use Live Coding or claim a C++ verification from it. If the generated Markdown changes, preserve and commit that exact file with Task 1; never hand-edit generated card facts to satisfy the website parser.

---

### Task 1: Parse the generated card catalog

**Files:**
- Create: `scripts/graduation_showcase/__init__.py`
- Create: `scripts/graduation_showcase/model.py`
- Create: `scripts/graduation_showcase/catalog.py`
- Create: `scripts/test_graduation_showcase_catalog.py`
- Modify if generated: `docs/design/2026-08-11-full-card-catalog.md`
- Read: `docs/design/2026-08-11-full-card-catalog.md`

- [ ] **Step 1: Write the failing Markdown parsing test**

```python
import unittest

from scripts.graduation_showcase.catalog import parse_card_catalog, split_markdown_row


class GraduationShowcaseCatalogTests(unittest.TestCase):
    def test_split_markdown_row_preserves_escaped_signature_pipes(self):
        row = r"| 001 | 青锋一式 | `Hero.Generic.QingFengYiShi` | 普通 | 1 气 / 0 内 | 单体敌方 | 初始解锁 | 造成140%攻击伤害 | `TargetMode=SingleEnemy \| Base=[Damage=140]` |"
        cells = split_markdown_row(row)
        self.assertEqual(len(cells), 9)
        self.assertEqual(cells[2], "`Hero.Generic.QingFengYiShi`")
        self.assertIn("TargetMode=SingleEnemy | Base", cells[8])

    def test_parse_card_catalog_reads_identity_cost_and_effect(self):
        source = """## 1. 主角·泛用（12 张）
| # | 卡牌 | CardId | 品质 | 费用 | 目标 | 获取 | 完整效果 | 实现签名 |
| ---: | --- | --- | --- | --- | --- | --- | --- | --- |
| 001 | 青锋一式 | `Hero.Generic.QingFengYiShi` | 普通 | 1 气 / 0 内 | 单体敌方 | 初始解锁 | 造成140%攻击伤害 | `TargetMode=SingleEnemy` |
"""
        cards = parse_card_catalog(source)
        self.assertEqual(len(cards), 1)
        card = cards[0]
        self.assertEqual(card.number, 1)
        self.assertEqual(card.card_id, "Hero.Generic.QingFengYiShi")
        self.assertEqual((card.energy, card.mana), (1, 0))
        self.assertEqual(card.effect, "造成140%攻击伤害")
        self.assertEqual(card.section, "主角·泛用")


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the parser test and verify RED**

Run:

```powershell
& $showcasePython -m unittest discover -s scripts -p 'test_graduation_showcase_catalog.py' -v
```

Expected: import failure for `scripts.graduation_showcase.catalog`.

- [ ] **Step 3: Implement the immutable model and Markdown parser**

```python
# scripts/graduation_showcase/model.py
from dataclasses import dataclass


class ShowcaseDataError(ValueError):
    pass


@dataclass(frozen=True)
class CardRecord:
    number: int
    name: str
    card_id: str
    quality: str
    energy: int
    mana: int
    target: str
    acquisition: str
    effect: str
    signature: str
    section: str
```

```python
# scripts/graduation_showcase/catalog.py
import re

from .model import CardRecord, ShowcaseDataError

CARD_ID = re.compile(r"^`([^`]+)`$")
COST = re.compile(r"^(\d+) 气 / (\d+) 内$")
SECTION = re.compile(r"^## \d+\. ([^（]+)")


def split_markdown_row(line: str) -> list[str]:
    body = line.strip()
    if not body.startswith("|") or not body.endswith("|"):
        raise ShowcaseDataError("card row must start and end with a pipe")
    return [cell.strip().replace(r"\|", "|") for cell in re.split(r"(?<!\\)\|", body[1:-1])]


def parse_card_catalog(text: str) -> list[CardRecord]:
    cards: list[CardRecord] = []
    section = ""
    for line_number, line in enumerate(text.splitlines(), start=1):
        section_match = SECTION.match(line)
        if section_match:
            section = section_match.group(1).strip()
            continue
        if not re.match(r"^\| \d{3} \|", line):
            continue
        cells = split_markdown_row(line)
        if len(cells) != 9:
            raise ShowcaseDataError(f"line {line_number}: expected 9 cells, found {len(cells)}")
        id_match = CARD_ID.match(cells[2])
        cost_match = COST.match(cells[4])
        if not section or not id_match or not cost_match:
            raise ShowcaseDataError(f"line {line_number}: malformed section, CardId, or cost")
        cards.append(CardRecord(
            number=int(cells[0]),
            name=cells[1],
            card_id=id_match.group(1),
            quality=cells[3],
            energy=int(cost_match.group(1)),
            mana=int(cost_match.group(2)),
            target=cells[5],
            acquisition=cells[6],
            effect=cells[7],
            signature=cells[8].strip("`"),
            section=section,
        ))
    if not cards:
        raise ShowcaseDataError("catalog contains no card rows")
    return cards
```

- [ ] **Step 4: Run the parser test and verify GREEN**

Run the Step 2 command. Expected: 2 tests pass.

- [ ] **Step 5: Commit the parser slice**

```powershell
git add -- scripts/graduation_showcase scripts/test_graduation_showcase_catalog.py docs/design/2026-08-11-full-card-catalog.md
git commit --only -m 'feat: parse graduation card catalog' -- scripts/graduation_showcase scripts/test_graduation_showcase_catalog.py docs/design/2026-08-11-full-card-catalog.md
```

---

### Task 2: Freeze the 173-card active-set contract

**Files:**
- Modify: `scripts/graduation_showcase/catalog.py`
- Modify: `scripts/test_graduation_showcase_catalog.py`
- Read: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp:1930`
- Read: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp:3082`

- [ ] **Step 1: Add failing tests for 198 catalog rows and 173 active rows**

```python
from collections import Counter
from pathlib import Path

from scripts.graduation_showcase.catalog import active_cards, bucket_for, validate_active_catalog


    def test_real_catalog_filters_to_the_approved_173(self):
        text = Path("docs/design/2026-08-11-full-card-catalog.md").read_text(encoding="utf-8")
        catalog = parse_card_catalog(text)
        active = active_cards(catalog)
        self.assertEqual(len(catalog), 198)
        self.assertEqual(len(active), 173)
        self.assertEqual(Counter(bucket_for(card) for card in active), {
            "hero": 36,
            "partner": 108,
            "npc": 24,
            "boss": 5,
        })
        self.assertTrue(all(not card.card_id.startswith("Route.") or card.card_id.startswith("Route.Boss.") for card in active))
        validate_active_catalog(active)
```

- [ ] **Step 2: Run and verify RED**

Run the Task 1 test command. Expected: import failure for `active_cards`.

- [ ] **Step 3: Implement active filtering and strict bucket counts**

```python
APPROVED_BUCKET_COUNTS = {"hero": 36, "partner": 108, "npc": 24, "boss": 5}


def active_cards(cards: list[CardRecord]) -> list[CardRecord]:
    return [card for card in cards if not card.card_id.startswith("Route.") or card.card_id.startswith("Route.Boss.")]


def bucket_for(card: CardRecord) -> str:
    prefixes = (
        ("Hero.", "hero"),
        ("Profession.", "partner"),
        ("Npc.", "npc"),
        ("Route.Boss.", "boss"),
    )
    for prefix, bucket in prefixes:
        if card.card_id.startswith(prefix):
            return bucket
    raise ShowcaseDataError(f"active card has unknown bucket: {card.card_id}")


def validate_active_catalog(cards: list[CardRecord]) -> None:
    from collections import Counter
    ids = [card.card_id for card in cards]
    if len(ids) != len(set(ids)):
        raise ShowcaseDataError("active CardIds must be unique")
    counts = Counter(bucket_for(card) for card in cards)
    if dict(counts) != APPROVED_BUCKET_COUNTS:
        raise ShowcaseDataError(f"active buckets changed: {dict(counts)}")
    for card in cards:
        if not all((card.name, card.quality, card.target, card.acquisition, card.effect, card.signature)):
            raise ShowcaseDataError(f"active card has an empty required field: {card.card_id}")
```

- [ ] **Step 4: Run and verify GREEN**

Expected: 3 catalog tests pass and the real catalog reports 198/173.

- [ ] **Step 5: Commit the active-set contract**

```powershell
git add -- scripts/graduation_showcase/catalog.py scripts/test_graduation_showcase_catalog.py
git commit --only -m 'test: freeze active graduation card set' -- scripts/graduation_showcase/catalog.py scripts/test_graduation_showcase_catalog.py
```

---

### Task 3: Validate structured narrative, profiles, and design notes

**Files:**
- Create: `scripts/graduation_showcase/content.py`
- Create: `scripts/test_graduation_showcase_content.py`
- Create: `docs/graduation-showcase/site-content.json`
- Create: `docs/graduation-showcase/role-profiles.json`

- [ ] **Step 1: Write failing schema tests**

```python
import json
import unittest
from pathlib import Path

from scripts.graduation_showcase.content import load_card_notes, load_role_profiles, load_site_content, validate_note_coverage


class GraduationShowcaseContentTests(unittest.TestCase):
    def test_site_content_contains_every_approved_narrative(self):
        content = load_site_content(Path("docs/graduation-showcase/site-content.json"))
        serialized = json.dumps(content, ensure_ascii=False)
        for phrase in (
            "城镇有故事，局内有策略，桌面有成长",
            "HD2D 城镇探索",
            "类《杀戮尖塔》局内卡牌挑战",
            "桌面放置刷宝提升",
            "Slate/UMG 整体界面设计",
            "2400 场锁定矩阵",
            "2520 场正交矩阵",
            "Wilson 95% 区间",
            "2118 胜、282 负",
        ):
            self.assertIn(phrase, serialized)

    def test_profiles_cover_hero_six_partners_six_npcs_and_boss(self):
        profiles = load_role_profiles(Path("docs/graduation-showcase/role-profiles.json"))
        self.assertEqual(len(profiles), 14)
        self.assertEqual(sum(profile["cardCount"] for profile in profiles), 173)
        self.assertEqual(len({profile["id"] for profile in profiles}), 14)

    def test_note_loader_requires_the_complete_design_contract(self):
        path = Path("docs/graduation-showcase/card-notes/hero.json")
        if path.exists():
            notes = load_card_notes([path])
            validate_note_coverage(notes, set(notes), {"hero"})
```

- [ ] **Step 2: Run and verify RED**

```powershell
& $showcasePython -m unittest discover -s scripts -p 'test_graduation_showcase_content.py' -v
```

Expected: import or missing-file failure.

- [ ] **Step 3: Implement strict JSON loading**

```python
# scripts/graduation_showcase/content.py
import json
from pathlib import Path

from .model import ShowcaseDataError


def _read_object(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ShowcaseDataError(f"cannot read JSON object {path}: {error}") from error
    if not isinstance(value, dict) or value.get("schemaVersion") != 1:
        raise ShowcaseDataError(f"{path} must be a schemaVersion 1 object")
    return value


def load_site_content(path: Path) -> dict:
    value = _read_object(path)
    for key in ("meta", "corePillars", "gameLoop", "cardRules", "slate", "numericTesting", "production"):
        if not value.get(key):
            raise ShowcaseDataError(f"site content missing {key}")
    return value


def load_role_profiles(path: Path) -> list[dict]:
    value = _read_object(path)
    profiles = value.get("profiles")
    if not isinstance(profiles, list):
        raise ShowcaseDataError("role profiles must be a list")
    required = {"id", "name", "kind", "cardCount", "keywords", "experienceGoal", "designProblem", "loop", "numericBoundaries", "imageId"}
    for profile in profiles:
        if not isinstance(profile, dict) or not required.issubset(profile) or not all(profile[key] for key in required):
            raise ShowcaseDataError("role profile is incomplete")
    return profiles


NOTE_FIELDS = {"profileId", "mechanics", "designRole", "intent", "numericRationale", "synergies", "risks"}


def load_card_notes(paths: list[Path]) -> dict[str, dict]:
    merged: dict[str, dict] = {}
    for path in paths:
        value = _read_object(path)
        cards = value.get("cards")
        if not isinstance(cards, dict):
            raise ShowcaseDataError(f"{path} cards must be an object")
        for card_id, note in cards.items():
            if card_id in merged:
                raise ShowcaseDataError(f"duplicate design note: {card_id}")
            if not isinstance(note, dict) or not NOTE_FIELDS.issubset(note):
                raise ShowcaseDataError(f"incomplete design note: {card_id}")
            if not all(note[field] for field in NOTE_FIELDS):
                raise ShowcaseDataError(f"empty design note field: {card_id}")
            if any(not isinstance(note[field], list) or not note[field] for field in ("mechanics", "synergies", "risks")):
                raise ShowcaseDataError(f"design note lists are invalid: {card_id}")
            if any(len("".join(note[field].split())) < 12 for field in ("intent", "numericRationale")):
                raise ShowcaseDataError(f"design note prose is too short: {card_id}")
            merged[card_id] = note
    return merged


def validate_note_coverage(notes: dict[str, dict], expected_ids: set[str], valid_profiles: set[str]) -> None:
    if set(notes) != expected_ids:
        missing = sorted(expected_ids - set(notes))
        extra = sorted(set(notes) - expected_ids)
        raise ShowcaseDataError(f"design note coverage changed; missing={missing}, extra={extra}")
    invalid_profiles = sorted({note["profileId"] for note in notes.values()} - valid_profiles)
    if invalid_profiles:
        raise ShowcaseDataError(f"unknown design-note profiles: {invalid_profiles}")
```

- [ ] **Step 4: Author the approved narrative and exact 14 profiles**

`site-content.json` uses the exact approved title/subtitle and the text from specification sections 3, 9, and 10. Its numeric-testing object contains eight layers in order, the five orthogonal dimensions, the bounded changes `{attackMultiplierPp: 15, fixedValue: 2, energy: 1, mana: 3, enemyPercent: 10}`, and the dated 2026-08-11 evidence with `cases=2400`, `victories=2118`, `defeats=282`, `winRate=0.8825`, and `maxRounds=0`.

`role-profiles.json` contains these exact profile IDs and counts:

```json
{
  "schemaVersion": 1,
  "profiles": [
    {"id":"hero","name":"主角","kind":"hero","cardCount":36},
    {"id":"partner-blade","name":"刀客","kind":"partner","cardCount":18},
    {"id":"partner-guard","name":"守卫","kind":"partner","cardCount":18},
    {"id":"partner-healer","name":"药师","kind":"partner","cardCount":18},
    {"id":"partner-hunter","name":"弓手","kind":"partner","cardCount":18},
    {"id":"partner-sorcerer","name":"法师","kind":"partner","cardCount":18},
    {"id":"partner-formation","name":"阵师","kind":"partner","cardCount":18},
    {"id":"npc-tusi","name":"土司首领","kind":"npc","cardCount":4},
    {"id":"npc-song","name":"宋金宝","kind":"npc","cardCount":4},
    {"id":"npc-yue","name":"月白","kind":"npc","cardCount":4},
    {"id":"npc-zhou","name":"周光祖","kind":"npc","cardCount":4},
    {"id":"npc-jin","name":"金贵","kind":"npc","cardCount":4},
    {"id":"npc-qiong","name":"琼么儿","kind":"npc","cardCount":4},
    {"id":"boss-rewards","name":"首领奖励牌","kind":"boss","cardCount":5}
  ]
}
```

Complete every object with the following exact profile contract; each semicolon-separated phrase becomes the value or list for the named field:

| id | keywords | experienceGoal | designProblem | loop | numericBoundaries | imageId |
|---|---|---|---|---|---|---|
| `hero` | 泛用工具、六职业联动、八牌法术任务 | 在同一套八张配置里承担补位与跨职业桥接 | 主角不能复制任一伙伴的完整职业循环 | 泛用稳定段 → 读取伙伴资源 → 联动牌兑现 → 高等级泛用牌收束 | 泛用牌以0–2气为主；5/10/15/20级各解锁一张；剑意贯虹按`260%+20%×气势` | `hero` |
| `partner-blade` | 冲锋、收招、气势、流血、反击、藏式 | 通过首牌与末牌排序把一个回合编成刀法套路 | 顺序收益必须强但不能让中间牌失去价值 | 冲锋起手 → 中段铺状态/反击 → 收招保存 → 下回合延续 | 反击为100%攻击；顺序重放不再次计主动出牌 | `partner-blade` |
| `partner-guard` | 护甲、援护、格挡、标记、全耗甲 | 主动决定保护谁以及何时把防御变成输出 | 坦克不能只延长战斗而缺少进攻决策 | 加甲 → 援护承伤 → 格挡反击 → 保甲或全耗甲群攻 | 格挡=`100%攻击+当前护甲`且不耗甲；全耗甲每点+20个百分点 | `partner-guard` |
| `partner-healer` | 药效、药方、治疗、反向治疗、中毒、灼烧 | 围绕生命变化建立治疗与伤害共用的资源循环 | 治疗职业需要主动攻击线且不能产生无限药方递归 | 制药效 → 触发生命变化 → 药方监听 → 治疗或毒爆兑现 | 单体基础治疗≤12、群体≤6；药效每层+1；首次药方基础气费+1 | `partner-healer` |
| `partner-hunter` | 流血、多段、蓄力、重箭、灵动、过牌 | 先积蓄再选择最合适的箭法释放全部蓄力 | 重箭必须保留无蓄力时的基础可用性 | 基础攻击 → 获得蓄力 → 重箭锁定并耗尽 → 多段/毒爆/抽牌 | 重箭先锁定全部蓄力；每张牌按自己的每层条款结算 | `partner-hunter` |
| `partner-sorcerer` | 五牌任务、炎法、寒冰、雷法、通用强牌 | 通过首次出牌顺序编排一轮法术仪式 | 强重放需要可保存、可恢复且禁止递归 | 首牌开任务 → 五个唯一CardId排序 → 免费重放 → 首牌奖励 | 以0气为主、最多1气；重放保存品质/目标/已付内力且不再推进任务 | `partner-sorcerer` |
| `partner-formation` | 六地势、换场、全地势收益、固定目标 | 围绕全队需求选择战场而不是只强化自己 | 条件地势不匹配时卡牌仍必须可打出 | 换场 → 触发固定地势收益 → 选择匹配收益牌 → 跨职业接力 | 六类收益；条件不符保留基础段；出生为2换场+3主流派+1自由收益 | `partner-formation` |
| `npc-tusi` | 刀客、守卫、协战、援护、寨主号令 | 用领袖式攻防指令把队友动作聚合成一回合 | 四选三任缺一张仍要同时保留刀客与守卫端 | 号令铺气势/甲 → 协战 → 格挡保护 → 收招延续 | 0–2气；协战按牌面100%或150%；援护/格挡使用守卫公式 | `npc-tusi` |
| `npc-song` | 法师、刀客、三牌任务、冲锋、收招 | 用检索和费用控制完成短任务并爆发 | 三张携带牌要自给检索且顺序奖励不能递归 | 检索未完成牌 → 三张各打一次 → 顺序重放 → 启动牌奖励 | 0–1气；任务重放免费且不再推进；奖励可回2气/抽3 | `npc-song` |
| `npc-yue` | 阵师、法师、地势、灼烧、落雷、三牌任务 | 把场地收益编进短法术任务 | 三张组合需要同时覆盖地势与法术兑现 | 每牌检索 → 地势/状态铺垫 → 三牌重放 → 启动奖励 | 0–1气；任务奖励按锁定标记或地势次数结算 | `npc-yue` |
| `npc-zhou` | 药师、阵师、药效、地势、非致死失血 | 用地理与药理把生命变化转成全队收益 | 群体治疗不能脱离自损和地势代价 | 地势收益 → 药效6 → 非致死失血 → 群体治疗/毒爆 | 0–1气；友方主动失血最低留1；药效按完整快照结算 | `npc-zhou` |
| `npc-jin` | 守卫、弓手、标记、蓄力、格挡、脱身 | 以情报和生存工具为队伍建立安全输出窗口 | 辅助牌仍要同时连接蓄力和护甲两条资源 | 标记目标 → 给协战者蓄力 → 护甲/格挡保护 → 重箭兑现 | 0–2气；重箭协战按每层40%或50%；守卫反应使用格挡 | `npc-jin` |
| `npc-qiong` | 弓手、药师、蓄力、中毒、毒爆、药效 | 在机动射击与治疗爆发间切换 | 四选三任缺一张仍需保留生产和消费端 | 灵动/蓄力 → 中毒铺层 → 毒爆 → 药效治疗 | 0–2气；药效核心值6；单体治疗12+药效；毒雾中毒6 | `npc-qiong` |
| `boss-rewards` | 黑熊、老虎、珍稀、专属槽、终局奖励 | 用极强规则牌纪念首领击杀并改变后续牌组 | 高强度不能绕过三槽上限与重复限制 | 击杀首领 → 从对应池选一 → 占用空槽 → 后续战斗入组 | 黑熊2张、老虎3张；最多3槽；重复和第四张被拒绝 | `boss-rewards` |

- [ ] **Step 5: Run GREEN and commit**

Run the Step 2 command. Expected: 2 tests pass.

```powershell
git add -- scripts/graduation_showcase/content.py scripts/test_graduation_showcase_content.py docs/graduation-showcase/site-content.json docs/graduation-showcase/role-profiles.json
git commit --only -m 'docs: author graduation showcase narrative' -- scripts/graduation_showcase/content.py scripts/test_graduation_showcase_content.py docs/graduation-showcase/site-content.json docs/graduation-showcase/role-profiles.json
```

---

### Task 4: Author all 36 hero card design notes

**Files:**
- Create: `docs/graduation-showcase/card-notes/hero.json`
- Modify: `scripts/test_graduation_showcase_content.py`
- Read: `docs/design/2026-08-11-full-card-catalog.md:28`
- Read: `docs/design/2026-08-11-gamexxk-project-plan/03-rosters-deckbuilding-and-archetypes.md`

- [ ] **Step 1: Add the failing exact-range coverage assertion**

Add this reusable helper and the hero assertion to `GraduationShowcaseContentTests`:

```python
    def assert_note_file(self, relative_path: str, numbers: range, profiles: set[str]) -> None:
        catalog_text = Path("docs/design/2026-08-11-full-card-catalog.md").read_text(encoding="utf-8")
        catalog = parse_card_catalog(catalog_text)
        expected_ids = {card.card_id for card in catalog if card.number in numbers}
        notes = load_card_notes([Path(relative_path)])
        validate_note_coverage(notes, expected_ids, profiles)

    def test_hero_notes_cover_rows_001_through_036(self):
        self.assert_note_file(
            "docs/graduation-showcase/card-notes/hero.json",
            range(1, 37),
            {"hero"},
        )
```

Import `parse_card_catalog` at the top of the test file. `load_card_notes` enforces non-empty `profileId`, `mechanics`, `designRole`, `intent`, `numericRationale`, `synergies`, and `risks`, with at least 12 non-whitespace characters in intent and numeric rationale.

- [ ] **Step 2: Run and verify RED**

Run the content test command. Expected: `hero.json` missing.

- [ ] **Step 3: Author rows 001–036**

Use this exact schema for every CardId; `profileId` must match one of the 14 profile IDs from Task 3:

```json
{
  "schemaVersion": 1,
  "cards": {
    "Hero.Generic.QingFengYiShi": {
      "profileId": "hero",
      "mechanics": ["直接伤害", "下一张减费"],
      "designRole": "泛用启动与费用桥接",
      "intent": "用一张可稳定出手的攻击牌开启回合，同时把下一张牌拉进可支付区间。",
      "numericRationale": "140%承担基础攻击价值；一次-1气只改善后续节奏，不形成永久费用膨胀。",
      "synergies": ["高费终结牌", "追风套出牌阈值", "法术任务排序"],
      "risks": ["减费目标被低价值牌消耗", "只看即时伤害会低估费用桥接"]
    }
  }
}
```

Rows 001–012 explain the generic toolbox and level-unlock curve. Rows 013–036 explain the six four-card partner-link packages in Blade, Guard, Healer, Hunter, Mage, Formation order. Every rationale must quote the card’s actual cost and at least one exact number from `完整效果`; no note may introduce an effect absent from the catalog.

- [ ] **Step 4: Run and verify GREEN**

Expected: hero note count 36, exact ID equality, all note fields valid.

- [ ] **Step 5: Commit hero notes**

```powershell
git add -- docs/graduation-showcase/card-notes/hero.json scripts/test_graduation_showcase_content.py
git commit --only -m 'docs: explain all hero card designs' -- docs/graduation-showcase/card-notes/hero.json scripts/test_graduation_showcase_content.py
```

---

### Task 5: Author Blade and Guard partner notes

**Files:**
- Create: `docs/graduation-showcase/card-notes/partners-blade-guard.json`
- Modify: `scripts/test_graduation_showcase_content.py`
- Read: `docs/design/2026-08-11-full-card-catalog.md:140`

- [ ] **Step 1: Add a failing exact-range test for rows 061–096**

```python
    def test_blade_guard_notes_cover_rows_061_through_096(self):
        self.assert_note_file(
            "docs/graduation-showcase/card-notes/partners-blade-guard.json",
            range(61, 97),
            {"partner-blade", "partner-guard"},
        )
```

- [ ] **Step 2: Verify RED**

Expected: missing notes file.

- [ ] **Step 3: Author the 36 notes**

For Blade, distinguish the two fixed cores and Blood Blade, Momentum Break, Mobile Counter, and Sheathed Sequence families; explicitly explain Charge/Finish timing, delayed retention, and reaction-source independence. For Guard, distinguish armor setup, ally protection, non-consuming armor damage, full-armor AoE conversion, Block, and taunt/Mark. Every note uses the Task 4 schema and cites real costs and numbers.

- [ ] **Step 4: Verify GREEN**

Expected: 36 exact notes, no empty arrays, no invented CardIds.

- [ ] **Step 5: Commit**

```powershell
git add -- docs/graduation-showcase/card-notes/partners-blade-guard.json scripts/test_graduation_showcase_content.py
git commit --only -m 'docs: explain blade and guard cards' -- docs/graduation-showcase/card-notes/partners-blade-guard.json scripts/test_graduation_showcase_content.py
```

---

### Task 6: Author Healer and Hunter partner notes

**Files:**
- Create: `docs/graduation-showcase/card-notes/partners-healer-hunter.json`
- Modify: `scripts/test_graduation_showcase_content.py`
- Read: `docs/design/2026-08-11-full-card-catalog.md:184`

- [ ] **Step 1: Add and run a failing exact-range test for rows 097–132**

```python
    def test_healer_hunter_notes_cover_rows_097_through_132(self):
        self.assert_note_file(
            "docs/graduation-showcase/card-notes/partners-healer-hunter.json",
            range(97, 133),
            {"partner-healer", "partner-hunter"},
        )
```

Expected: missing 36-note file.

- [ ] **Step 2: Author Healer rows 097–114**

Explain Medicine potency, ally healing versus enemy reverse-heal, Poison/Burn and poison detonation, first-play Formula surcharge, non-recursive terminal health changes, and the six-stack milestone. Cite actual target modes, costs, stack values, healing values, and first-play tradeoffs.

- [ ] **Step 3: Author Hunter rows 115–132**

Explain fixed Bleed multi-hit, Charge production, Heavy Arrow snapshot-and-consume timing, Agility survival, poison detonation, and low-energy card flow. Cite each card’s actual per-charge payload and base effect.

- [ ] **Step 4: Run GREEN**

Expected: exact 36-note coverage and valid content fields.

- [ ] **Step 5: Commit**

```powershell
git add -- docs/graduation-showcase/card-notes/partners-healer-hunter.json scripts/test_graduation_showcase_content.py
git commit --only -m 'docs: explain healer and hunter cards' -- docs/graduation-showcase/card-notes/partners-healer-hunter.json scripts/test_graduation_showcase_content.py
```

---

### Task 7: Author Sorcerer and Formation partner notes

**Files:**
- Create: `docs/graduation-showcase/card-notes/partners-sorcerer-formation.json`
- Modify: `scripts/test_graduation_showcase_content.py`
- Read: `docs/design/2026-08-11-full-card-catalog.md:228`

- [ ] **Step 1: Add and run a failing exact-range test for rows 133–168**

```python
    def test_sorcerer_formation_notes_cover_rows_133_through_168(self):
        self.assert_note_file(
            "docs/graduation-showcase/card-notes/partners-sorcerer-formation.json",
            range(133, 169),
            {"partner-sorcerer", "partner-formation"},
        )
```

Expected: missing 36-note file.

- [ ] **Step 2: Author Sorcerer rows 133–150**

Explain the five-unique-card ordered task, Fire/Ice/Lightning/Universal families, recorded target and paid-mana snapshots, free replay, start-card rewards, and why auto replay does not recurse. Each note names its sequence position rule or reward role.

- [ ] **Step 3: Author Formation rows 151–168**

Explain six terrain-switch cards, twelve all-terrain payoff cards, the basic-effect fallback, fixed target mode, and how terrain is a team-level routing decision rather than a private class tag.

- [ ] **Step 4: Run GREEN**

Expected: exact 36-note coverage.

- [ ] **Step 5: Commit**

```powershell
git add -- docs/graduation-showcase/card-notes/partners-sorcerer-formation.json scripts/test_graduation_showcase_content.py
git commit --only -m 'docs: explain sorcerer and formation cards' -- docs/graduation-showcase/card-notes/partners-sorcerer-formation.json scripts/test_graduation_showcase_content.py
```

---

### Task 8: Author all NPC and boss-reward notes

**Files:**
- Create: `docs/graduation-showcase/card-notes/npcs-boss.json`
- Modify: `scripts/test_graduation_showcase_content.py`
- Read: `docs/design/2026-08-11-full-card-catalog.md:92`
- Read: `docs/design/2026-08-11-full-card-catalog.md:305`

- [ ] **Step 1: Add and run the failing 29-note test**

```python
    def test_npc_boss_notes_cover_rows_037_060_and_194_198(self):
        numbers = set(range(37, 61)) | set(range(194, 199))
        catalog_text = Path("docs/design/2026-08-11-full-card-catalog.md").read_text(encoding="utf-8")
        expected_ids = {card.card_id for card in parse_card_catalog(catalog_text) if card.number in numbers}
        notes = load_card_notes([Path("docs/graduation-showcase/card-notes/npcs-boss.json")])
        validate_note_coverage(notes, expected_ids, {
            "npc-tusi", "npc-song", "npc-yue", "npc-zhou", "npc-jin", "npc-qiong", "boss-rewards",
        })
```

Expected: missing file.

- [ ] **Step 2: Author the 24 NPC notes**

Group by Tusi Chief, Song Jin Bao, Yue Bai, Zhou Guang Zu, Jin Gui, and Qiong Mei Er. Each group explains its two-profession bridge, 4-choose-3 resilience, and any three-card spell task or ally-attack behavior.

- [ ] **Step 3: Author the five boss notes**

Explain the two BlackBear-keyed cards, three Tiger-keyed cards, three-slot cap, duplicate rejection, and reward-versus-deck-space tradeoff. Do not call them ordinary route cards.

- [ ] **Step 4: Run the complete content suite**

Expected: 173 unique notes; exact bucket counts 36+108+24+5; all fields valid.

- [ ] **Step 5: Commit**

```powershell
git add -- docs/graduation-showcase/card-notes/npcs-boss.json scripts/test_graduation_showcase_content.py
git commit --only -m 'docs: explain npc and boss reward cards' -- docs/graduation-showcase/card-notes/npcs-boss.json scripts/test_graduation_showcase_content.py
```

---

### Task 9: Build deterministic local media derivatives

**Files:**
- Create: `docs/graduation-showcase/media-manifest.json`
- Create: `scripts/graduation_showcase/media.py`
- Generate: `Deliverables/GameXXK_Graduation_Showcase/assets/images/`
- Generate: `Saved/HarnessReports/graduation-showcase/media-report.json`

- [ ] **Step 1: Author the exact media manifest**

Include copy entries for:

```text
Saved/Codex/real_flow_after_qingshan_warmup.png                    -> screens/hd2d-town.png
Saved/HarnessReports/desktop-workbench-fixed-1920.png             -> screens/desktop-workbench.png
Saved/Codex/challenge_route_map_pie_v2.png                        -> screens/challenge-route.png
Saved/Codex/battle_open_pie_shuimo_2x_v3.png                     -> screens/card-battle.png
Saved/Codex/battle_arrow_manual_acceptance_ready.png              -> screens/safestage-arrow.png
```

Include one alpha-trim portrait entry for `SourceAssets/AnimationProcessing/Production/character_00_hero_idle/frames/frame_0000.png`. Include these twelve eight-slice vertical-atlas sources:

```text
partner_blade_idle_8dir.png
partner_guard_idle_8dir.png
partner_healer_idle_8dir.png
partner_hunter_idle_8dir.png
partner_sorcerer_idle_8dir.png
partner_formation_master_idle_8dir.png
npc_tusi_chief_idle_8dir.png
npc_song_jin_bao_idle_8dir.png
npc_yue_bai_idle_8dir.png
npc_zhou_guang_zu_idle_8dir.png
npc_jin_gui_idle_8dir.png
npc_qiong_mei_er_idle_8dir.png
```

They all resolve under `SourceAssets/PartyDeck/character-references/packed/`. Every portrait outputs a transparent 256×320 PNG with nearest-neighbor scaling and at least 16 transparent pixels on each canvas edge.

- [ ] **Step 2: Implement copy, vertical-slice, alpha-trim, and SHA reporting**

`media.py` reads the manifest, validates every source resolves inside the project, uses Pillow `Image.Resampling.NEAREST`, computes non-empty alpha bounding boxes, writes the derivative, and records source/output SHA256, dimensions, mode, and alpha bounding box.

- [ ] **Step 3: Run the media build twice**

```powershell
& $showcasePython -m scripts.graduation_showcase.media --project-root . --manifest docs/graduation-showcase/media-manifest.json --output Deliverables/GameXXK_Graduation_Showcase/assets/images --report Saved/HarnessReports/graduation-showcase/media-report.json
```

Expected: 18 outputs; the second run produces identical output hashes.

- [ ] **Step 4: Inspect the contact sheet**

Have `media.py --contact-sheet` write `Saved/HarnessReports/graduation-showcase/role-portraits-contact-sheet.png`. Check that no portrait is clipped, blurred, chroma-backed, or stretched. This is deterministic art verification, not TDD.

- [ ] **Step 5: Commit source code and manifest only**

```powershell
git add -- docs/graduation-showcase/media-manifest.json scripts/graduation_showcase/media.py
git commit --only -m 'feat: derive graduation showcase media' -- docs/graduation-showcase/media-manifest.json scripts/graduation_showcase/media.py
```

---

### Task 10: Render a complete semantic offline document

**Files:**
- Create: `scripts/graduation_showcase/render.py`
- Create: `scripts/graduation_showcase/build.py`
- Create: `scripts/graduation_showcase/templates/index.template.html`
- Create: `scripts/graduation_showcase/templates/styles.css`
- Create: `scripts/graduation_showcase/templates/app.js`
- Create: `scripts/test_graduation_showcase_render.py`
- Generate: `Deliverables/GameXXK_Graduation_Showcase/index.html`
- Generate: `Deliverables/GameXXK_Graduation_Showcase/assets/cards-data.js`

- [ ] **Step 1: Write the failing renderer contract**

The test builds into a temporary directory and asserts:

```python
self.assertEqual(html.count('data-card-row="'), 173)
self.assertIn("HD2D 城镇探索", html)
self.assertIn("类《杀戮尖塔》局内卡牌挑战", html)
self.assertIn("桌面放置刷宝提升", html)
self.assertIn("Slate/UMG 整体界面设计", html)
self.assertIn("2400 场锁定矩阵", html)
self.assertIn("2520 场正交矩阵", html)
self.assertIn("Wilson 95% 区间", html)
self.assertNotRegex(html, r'https?://')
self.assertEqual(len(set(re.findall(r'id="card-([^"]+)"', html))), 173)
```

Also assert that every card has a native `<details>` block, complete effect, design role, intent, numeric rationale, synergies, risks, and implementation signature.

- [ ] **Step 2: Run and verify RED**

```powershell
& $showcasePython -m unittest discover -s scripts -p 'test_graduation_showcase_render.py' -v
```

Expected: missing renderer/build modules.

- [ ] **Step 3: Implement the semantic renderer**

The template uses `<header>`, `<nav aria-label="章节目录">`, `<main>`, `<section>`, real headings, `<table>` with `<caption>`, and `<details>` for every card. `render.py` escapes every data field with `html.escape`, sorts cards by original number, emits `f"card-{slug_card_id(card.card_id)}"` IDs, and writes `"window.GAMEXXK_CARDS = Object.freeze(" + json.dumps(normalized_cards, ensure_ascii=False, separators=(",", ":")) + ");"`.

Card rows include these exact attributes for progressive filtering:

```html
<tr data-card-row data-card-id="Hero.Generic.QingFengYiShi" data-profile="hero" data-kind="hero" data-quality="普通" data-energy="1" data-target="单体敌方" data-mechanics="直接伤害|下一张减费">
```

The template exposes these stable controls: `#card-search`, `#profile-filter`, `#quality-filter`, `#energy-filter`, `#target-filter`, `#mechanic-filter`, `#clear-filters`, `#card-count`, `#filter-summary`, and `#no-results`. The no-results element starts hidden and the summary uses `aria-live="polite"`.

The normalized object takes `profile` from `note["profileId"]`. Define `slug_card_id(card_id: str) -> str` as `re.sub(r"[^A-Za-z0-9_-]+", "-", card_id).strip("-")`, so `Profession.Guard.TieBi` becomes `card-Profession-Guard-TieBi` in both renderer and browser tests.

`build.py` accepts `--project-root`, `--output`, and optional `--generated-at`; it loads all sources, validates 173 coverage, invokes media generation, renders outputs, copies templates, and writes the evidence Markdown into `assets/evidence/`.

Create a minimal baseline `styles.css` containing the approved `:root` color variables and a readable unstyled document flow. Create a baseline `app.js` containing only `"use strict";`; Task 12’s failing browser test proves interaction behavior is still absent before it is implemented.

- [ ] **Step 4: Run and verify GREEN**

Expected: all renderer tests pass and generated HTML contains 173 card rows.

- [ ] **Step 5: Commit renderer sources**

```powershell
git add -- scripts/graduation_showcase/render.py scripts/graduation_showcase/build.py scripts/graduation_showcase/templates/index.template.html scripts/graduation_showcase/templates/styles.css scripts/graduation_showcase/templates/app.js scripts/test_graduation_showcase_render.py
git commit --only -m 'feat: render offline graduation showcase' -- scripts/graduation_showcase/render.py scripts/graduation_showcase/build.py scripts/graduation_showcase/templates/index.template.html scripts/graduation_showcase/templates/styles.css scripts/graduation_showcase/templates/app.js scripts/test_graduation_showcase_render.py
```

---

### Task 11: Apply the approved water-ink presentation system

**Files:**
- Modify: `scripts/graduation_showcase/templates/styles.css`
- Generate: `Deliverables/GameXXK_Graduation_Showcase/assets/styles.css`

- [ ] **Step 1: Define the exact tokens and typography**

```css
:root {
  --charcoal: #24221d;
  --paper: #e8d9be;
  --paper-hi: #f3e8d2;
  --ink: #2d2a24;
  --ink-muted: #776f63;
  --wood: #6b4a2d;
  --gold: #b7862b;
  --cinnabar: #a74635;
  --pine: #526e65;
  --rule: #b5a58d;
  --serif: "Noto Serif SC", "Source Han Serif SC", SimSun, serif;
  --sans: "Noto Sans SC", "Source Han Sans SC", "Microsoft YaHei", sans-serif;
}
```

- [ ] **Step 2: Implement layout and components**

Implement a centered paper archive, sticky left chapter nav above 1100px, role chapter headers, mechanism-flow rows, metric cards, readable tables, quality badges, card `<details>`, no-results message, visible focus, and reduced-motion rules. Keep body copy 16px/1.8 and data tables at least 13px/1.55.

- [ ] **Step 3: Implement responsive and print rules**

At 1100px collapse the left nav into a top chapter strip. At 760px render table rows as labeled cards using `data-label`. Under `@media print`, hide navigation/search controls, force card details visible, keep headings with following content, repeat table headers, and avoid splitting an individual card across pages.

- [ ] **Step 4: Rebuild and perform deterministic CSS checks**

Run the build command from Task 13 and verify no `url(http`, remote font import, pure white body background, or text smaller than 12px exists.

- [ ] **Step 5: Commit CSS**

```powershell
git add -- scripts/graduation_showcase/templates/styles.css
git commit --only -m 'feat: style graduation showcase archive' -- scripts/graduation_showcase/templates/styles.css
```

---

### Task 12: Add progressive search, filters, and deep-link focus with TDD

**Files:**
- Create: `scripts/test_graduation_showcase_browser.mjs`
- Modify: `scripts/graduation_showcase/templates/app.js`
- Generate: `Deliverables/GameXXK_Graduation_Showcase/assets/app.js`

- [ ] **Step 1: Write the failing Playwright behavior script**

Use this executable structure, then add the profile/energy and no-result assertions listed below it:

```javascript
import assert from "node:assert/strict";
import {mkdir, writeFile} from "node:fs/promises";
import {dirname, resolve} from "node:path";
import {fileURLToPath, pathToFileURL} from "node:url";
import {createRequire} from "node:module";

const require = createRequire(import.meta.url);
const {chromium} = require("playwright");
const projectRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const indexUrl = pathToFileURL(resolve(projectRoot, "Deliverables/GameXXK_Graduation_Showcase/index.html")).href;
const reportDir = resolve(projectRoot, "Saved/HarnessReports/graduation-showcase");
const browser = await chromium.launch({headless: true});
const protocols = new Set();

try {
  const context = await browser.newContext({viewport: {width: 1440, height: 1000}});
  const page = await context.newPage();
  page.on("request", request => protocols.add(new URL(request.url()).protocol));
  await page.goto(indexUrl);
  const visibleCount = () => page.locator('[data-card-row]:not([hidden])').count();
  assert.equal(await visibleCount(), 173);

  await page.locator("#card-search").fill("铁壁");
  await page.locator("#card-search").press("Enter");
  assert.ok((await visibleCount()) > 0);
  assert.ok(await page.locator('[data-card-id="Profession.Guard.TieBi"]:not([hidden])').isVisible());

  await page.locator("#clear-filters").click();
  await page.locator("#profile-filter").selectOption("partner-guard");
  await page.locator("#energy-filter").selectOption("1");
  const filteredRows = page.locator('[data-card-row]:not([hidden])');
  assert.ok((await filteredRows.count()) > 0);
  for (let index = 0; index < await filteredRows.count(); index += 1) {
    assert.equal(await filteredRows.nth(index).getAttribute("data-profile"), "partner-guard");
    assert.equal(await filteredRows.nth(index).getAttribute("data-energy"), "1");
  }

  await page.locator("#card-search").fill("不存在的卡牌关键字XYZ");
  await page.locator("#card-search").press("Enter");
  assert.equal(await visibleCount(), 0);
  assert.ok(await page.locator("#no-results").isVisible());
  assert.ok((await page.locator("#filter-summary").innerText()).includes("partner-guard"));

  await page.locator("#clear-filters").click();
  assert.equal(await visibleCount(), 173);
  assert.equal(await page.locator("#card-search").inputValue(), "");
  assert.equal(await page.locator("#profile-filter").inputValue(), "");
  assert.equal(await page.locator("#energy-filter").inputValue(), "");

  await page.evaluate(() => { location.hash = "card-Profession-Guard-TieBi"; });
  await page.waitForFunction(() => document.activeElement?.id === "card-Profession-Guard-TieBi");
  assert.equal(await page.evaluate(() => document.activeElement?.id), "card-Profession-Guard-TieBi");
  await context.close();

  const noJsContext = await browser.newContext({javaScriptEnabled: false});
  const noJsPage = await noJsContext.newPage();
  await noJsPage.goto(indexUrl);
  assert.equal(await noJsPage.locator("[data-card-row]").count(), 173);
  assert.ok((await noJsPage.locator("body").innerText()).includes("完整效果"));
  await noJsContext.close();

  assert.deepEqual([...protocols], ["file:"]);
  await mkdir(reportDir, {recursive: true});
  await writeFile(resolve(reportDir, "browser-report.json"), JSON.stringify({ok: true, cards: 173, protocols: [...protocols]}, null, 2));
} finally {
  await browser.close();
}
```

The complete script must test:

1. Initial visible card rows = 173.
2. Searching `铁壁` leaves matching rows and includes `Profession.Guard.TieBi`.
3. Choosing `partner-guard` and energy `1` applies both filters.
4. A no-result query displays the no-results state and filter summary.
5. Clear restores 173 rows and empties every control.
6. Navigating to `#card-Profession-Guard-TieBi` focuses/reveals the card.
7. With JavaScript disabled, all 173 cards and every complete effect remain readable.
8. Every browser request uses `file:`; no HTTP/HTTPS request occurs.

- [ ] **Step 2: Run and verify RED**

```powershell
& $showcaseNode scripts/test_graduation_showcase_browser.mjs
```

Expected: failure because controls have no behavior or `app.js` is absent.

- [ ] **Step 3: Implement minimal progressive behavior**

Implement this single filtering and hash-focus path:

```javascript
(() => {
  "use strict";
  const rows = [...document.querySelectorAll("[data-card-row]")];
  const search = document.querySelector("#card-search");
  const selects = {
    profile: document.querySelector("#profile-filter"),
    quality: document.querySelector("#quality-filter"),
    energy: document.querySelector("#energy-filter"),
    target: document.querySelector("#target-filter"),
    mechanics: document.querySelector("#mechanic-filter"),
  };
  const count = document.querySelector("#card-count");
  const summary = document.querySelector("#filter-summary");
  const noResults = document.querySelector("#no-results");
  const normalize = value => String(value || "").normalize("NFKC").toLocaleLowerCase("zh-CN").trim();

  function applyFilters() {
    const query = normalize(search.value);
    const active = Object.entries(selects).filter(([, control]) => control.value);
    let visible = 0;
    for (const row of rows) {
      const textMatch = !query || normalize(`${row.dataset.cardId} ${row.textContent}`).includes(query);
      const filterMatch = active.every(([key, control]) => {
        const value = normalize(row.dataset[key]);
        const selected = normalize(control.value);
        return key === "mechanics" ? value.split("|").includes(selected) : value === selected;
      });
      row.hidden = !(textMatch && filterMatch);
      if (!row.hidden) visible += 1;
    }
    count.textContent = `${visible} / ${rows.length}`;
    summary.textContent = [query, ...active.map(([, control]) => control.value)].filter(Boolean).join(" · ") || "全部卡牌";
    noResults.hidden = visible !== 0;
  }

  function focusHash() {
    if (!location.hash) return;
    const target = document.getElementById(decodeURIComponent(location.hash.slice(1)));
    if (!target) return;
    const details = target.querySelector("details");
    if (details) details.open = true;
    target.tabIndex = -1;
    target.scrollIntoView({block: "center"});
    target.focus({preventScroll: true});
  }

  search.addEventListener("input", applyFilters);
  for (const control of Object.values(selects)) control.addEventListener("change", applyFilters);
  document.querySelector("#clear-filters").addEventListener("click", () => {
    search.value = "";
    for (const control of Object.values(selects)) control.value = "";
    applyFilters();
  });
  window.addEventListener("hashchange", focusHash);
  window.GameXXKShowcase = Object.freeze({applyFilters});
  applyFilters();
  focusHash();
})();
```

- [ ] **Step 4: Rebuild and verify GREEN**

Expected: the browser script exits 0 and writes a JSON summary under `Saved/HarnessReports/graduation-showcase/browser-report.json`.

- [ ] **Step 5: Commit interaction sources and tests**

```powershell
git add -- scripts/graduation_showcase/templates/app.js scripts/test_graduation_showcase_browser.mjs
git commit --only -m 'feat: add offline card exploration' -- scripts/graduation_showcase/templates/app.js scripts/test_graduation_showcase_browser.mjs
```

---

### Task 13: Generate and validate the complete deliverable

**Files:**
- Modify: `scripts/graduation_showcase/build.py`
- Modify: `scripts/test_graduation_showcase_render.py`
- Generate: `Deliverables/GameXXK_Graduation_Showcase/assets/build-manifest.json`
- Create: `Deliverables/GameXXK_Graduation_Showcase/使用说明.txt`

- [ ] **Step 1: Add failing build-manifest assertions**

Require `schemaVersion=1`, `activeCardCount=173`, bucket counts, sorted 173 CardIds, source and design-note SHA256 values, every generated file SHA256, media missing list empty, and a supplied ISO-8601 `generatedAt`.

- [ ] **Step 2: Verify RED**

Run the renderer test command. Expected: missing manifest fields.

- [ ] **Step 3: Implement manifest and usage instructions**

`使用说明.txt` contains exactly these operational facts:

```text
GameXXK 毕业设计离线介绍页

1. 双击同目录下的 index.html。
2. 推荐使用当前稳定版 Microsoft Edge 或 Google Chrome。
3. 页面不需要联网、安装依赖或启动服务器。
4. 搜索和筛选失效时，向下滚动仍可阅读全部正文与173张现役卡牌。
5. 打印或导出 PDF 时，请使用浏览器的“打印”功能并启用背景图形。
```

- [ ] **Step 4: Run the full local build and tests**

```powershell
& $showcasePython -m scripts.graduation_showcase.build --project-root . --output Deliverables/GameXXK_Graduation_Showcase
& $showcasePython -m unittest discover -s scripts -p 'test_graduation_showcase_*.py' -v
& $showcaseNode scripts/test_graduation_showcase_browser.mjs
```

Expected: all tests pass, active count 173, no missing media, browser report `ok=true`.

- [ ] **Step 5: Commit source changes and generated deliverable directory**

```powershell
git add -- scripts/graduation_showcase/build.py scripts/test_graduation_showcase_render.py Deliverables/GameXXK_Graduation_Showcase
git commit --only -m 'feat: build GameXXK graduation showcase' -- scripts/graduation_showcase/build.py scripts/test_graduation_showcase_render.py Deliverables/GameXXK_Graduation_Showcase
```

---

### Task 14: Capture responsive, print, and offline evidence

**Files:**
- Modify: `scripts/test_graduation_showcase_browser.mjs`
- Generate: `Saved/HarnessReports/graduation-showcase/showcase-1920.png`
- Generate: `Saved/HarnessReports/graduation-showcase/showcase-1440.png`
- Generate: `Saved/HarnessReports/graduation-showcase/showcase-1024.png`
- Generate: `Saved/HarnessReports/graduation-showcase/showcase-print.pdf`

- [ ] **Step 1: Extend the browser harness to capture exact viewports**

Capture the same top/core, Slate, numeric-test, Guard-card, and NPC-card anchors at widths 1920, 1440, and 1024. Assert `scrollWidth <= clientWidth` for the document and each card table at every width.

- [ ] **Step 2: Capture print output**

Use Playwright `page.emulateMedia({media: "print"})` and `page.pdf({format: "A4", printBackground: true})`. Assert the PDF is non-empty and every one of the 173 card IDs appears in the print DOM before export.

- [ ] **Step 3: Test a Chinese-and-space path**

Copy the deliverable to `D:\GameXXKBuildTemp\毕设 页面\GameXXK_Graduation_Showcase`, load its `file:///` URL, rerun search and JavaScript-disabled checks, then leave the copy as disposable evidence outside the repository.

- [ ] **Step 4: Run the harness**

Expected: all widths have zero overflow, screenshots and PDF exist, file-only network policy passes.

- [ ] **Step 5: Commit the enhanced harness only**

```powershell
git add -- scripts/test_graduation_showcase_browser.mjs
git commit --only -m 'test: verify graduation showcase offline layouts' -- scripts/test_graduation_showcase_browser.mjs
```

---

### Task 15: Run Luna visual review and fix presentation defects

**Files:**
- Modify for confirmed findings only: `scripts/graduation_showcase/templates/styles.css`
- Modify for confirmed findings only: `scripts/graduation_showcase/templates/index.template.html`
- Modify for confirmed findings only: `docs/graduation-showcase/media-manifest.json`
- Generate: `Saved/HarnessReports/graduation-showcase/luna-visual-review.json`

- [ ] **Step 1: Submit the captured screenshots to the mandated visual agent**

```powershell
& 'C:\Users\shxuw\.claude\skills\codex-vision\scripts\codex_vision.ps1' `
  -Prompt '复核 GameXXK 毕设离线 HTML：检查三大核心是否先于卡表清晰出现；水墨纸本风格是否克制；Slate 和数值测试章节是否像专业技术说明；173张卡表是否可读；1920/1440/1024 是否存在截断、遮挡、低对比、过密、图片变形或错误层级。只报告截图中可见的事实，并明确指出无异常的项目。' `
  -Images @(
    'D:\UE5 demo\GameXXK\Saved\HarnessReports\graduation-showcase\showcase-1920.png',
    'D:\UE5 demo\GameXXK\Saved\HarnessReports\graduation-showcase\showcase-1440.png',
    'D:\UE5 demo\GameXXK\Saved\HarnessReports\graduation-showcase\showcase-1024.png'
  ) `
  -Effort max `
  -Out 'D:\UE5 demo\GameXXK\Saved\HarnessReports\graduation-showcase\luna-visual-review.json' `
  -Title 'GameXXK graduation showcase final review' `
  -Workspace 'D:\UE5 demo\GameXXK'
```

- [ ] **Step 2: Classify every finding**

For each finding record `confirmed`, `not-reproduced`, or `outside-scope`. Only confirmed screenshot defects authorize CSS/template/media-manifest changes.

- [ ] **Step 3: Apply the smallest presentation fixes**

Do not modify gameplay code or source art. Fix only spacing, font size, contrast, overflow, responsive grouping, object-fit/object-position, or print rules supported by the evidence.

- [ ] **Step 4: Rebuild, recapture, and rerun Luna once**

Expected: no confirmed clipping, overlap, unreadable text, distorted image, or broken hierarchy remains.

- [ ] **Step 5: Commit verified presentation corrections**

```powershell
git add -- scripts/graduation_showcase/templates/styles.css scripts/graduation_showcase/templates/index.template.html docs/graduation-showcase/media-manifest.json Deliverables/GameXXK_Graduation_Showcase
git commit --only -m 'fix: polish graduation showcase presentation' -- scripts/graduation_showcase/templates/styles.css scripts/graduation_showcase/templates/index.template.html docs/graduation-showcase/media-manifest.json Deliverables/GameXXK_Graduation_Showcase
```

If Luna finds no defects, skip this commit.

---

### Task 16: Final verification, package, and handoff

**Files:**
- Create: `Deliverables/GameXXK_Graduation_Showcase.zip`
- Create: `docs/production/2026-08-23-gamexxk-graduation-showcase-acceptance.md`

- [ ] **Step 1: Run fresh verification from the final source**

```powershell
& $showcasePython -m scripts.graduation_showcase.build --project-root . --output Deliverables/GameXXK_Graduation_Showcase
& $showcasePython -m unittest discover -s scripts -p 'test_graduation_showcase_*.py' -v
& $showcaseNode scripts/test_graduation_showcase_browser.mjs
git diff --check -- docs/graduation-showcase scripts/graduation_showcase scripts/test_graduation_showcase_* Deliverables/GameXXK_Graduation_Showcase
```

Expected: all tests green, browser report `ok=true`, diff check clean, manifest active count 173.

- [ ] **Step 2: Verify no network dependencies**

```powershell
rg -n 'https?://|@import|url\(["'']?//' Deliverables/GameXXK_Graduation_Showcase
```

Expected: no matches.

- [ ] **Step 3: Create the ZIP reproducibly**

Resolve and verify `D:\UE5 demo\GameXXK\Deliverables\GameXXK_Graduation_Showcase` before packaging. Use `Compress-Archive -Force` to create `D:\UE5 demo\GameXXK\Deliverables\GameXXK_Graduation_Showcase.zip`, extract it to a temporary directory, and rerun the Playwright smoke against the extracted `index.html`.

- [ ] **Step 4: Record acceptance without overstating game freshness**

Create the dedicated acceptance record with the HTML path, ZIP path, 173-card manifest count, site-test results, offline browser evidence, and Luna report. State that the 2026-08-11 simulation card is historical evidence rather than a fresh game-balance certification. Do not edit or commit the already-dirty rolling `docs/production/current-goal-acceptance.md` in this work package.

- [ ] **Step 5: Commit acceptance and package**

```powershell
git add -- Deliverables/GameXXK_Graduation_Showcase.zip docs/production/2026-08-23-gamexxk-graduation-showcase-acceptance.md
git commit --only -m 'docs: deliver GameXXK graduation showcase' -- Deliverables/GameXXK_Graduation_Showcase.zip docs/production/2026-08-23-gamexxk-graduation-showcase-acceptance.md
```

- [ ] **Step 6: Final handoff**

Return clickable links to `index.html`, the showcase folder, the ZIP, the build manifest, and the Luna report. Report exact test counts from the fresh commands and do not claim any Unreal compile or gameplay suite was rerun unless it actually was.
