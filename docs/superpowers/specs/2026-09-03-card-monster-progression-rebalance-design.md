# Card, monster phase, progression, and settlement rebalance

**Status:** Approved base design; current user-led design review pauses runtime implementation. The Ice and Universal card revisions below record the latest user wording and explicit calculation interpretations.

**Date:** 2026-09-03

**Workspace:** root checkout on the user-authorized `codex/overall-in-run-optimization` branch; no worktrees

## 1. Intent and authority

This specification records the user-approved combat, card, monster, progression, status-UI, terrain, and settlement decisions from the 2026-09-02 through 2026-09-03 design review. This commit is documentation only. It does not authorize runtime, Blueprint, art, map, or asset changes yet.

When this specification conflicts with an older design document, generated card catalog, test expectation, or current implementation, this specification wins. Card IDs, names, costs, target modes, and semantics not explicitly replaced here are inherited from:

- `Source/GameXXK/Private/GameXXKCardCatalog.cpp`;
- `docs/design/2026-08-11-full-card-catalog.md`;
- the current enemy catalog, only where this specification does not override it.

The implementation must regenerate or update derived catalogs after the runtime becomes authoritative. It must not try to preserve conflicting numeric output merely because an older test locked it.

## 2. Scope and non-goals

In scope:

- the 173-card active player pool and its quality/numeric rules;
- player armor, damage-over-time, healing, Medicine, task, formula, and terrain behavior;
- equipment and gem contribution changes relevant to the combat budget;
- 27 Training stages at levels 5 through 135;
- all 21 enemies, all ordinary intents, and all elite/Boss phase decks;
- multi-phase UI and save behavior;
- authored three-enemy formations and ordered intent forecasting;
- single-map Boss settlement and future multi-map Boss-card rewards;
- deterministic full-card and full-formation certification.

Out of scope for this documentation commit:

- changing C++, Blueprint, Unreal assets, maps, or saves;
- creating status art (the user declined a visual-generation pass);
- implementing the future multi-map mode itself;
- balancing by silently changing the approved number of phases or stage levels.

## 3. Active card pool

The player-facing active pool is exactly 173 cards:

| Source | Count |
|---|---:|
| Hero generic and profession cards | 36 |
| Six permanent partners | 108 |
| Six task NPCs | 24 |
| Boss reward cards | 5 |
| **Total** | **173** |

The 20 `Route.General.*`/`Route.Terrain.*` cards and five `Route.Rare.*` cards are retired. They must not register in the active catalog, appear in rewards, enter decks, or count in documentation totals.

The five `Route.Boss.*` IDs remain only as save-compatible IDs and are presented to players as **Boss cards**, not Route cards. Their acquisition rules are in section 11.

## 4. Shared numeric rules

### 4.1 Quality

| Quality | Multiplier |
|---|---:|
| Common | 1.0 |
| Rare | 1.2 |
| Epic | 1.4 |

Damage, healing, fixed damage, ally attacks, DOT coefficients, and defense-derived or Mana-derived Armor use the quality multiplier and round upward. Mana recovery itself remains an unscaled resource operation. Old output based on quality x2/x4 is obsolete.

Player-facing card-quality labels are `普通 / 稀有 / 史诗`; the existing internal `Epic` enum maps to the user-facing `史诗` tier. Older generated text that calls this card tier `珍稀` must be reconciled rather than creating a fourth card quality.

Discrete counts do not automatically scale: status layers, Energy, Mana, draw, discard, replay, reaction uses, task pieces, terrain-trigger counts, and cleanse categories change only when the card explicitly supplies a quality table.

Numeric values explicitly labeled `Rare` or `Epic` in the card override tables are already the final authored value at that quality and must not be multiplied again. An unlabeled base coefficient is multiplied exactly once by the card's runtime quality.

### 4.2 Level-difference damage

Player levels remain capped at 100. Enemy combat levels and equipment item levels may reach 135.

For direct-attack and fixed-damage packets, compare attacker and target combat levels. Each level of difference changes damage by one percent, clamped to `[-50%, +50%]`. The adjustment applies after the card's attack/fixed-damage calculation and defense, using one deterministic rounding rule shared by preview and resolution. DOT-reservoir damage is excluded because the visible reservoir is already its final damage number; do not apply a second level-difference or difficulty multiplier to it.

Training enemy damage also uses a difficulty multiplier:

| Difficulty | Damage multiplier |
|---|---:|
| Normal | 100% |
| Hard | 125% |
| Hell | 150% |

There is no extra difficulty HP or Defense multiplier. Difficulty strength comes from stage level, damage multiplier, intent values, formations, and phases.

Keep each enemy's current catalog base HP/Attack/Defense and per-level growth and extrapolate them through combat level 135. Phase changes never mutate those base stats; stronger phases use stronger decks and passives.

For direct attacks, use one shared preview/resolution order: resolve Attack coefficient and quality; apply the enemy difficulty multiplier when applicable; apply source Weak; subtract effective target Defense; apply Mark/Vulnerability; apply the clamped level-difference modifier; then absorb with Armor and resolve post-card reactions/passives. Fixed damage bypasses Attack and Defense but still uses its approved quality and level-difference rules. DOT reservoir packets bypass this chain and deal their visible reservoir value.

### 4.3 Defense-derived armor

An effect explicitly authored as Defense-derived Armor is calculated from the **caster's current Defense**, not the recipient's Defense:

`Armor = ceil(caster Defense * printed-cost factor * quality multiplier)`

| Printed Energy cost | Base factor |
|---|---:|
| 0 | 40% |
| 1 | 80% |
| 2 | 140% |
| 3 or more | 200% |

Temporary discounts or free-play effects do not change the armor tier; use the printed base Energy cost. Card faces and combat presentation show only the resolved armor number, not the percentage.

Group armor uses the caster's value and grants the full resolved amount independently to every unique living ally. It is never split. A unit named twice by the same card receives the primary armor once unless an explicit secondary coefficient is part of the approved card semantics.

Mixed effects use their explicit secondary Defense coefficient, such as 20%, 40%, or 50%. There is no gameplay cap of 99 armor; use saturating integer arithmetic only for technical overflow protection.

### 4.3.1 Mana overflow converted to Armor

The latest user clarification applies the DOT **generation multiplier** to Mana overflow converted into Armor. Mana points are not Defense-percentage points, and Defense does not participate in this conversion:

`OverflowArmor = ceil(OverflowMana * CardConversionRate * Q * (TeamMaxLevel / 25 + 1))`

All currently specified Ice overflow conversions use `CardConversionRate = 1` (100%). Read the existing battle-start `TeamMaxLevel` snapshot. Reuse the shared integer generation arithmetic; do not apply a DOT reservoir cap to Armor or multiply the resolved Armor by quality/level again when it is copied, doubled, refunded, or consumed for an attack.

For the revised four partner Ice cards, percentage recovery is `ceil(current Mana * RecoveryPercent / 100)`. Take current Mana at the moment that effect resolves, round the recovery upward first, restore up to the current maximum, then convert only the actual overflow. Do not multiply recovered Mana by quality, level, or Defense. Each replay recomputes recovery from its then-current Mana; it does not reuse the amount recovered on the first play. Explicit fixed battle effects such as `MaxMana +4/+8` remain resource effects.

An Ice partner task enables this overflow rule for Mana recovery from that partner's Sorcerer cards. A Universal starter followed by an Ice second card enables the branch before the second card resolves; its replays use the locked Ice branch. `HanXu` retains its separately authored Hero card effects, with only its overflow using the new conversion formula. This subsection does not turn every ordinary Mana recovery in other task branches into an Armor effect.

The review's numeric fixtures use level-one raw Mana with no equipment/level growth (partner 34; Hero 30) and no extra fixed bonus. The broader Mana-pool/equipment-growth reconciliation identified in the review remains a prerequisite before full balance certification; the current implementation's growing Mana pool must not silently replace that fixture.

### 4.4 Armor conversion and reactions

Armor-to-damage conversion has no gameplay cap. Each consumed Armor adds **one attack-percentage point**. The general rebalance base is the old base plus 100 percentage points, except where a later explicit card rule overrides it. The latest user override sets the partner standard Ice base to 100% (not 200%); separately authored conversion attacks keep their own explicit bases. Quality scales the base attack portion only; it never scales the `+1 point per Armor` portion.

Block resolves after the entire opposing single-target attack card has finished. It uses the defender's post-card remaining Armor and triggers once per opposing card, not once per hit:

`Block damage = 100% current Attack + post-card remaining Armor`

Counter likewise triggers once per opposing single-target direct attack card and deals 100% current Attack. Group attacks, DOT, Counter, and Block never recursively trigger Counter or Block. A defeated reaction owner cannot react.

For player reactions, the card that registered Counter/Block supplies its quality to the 100% Attack portion; do not quality-scale the carried Armor a second time. Enemy reactions use implicit quality 1.0, then the normal enemy difficulty/level damage pipeline.

### 4.5 DOT reservoirs

The four DOT reservoirs are Bleed, Poison, Burn, and Rot. Capture the highest player-party combat level at battle start as `TeamMaxLevel`.

For a card with base DOT coefficient `C` and quality multiplier `Q`:

`AddedDOT = ceil(C * Q * (TeamMaxLevel / 25 + 1))`

The division is real-valued. This is not a step multiplier that waits for level 25. Apply the ceiling to the whole expression.

The cap of each independent DOT reservoir is:

`DOTCap = max(25, 25 * ceil(TeamMaxLevel / 25))`

| Team maximum level | Cap |
|---|---:|
| 1-25 | 25 |
| 26-50 | 50 |
| 51-75 | 75 |
| 76-100 | 100 |
| 101-125 (future/robustness) | 125 |
| 126-135 (future/robustness) | 150 |

The reservoir's visible value is the damage value. Never multiply it by level a second time. Applying DOT reports the actual amount that fits below the cap. Cards, status tooltips, previews, and combat logs show resolved values only.

An ordinary "trigger DOT" effect reads the current reservoir and does not consume it. A card consumes DOT only if its approved text explicitly says so. Approved cleanses are all-or-nothing: "remove Bleed" clears the whole Bleed reservoir; "remove all DOT" clears all four reservoirs.

On an enemy phase transition, every negative reservoir on that enemy is cleared. DOT-to-attack conversion uses `+2 attack-percentage points` per point in the resolved reservoir; old `+10 per layer` behavior is retired.

### 4.6 Healing and Medicine

Player healing uses coefficients, normally 10 through 50:

`Healing = ceil((healing coefficient + current owner Medicine) * quality multiplier * (TeamMaxLevel / 25 + 1))`

Medicine is an owner-scoped, battle-local integer reservoir. It starts at zero and resets after battle. A successful heal or Medicine reversal consumes the owner's complete current Medicine reservoir once. A group heal consumes once and grants the full resolved amount to each living ally.

Medicine generation remains a small explicit integer, generally 1 through 8, and does not receive automatic level or quality scaling. Track cumulative Medicine gained separately: every cumulative six Medicine grants one Momentum and the remainder persists. Spending Medicine does not undo cumulative progress.

Hero and partner Healer formulas are independently owned. A Hero formula can consume or update only Hero state; a partner formula can consume or update only that partner's state. The same battle event may satisfy both independently.

Enemy shared-Energy theft stores an additive penalty for the next player-round refill, applies it once with a minimum resulting Energy of zero, and then clears it. White Ape/Money Rat next-card surcharges are a separate single pending +1 that refreshes rather than stacks.

### 4.7 Retained core status semantics

- Mark is a binary damage amplifier with duration expressed by layers: while at least one layer exists, direct damage receives the existing 15% Mark bonus, then each direct hit consumes one layer. It is not +15% per layer. Enemy single-target intents prioritize Marked party members unless an explicit persistent target such as Prey overrides them.
- Vulnerability keeps its existing +10% direct-damage amplification per layer and normal per-hit consumption behavior.
- Weak makes direct attacks deal 50% less damage and loses one layer at its normal round boundary.
- Agility, Guard links, Counter, Block, Charge, and Prey keep their established visible resolution order except where this specification explicitly replaces it.

## 5. Equipment, gems, and late-game benchmark

Deterministic equipment Attack contribution is approximately halved from the current curve. Equipment base Defense and HP contribution are also halved. Gem MaxHealth uses five times its same-rank Attack/Defense value rather than the old ten-times layer; do not halve innate character HP a second time. The approved post-reduction level-100 late-game reference stats used for the Hell 3-3 calculations **already include** six Treasure equipment pieces and the balanced four-Attack/four-Defense/four-HP Treasure-gem package:

| Role | HP | Attack | Defense |
|---|---:|---:|---:|
| Hero | 2659 | 592 | 392 |
| Blade | 2371 | 563 | 285 |
| Guard | 2800 | 425 | 358 |
| Healer | 2238 | 423 | 286 |
| Hunter | 2232 | 562 | 272 |
| Sorcerer | 2094 | 495 | 257 |
| Formation Master | 2374 | 452 | 301 |

Gem growth after Immortal uses x1.25 with upward rounding, not x2:

| Gem quality (rank) | Attack | Defense | HP |
|---|---:|---:|---:|
| Common (1) | 1 | 1 | 5 |
| Rare (2) | 2 | 2 | 10 |
| Epic (3) | 4 | 4 | 20 |
| Legendary (4) | 8 | 8 | 40 |
| Immortal (5) | 16 | 16 | 80 |
| Treasure (6) | 20 | 20 | 100 |
| Transcendent (7) | 25 | 25 | 125 |
| Celestial (8) | 32 | 32 | 160 |
| Ascendant (9) | 40 | 40 | 200 |
| Cosmic (10) | 50 | 50 | 250 |

Treasure equipment has twelve gem slots. The primary Hell 3-3 benchmark uses four Attack, four Defense, and four HP Treasure gems, adding +80 Attack, +80 Defense, and +400 HP under the approved socket accounting. Do not add this package again to the reference stats above.

Level-100 characters may equip item-level 101-135 equipment. Idle equipment drops between the previous stage level plus one and the current stage level, inclusive; the first stage uses 1-5 and the last band is 131-135.

## 6. Player-card semantic overrides

This section lists approved overrides and high-risk semantics. Unlisted fields retain the baseline CardId definition and are still transformed by sections 3-5.

### 6.1 Hero generic cards

| Card | Approved semantics |
|---|---|
| `QingFengYiShi` | 100% Attack; the next active card costs 1 less. |
| `HeYuZhan` | 160% Attack; trigger the target's highest Bleed/Poison/Burn reservoir once. |
| `FengShenBu` | Cost 0, Exhaust; Agility 2; draw 2/3/4 by quality, then discard 1. |
| `SuiYanJi` | 150% Attack, Vulnerability 3, Mark 1. |
| `GuiYuanShu` | Healing coefficient 15; ally target clears all Bleed, Poison, and Burn; that target's next active card costs 1 less. |
| `HengJianShouShi` | Cost 1; primary 80%-Defense Armor, Mark 2, Block 1. |
| `NingShenTuNa` | Cost 0, Exhaust; Momentum 2 and Mana 10. |
| `GuanXi` | Cost 0, Exhaust; draw 3, discard 1. |
| `PoYunYiShan` | 160% Attack plus the retained 100% Agility-consumption branch and draw. |
| `XingQiHuiHuan` | Cost 0, Exhaust; draw 2/3/4 by quality and gain 1 Energy. |
| `JianYiGuanHong` | Keep the Momentum formula; quality scales only the 260% base portion. |
| `GuiYuanFanZhao` | Group healing coefficient 15; group secondary Armor 50%; clear all Bleed/Poison/Burn; draw 2. |

### 6.2 Hero profession cards

#### Blade

- `TongFengYinShi`: cost becomes 1; draw 1; grant Momentum 2/3/4 by quality. Charge replays the next card's base effect. Finish replays this source card's base effect after the first active card next player round.
- `XueLuXiangCheng`: 150% Attack and Bleed coefficient 6; retain the approved Charge/Finish structure.
- `YingFengHuanBu`: Mark 2, Agility 2, Counter 1; Charge gives the next card Agility 2 and Counter 1; Finish grants Mark 2 and Counter 1 next round.
- `TongPaoJuShi`: grant Momentum 2; the selected target's next attack gains 10 percentage points per consumed Momentum. Charge grants 2 Momentum. Finish discounts the target's next card by 1 rather than making it free. Ownership follows the selected target, not the source by accident.

#### Guard

- `TieBiTongShou`: cost-1 primary Armor (80% Defense) and Block 2.
- `JieJiaHuanFeng`: quality-scaled 100% Attack plus current Armor; secondary Armor 40%; Block 1.
- `LieZhenChengFeng`: every ally receives the complete cost-2 Armor value (140% caster Defense); Block 1 each.
- `XuanJiaZhenYue`: consume the selected ally's complete Armor; deal quality-scaled base 200% AoE plus one attack-percentage point per consumed Armor.

#### Healer and four Hero formulas

- `YiXueCuiFang`: cost 0; all allies lose 1 nonlethal HP; generate Medicine 2/3/4 per affected ally by quality; draw 1.
- `HuiChunNiMai`: ally heal or enemy reversal coefficient 25; ally-only full DOT cleanse.
- `DuHuoTongLu`: 130% Attack, Poison coefficient 6, Burn coefficient 2, toxic explosion, Medicine 6.
- `BaiCaoJiZhen`: group healing coefficient 20; all enemies receive Poison coefficient 1 and Burn coefficient 1.

The first play of each Hero formula costs one additional Energy and opens that formula for the battle:

| Formula source | Persistent formula |
|---|---|
| `YiXueCuiFang` | The first actual HP loss of each ally each round grants Hero Medicine 1, maximum three grants per round. |
| `HuiChunNiMai` | The first Hero heal/reversal each round that consumes at least six Medicine draws 1. |
| `DuHuoTongLu` | An explosion that resolves at least two DOT types grants Medicine 2, maximum two triggers per round. |
| `BaiCaoJiZhen` | The first action each round that actually heals at least two allies grants shared Energy 1. |

Formula-produced effects cannot recursively satisfy the same formula.

#### Hunter

- `FengYanDingXian`: cost 0/Mana 3; draw 2, discard 1, Agility 2, Charge 3; quality changes Mana cost to 3/2/1.
- `LieYuLianShi`: 140% Attack, Bleed coefficient 8; Heavy Arrow adds quality-scaled 50% Attack per consumed Charge.
- `CuiDuChuanXin`: 130% Attack, Poison coefficient 6, toxic explosion; each consumed Charge triggers another explosion.
- `HuiFengLianShi`: 150% Attack plus quality-scaled 40 percentage points per Charge; retain draw/Charge behavior and refund 1 Energy at three consumed Charge.

#### Sorcerer

- `YanXu`: cost 1/Mana 3; 100% AoE, Burn coefficient 4, then search.
- `HanXu`: cost 0; secondary Armor 40%; gain Mana 6; Mana overflow converts to Armor at 100%.
- `LeiXu`: cost 1/Mana 3; Mark 3; lightning deals quality-scaled 50% Attack per Mark; then search.
- `GuiXu`: cost 0; draw 2, discard 1; quality grants Mana 0/2/4.

The Hero Sorcerer task requires the four distinct Hero Sorcerer cards, not eight cards. At most one completion occurs per player round. Completion replays the four base effects and grants the matching starter reward:

| Starter | Reward |
|---|---|
| Fire | Burn coefficient 6 and trigger Burn once. |
| Ice | Consume Armor; quality-scaled base 100% AoE plus one point per Armor. |
| Lightning | Mark 3 and quality-scaled 60% Attack per Mark. |
| Universal | Draw 2, Energy 1, and the next Hero card costs 1 less. |

The reward locks and uses the starter card's quality. Exhausted Sorcerer cards still count as played for the task; the task must never deadlock by searching only live draw/discard zones.

#### Formation Master

- `GuanShiLuoZi`: cost 1/Mana 3; 80% single-target Attack, explicitly trigger the current terrain benefit once, draw 1.
- `YiZhen`: cost 1/Mana 3; trigger the current terrain benefit once, or twice when terrain was actually changed this player round; no Energy refund.
- `LianYing`: cost 1; the next terrain-benefit trigger count is 2/3/4 by quality.
- `LiuHe`: cost 2/Mana 6; trigger each of the six terrain benefits once; do not add an extra current-terrain trigger.

### 6.3 Permanent-partner cards

All 108 existing permanent-partner CardIds remain. Their existing identity, cost, target, Charge/Finish/formula/task conditions remain unless overridden below.

#### Partner Blade

- Global Blood Edge conversion is +2 attack-percentage points per resolved DOT point.
- `Profession.Blade.YinXueDao` Finish healing is capped at coefficient 20 (level-100 Rare 120, Epic 140).
- `Profession.Blade.PoJun` extra hit is 60% Rare / 70% Epic.
- `Profession.Blade.ZhanJin` raw base becomes 300% (Epic 420%).
- `Profession.Blade.HengYunKaiFeng` raw base becomes 100% (Rare 120%).
- `Profession.Blade.LianXiGuiQiao` grants Mana 4 Rare / 6 Epic.
- `Profession.Blade.BaoDaoShouYe` grants Agility 2, not 4.
- Other partner-Blade semantics remain as in the baseline catalog, transformed by the shared rules.

#### Partner Guard

- Convert every flat primary Armor grant to the printed-cost Defense formula.
- `Profession.Guard.ZhenDun` secondary Armor is 40% Defense.
- `Profession.Guard.ZhenYueLing`: consume Armor; quality-scaled base 180% plus one point per Armor; group secondary Armor 50%; Block retained.
- `Profession.Guard.BiLeiFanGong`: consume Armor; quality-scaled base 220% plus one point per Armor; secondary Armor 50%.
- `Profession.Guard.PanShiTuNa` condition becomes `Armor > 0`.
- `Profession.Guard.YuanJunBiLei`: target primary Armor 80%; owner secondary Armor 40%.
- `Profession.Guard.DunZhenTuiJin`: group secondary Armor 40%.
- `Profession.Guard.YiFuDangGuan`: one complete Epic cost-3 Armor grant to each ally, with no duplicate owner grant.
- Existing Block counts remain.

At Defense 358, reference Armor values are: cost 0 = 144, cost 1 = 287, cost 2 = 502, cost 3 = 716; Rare cost 1 = 344, Rare cost 2 = 602; Epic cost 2 = 702, Epic cost 3 = 1003.

#### Partner Healer

| Card suffix | Approved base output and formula note |
|---|---|
| `Profession.Healer.YaoYin` | Ally coefficient 30; enemy branch group coefficient 15; any HP packet formula grants Medicine 1. |
| `Profession.Healer.XingQiZhen` | Recurring formula heal coefficient 10. |
| `Profession.Healer.CaoMuFuZhi` | Coefficient 25. |
| `Profession.Healer.QingXinSan` | Coefficient 20; ally clears all four DOT reservoirs; formula grants Medicine 1 per cleared type, max 3/round. |
| `Profession.Healer.LingZhiXuMing` | Rare coefficient 40 plus low-HP coefficient 10; low-HP formula grants Medicine 3. |
| `Profession.Healer.HuiChunLu` | Rare group coefficient 25; every three effective heals draws 1, max 2/round. |
| `Profession.Healer.ZhiXueCao` | Cost 0, coefficient 10, clear all Bleed; first successful clear formula grants group Armor equal to 20% Healer Defense. |
| `Profession.Healer.WenYangGao` | Rare coefficient 25 plus ally secondary Armor 30%; threshold formula coefficient 20 grants ally Armor 20% or enemy Vulnerability 1. |
| `Profession.Healer.JinChuangXuMing` | Rare coefficient 45 and Agility 1; below-30% formula grants Agility 2. |
| `Profession.Healer.YaoWangGuiYuan` | Epic group coefficient 30; clear all Bleed/Poison/Burn; Mana 3; three units changing HP grants draw 1 and team Mana 2. |
| `Profession.Healer.BaiCaoDu` | Poison base coefficient 6 (level-100 Common display 30); each Poison packet grants Medicine 1. |
| `Profession.Healer.FuGuSan` | Base Attack 60% (Rare 72%); Bleed/Poison base coefficients 6/4 (level-100 Rare displays 36/24); Vulnerability 1; dual-status formula marks. |
| `Profession.Healer.HuiQiXiang` | AoE Poison base coefficient 1 (level-100 display 5); group Poison formula grants Medicine 2 and draw 1. |
| `Profession.Healer.LianQiaoJieDu` | Poison base coefficient 5 (level-100 display 25) and explosion; dual explosion grants Medicine 2. |
| `Profession.Healer.YaoJiuWenShen` | 70% Attack and Bleed base coefficient 8 (level-100 display 40); every two Bleed packets grants Medicine 1. |
| `Profession.Healer.YaoNangFeiTou` | Base AoE 45% (Epic 63%); Bleed/Poison base coefficients 3/1 (level-100 Epic displays 21/7); group hit grants Energy 1. |
| `Profession.Healer.KuShenMaSan` | Poison base coefficient 8 (level-100 Rare display 48), Vulnerability 2; poisoned Vulnerability grants Medicine 1 and draw 1. |
| `Profession.Healer.WuWeiTiaoHe` | AoE Poison base coefficient 1 (level-100 Epic display 7) and two explosions; triple-DOT condition grants Momentum 1 and draw 1. |

Every partner-Healer formula opens on first play at +1 Energy and is owned only by that partner.

#### Partner Hunter

Defense-ignore values use `ceil(ignore coefficient * quality * level factor)`. At level 100, coefficient 6 resolves to 30/36/42 and coefficient 2 resolves to 10/12/14.

- Direct Heavy-Arrow coefficients quality-scale; counts do not.
- `Profession.Hunter.YingYan`: remove immediate Energy refund; draw 2, Agility 2, Charge 3; Epic draws 3.
- `Profession.Hunter.LieWang`: Mana cost 3/2/1.
- `Profession.Hunter.ChuanYang`: Rare 180%, ignore 36; +60 points and +12 ignore per Charge.
- `Profession.Hunter.LianZhuJian`: Bleed/Poison base coefficients 8/6 (level-100 Rare displays 48/36); base Attack/Heavy 50% (Rare 60%); gain Charge 1.
- `Profession.Hunter.FuZuShi`: include its approved hidden +20 primary points per Charge in displayed output.
- `Profession.Hunter.YinZong`: Rare Agility 2, Charge 1, next perfect dodge +2; Epic starts with Charge 2.
- `Profession.Hunter.DuanMaiShi`: base 100% (Rare 120%), Bleed base coefficient 8 (level-100 Rare display 48), +30 base points (+36 Rare) and a Bleed trigger per Charge.
- `Profession.Hunter.ShouHun`: Epic 210%, +20 per Mark, +49 per Charge, Mark per Charge.
- `Profession.Hunter.BaiBuChuanYang`: Epic 294%, +56 per Charge, then draw.
- `Profession.Hunter.LueYingJian`: Agility 1 per two Charge, maximum 2, not one per Charge.
- `Profession.Hunter.LieHunBiao`: Mana 4/3/2.
- `Profession.Hunter.PoJiaDing`: base 75% (Rare 90%), Vulnerability 2, Poison base coefficient 1 (level-100 Rare display 6), +25 base points (+30 Rare) and level-100 ignore 12 per Charge.
- `Profession.Hunter.HuiHuanJian`: remove base Energy refund; retain draw/Charge/Mana behavior.
- `Profession.Hunter.FuYeXianJing`: Poison base coefficient 8 (level-100 Rare display 48), Mark 2, Charge 2.
- `Profession.Hunter.YingLuo`: Epic 280%, +100 below the HP threshold, +84 per Charge, threshold rises five points per Charge.

#### Partner Sorcerer

The task requires five distinct partner Sorcerer cards and completes at most once per player round. Keep the approved Fire/Ice/Lightning/Universal task identities, with these numeric corrections:

- Fire/BaoYan DOT conversion is +2 points per resolved Burn.
- Standard Ice conversion uses quality-scaled base 100% plus one attack-percentage point per consumed Armor. Its base is therefore 100%/120%/140% at Common/Rare/Epic; consuming 300 Armor gives 400%/420%/440% Attack. This applies to all rewards explicitly using standard Ice, including 照见/六合/斗转. 万法归一 retains its separately authored 220% base and Hero 寒序 its 100% base.

The user confirmed the 10% current-Mana recovery version after reviewing the damage comparisons and step tables. This is now the authoritative rate for all four partner Ice cards, superseding the previous 25%/20% drafts and earlier Defense-derived base grants. Recovery rounds upward; base card quality does not multiply the recovery percentage.

| CardId / name | Base quality / cost | Base effect, in order | Starter reward after the standard Ice attack |
|---|---|---|---|
| `Profession.Sorcerer.SheLingHuo` / 寒息回流 | Rare; 1 Energy / 0 Mana | Recover `ceil(current Mana * 10%)`; convert actual overflow by section 4.3.1. | Gain Energy 1; draw 1. |
| `Profession.Sorcerer.FenMaiFu` / 玄冰拓脉 | Common; 0 Energy / 0 Mana | Increase this battle's MaxMana by 4 without changing current Mana; then recover `ceil(current Mana * 10%)` and convert actual overflow. | Increase this battle's MaxMana by another 8 and fill current Mana to that new maximum. |
| `Profession.Sorcerer.LingYanLianDan` / 霜镜叠甲 | Rare; 0 Energy / 0 Mana | If current Armor is zero, recover `ceil(current Mana * 10%)` and convert actual overflow. Otherwise double current Armor, without recovering Mana or applying another multiplier. | Every unique living ally, including the caster, receives `floor(ArmorConsumedByThisIceBlast / 4)` Armor. |
| `Profession.Sorcerer.HuLingMu` / 冰鉴索法 | Rare; 1 Energy / 0 Mana | Recover `ceil(current Mana * 10%)` and convert actual overflow; search for one unfinished carried Sorcerer card; if no legal search exists, grant one more copy of the Armor generated by this recovery. | All enemies gain Weak 2. |

Operational interpretation for this review: all four Ice cards' 10%-Mana recoveries use upward rounding. Ice Search's "another equal Armor grant" copies this execution's **resolved overflow Armor**, not the owner's total Armor; it neither recovers Mana a second time nor rescales the copy. If the recovery produces no overflow Armor, the extra grant is zero. Search candidates remain unfinished carried cards in Draw/Discard, so cards already in Hand do not qualify. The latest standard Ice base is 100% times starter quality. Costs and the four starter reward identities follow the current table.

The latest user amendment replaces 霜镜's former 40%-Defense group reward with one quarter of the Armor consumed by this same Ice blast. Capture that Armor once before consuming it, resolve the standard Ice attack, then grant the full quarter independently to every living ally including the caster. Use the existing quarter-Armor refund convention (round downward); do not multiply the grant by quality, level, Defense, damage dealt, or number of enemies. Do not divide the grant among allies. Consuming 1003 Armor grants 250 to each living ally; consuming zero grants zero. This reward occurs only when 霜镜 was the task starter, not on every ordinary or automatic play of 霜镜.

At TeamMaxLevel 100, independently begin each card at Mana 34/34 and Armor 0: Rare 寒息 recovers 4 and generates Armor 24; Common 玄冰 raises MaxMana to 38, recovers 4, ends Mana 38, and generates zero Armor because the enlarged pool is exactly filled; Rare 霜镜 at zero Armor recovers 4 and generates Armor 24; Rare 冰鉴 recovers 4 and generates Armor 24, or 48 total when search fails. From Mana 31/34, Rare 冰鉴 recovers 4, generates Armor 6, and a failed search adds only another 6. From Mana 30/34 it recovers 3, ends at 33/34, and generates no Armor. Zero overflow is a valid effect result and must not fail the card or its task progress. From Mana 42/42, 玄冰 raises the cap to 46, recovers 5, and converts one overflow into 5/6/7 Armor by quality. Defense changes must not alter these results or a fixed consumed-Armor refund.

At the confirmed 10% rate, the same-round minimum-quality sequence 照见→寒息→玄冰→冰鉴→霜镜 starts at Mana 34/34 and zero Armor. With the first Universal automatic-hand budget available, Ice Search cannot find an unfinished Draw/Discard target on either pass: Armor is 144 after active cards and 456 before the reward; Common 照见 refunds 114 Armor, and its potential Ice damage is 1694 against level-135 Defense 146 at Attack 495. The equivalent 斗转→寒息→玄冰→冰鉴→霜镜 sequence pays 2 Mana on its starter and performs the extra mirror replay: pre-reward Armor 816, potential Ice damage 2981. With 霜镜→寒息→玄冰→冰鉴→照见, pre-reward Armor is 351, Rare Ice is 471% Attack / potential damage 1421, and each living ally receives 87 Armor afterward. These are controlled semantic fixtures, not measured win rates; all earlier 25%/20% numbers are historical.

- `Profession.Sorcerer.ChiXiaoFenXing`: Rare AoE 60%, Mark 2.
- `Profession.Sorcerer.FenTianJue`: Epic 98%.
- `Profession.Sorcerer.NingYanChengRen`: base 55% per Mark; recorded positions 4-5 use 70% per Mark, then apply quality once. Its starter reward adds Mark 5 and performs a fixed five 70%-base hits.
- `Profession.Sorcerer.RanLingHuanYuan` / 雷走八方: the user has confirmed one direct hit per living enemy, with ordinary at-most-one-Mark consumption and the other Marks retained. Common / 0 Energy / 4 Mana remains. Zero Mark still yields a base hit. This replaces its old all-Mark 45%/60% volley identity. The current **numeric candidate**, not a frozen value, is 120% unmarked / 180% marked / 220% marked at recorded positions 3-4; candidate starter reward adds Mark 3 and deals one 240%-base hit. Quality scales the selected attack coefficient once. See `docs/superpowers/specs/2026-09-03-lightning-single-hit-design.md` for approved direction, candidate boundaries, and full-task projections. The separate proposed 引雷易伤 reward has not been approved and is excluded.
- `Profession.Sorcerer.YanMuHuTi` Ice reward: base 220% plus Armor conversion.
- `Profession.Sorcerer.XingHuoHuiShou` / 六合护法: Rare, 0 Energy / 4 Mana. Restore 8 current Mana to the caster only; every unique living ally, including the caster, receives the Armor converted from **this recovery's actual overflow**. If the previous recorded card in this task contains no direct damage, replace recovery 8 with 16. This is current-Mana recovery, not MaxMana growth. The fixed 8/16 is not quality-, level-, or Defense-scaled; only overflow Armor uses section 4.3.1 once.

六合's composite base effect applies before a Universal starter has selected its task branch and also outside the Ice branch. Pay the card's actual Mana cost first, recover the caster's Mana, snapshot the resulting overflow once, resolve one Armor amount using the card's quality and TeamMaxLevel, and grant that full amount independently to every living ally. Other allies' Mana and MaxMana do not change. Give the caster one grant, not an extra self-only grant from the generic Ice overflow path. A first recorded card has no predecessor and uses 8. Replays use the recorded position/predecessor condition but do not pay Mana, so their overflow may differ; 16 replaces 8 rather than being added to it. If the previous card contains a direct attack that misses or deals no HP damage, it still does not satisfy the non-damage condition.

六合's **Ice starter reward** is now standard Ice, then `floor(ArmorConsumedByThisIceBlast / 4)` Armor to every unique living ally including the caster. Use the consumed-Armor snapshot, with no additional Defense term or quality/level scaling. Already-held recipient Armor is preserved and the grant is added normally. The separately authored non-Ice reward branches remain recorded as Normal group 80%-Defense Armor plus Weak 2, Fire group 60%-Defense Armor plus Burn coefficient 4 and Weak 1, and Lightning Mark 2 / 30% per-Mark lightning / group 40%-Defense Armor; this amendment changes the composite base/sequence effect and the Ice reward.

At TeamMaxLevel 100 and Rare quality, begin an active 六合 at Mana 34/34: pay 4, recover 8, and generate 24 Armor for each living ally. Under the qualifying predecessor, recover 16 instead and generate 72 per ally. At full Mana, free replay produces 48/96 per ally for the locked 8/16 branch. With only Mana 4 before an active ordinary cast, payment leaves zero, recovery ends at 8/34, and nobody gains Armor. A cast with less than the actual 4-Mana cost is rejected before recovery. Changing ally Mana, Defense, or level must not change the grant calculated from the caster and the existing TeamMaxLevel snapshot.

The confirmed four-Ice recovery rate is 10%. At the shared level-100 / Attack-495 / initial Mana-34 benchmark, 六合→寒息→周天→冰鉴→霜镜 produces 774 Armor before its Ice reward and potential damage 2782, then grants 193 Armor per living ally. The caster ends at 193, while the other two initially unarmored allies retain 24+48 base Armor and end at 265 each. 照见→寒息→六合→冰鉴→霜镜 uses the 16-Mana branch, produces 912 pre-reward Armor / potential damage 3161, and ends with caster Armor 228 and other allies 168 each. 斗转→寒息→六合→冰鉴→霜镜 produces 1728 / 5915 and ends with caster Armor 0 and other allies 168 each. The accepted step ledger is `docs/design/2026-09-03-ice-mana-armor-confirmed-step-tables.md`; earlier revision and 20% ledgers remain historical. This numeric approval does not certify current runtime implementation or full-battle balance.

#### Partner Formation Master

The special unlock order is:

- level 1: all six terrain-switch cards plus `CunZhaiYuanZhen`, `HuiShengZhenSha`, `YiWeiZhen`, `ShanMenFengSuo`, `LinFengFuZhen`, `ZhenQiGuWu` (12 total);
- level 5: `BaMenLunZhuan`, `ShuiJingZheGuang`;
- level 10: `DiMaiJieLi`, `SiXiangLianHuan`;
- level 15: `ZhenShaZhen`, `WanXiangGuiZhen` (18 total).

All six switches cost 1, change terrain, and trigger the destination benefit once. Quality grants Mana 0/2/4.

| Card suffix | Approved semantics |
|---|---|
| `CunZhaiYuanZhen` | Rare group heal coefficient 20, group Armor 50%, terrain trigger 1. |
| `HuiShengZhenSha` | Rare 288% single target, terrain 1. |
| `YiWeiZhen` | Ally Agility 1, remove Vulnerability 1/2/3, terrain 1. |
| `BaMenLunZhuan` | Rare draw 3/discard 1/terrain 1/next trigger double; Epic draw 4. |
| `ZhenShaZhen` | Epic 448% AoE, Vulnerability 5, terrain 1. Old 1280% display is obsolete. |
| `WanXiangGuiZhen` | Epic group Armor 200%, draw 3, next terrain card free once, terrain 1. |
| `ShanMenFengSuo` | Vulnerability 2/3/4, terrain 1. |
| `ShuiJingZheGuang` | Rare target Armor 80%, clear all four DOT reservoirs, terrain 1. |
| `LinFengFuZhen` | Cost 0, ally Agility 1 (Epic 2), terrain 1. |
| `ZhenQiGuWu` | Rare team next attack +20 points (Epic +25), terrain 1. |
| `DiMaiJieLi` | Rare 240% single target, terrain 2. |
| `SiXiangLianHuan` | Epic group Armor 120%, draw 3, terrain 2. |

### 6.4 Task NPC cards

NPC Sorcerer tasks use three distinct NPC cards; Hero uses four; permanent partner uses five. The count is runtime data, never a hard-coded UI number.

#### Tusi Chief and Song Jinbao

- `Npc.TusiChief.ZhaiZhuHaoLing`: Rare cost 0; highest-Attack ally Momentum 1; Tusi secondary Armor 48%; Block 1; 120% attack; Finish group Armor 20%.
- `Npc.TusiChief.ShiMenShouShi`: target Armor 80%, Mark 2, Agility 2, Block 2; Charge secondary Armor 40% plus Block; Finish redirects.
- `Npc.TusiChief.TuSiJunLing`: Mark 2, Vulnerability 3, highest-Attack ally Armor 40% plus Block and 150% attack; retain approved branches.
- `Npc.TusiChief.MengZhaiShiYue`: Epic group Momentum 1, group Armor 70% of Tusi Defense, Block 1, each ally attacks for 84%, next card free, Retain.
- `Npc.SongJinBao.ShangQianGuWu`: cost 0; target Momentum 2, Mana 6, 100% attack; task reward group Momentum 2, draw 2, Energy 2.
- `Npc.SongJinBao.ErMuMiBao` (`耳目密报`): Rare cost 0/Mana 3; replace redundant intent reveal with Weak 1 and Mark 2, then search; reward Mark 3 and all allies attack 120%.
- `Npc.SongJinBao.GuiKeLing`: Mark 2, Agility 2, Counter 1, draw; reward Mark 3, Agility 4, Counter 3.
- `Npc.SongJinBao.YiNuoQianJin`: Epic cost 1/Mana 6; search and next two cards free; reward draw 3, Energy 2, next two cards free.

#### Yue Bai and Zhou Guangzu

- `Npc.YueBai.QingYanDianDeng`: cost 0/Mana 3; Burn coefficient 6, trigger once, search; reward is the AoE form.
- `Npc.YueBai.CanJuanPiZhu`: Rare cost 0; draw 2, terrain trigger 1, search, no target; reward terrain trigger 3.
- `Npc.YueBai.YueBaiZhaoYe`: cost 1/Mana 3; Mark 2, Burn coefficient 4, 100%, trigger Burn, search; reward AoE Mark 3 and lightning 60%.
- `Npc.YueBai.ShanHeCanTu`: Epic cost 0/Mana 6; group Armor 56% of Yue Bai Defense, Mana 5, terrain 1, search, no target; reward consumes Armor for AoE base 140% plus one point per Armor.
- `Npc.ZhouGuangZu.YiCaoBianShi`: cost 0; Medicine 6, heal/reversal coefficient 15, ally clears all Bleed/Poison/Burn.
- `Npc.ZhouGuangZu.HuangShanFuZhi`: cost 0/Mana 3; party loses 1 nonlethal HP, Medicine 6, group coefficient 15.
- `Npc.ZhouGuangZu.DiZhiMoTu`: Rare cost 0/Mana 3; draw 2, terrain trigger 2, no target.
- `Npc.ZhouGuangZu.YanFenFengMai`: Epic cost 1/Mana 3; terrain 1, Vulnerability 3, Poison coefficient 11, then explosion.

#### Jin Gui and Qiong Meier

- `Npc.JinGui.ShiJingErMu`: the correct public name is `市井耳目`; Rare cost 0/Mana 3; AoE Mark 2, draw 2; highest-Attack ally Charge 2 and Heavy 60%.
- `Npc.JinGui.QiaoYanZhouXuan`: cost 1; target Vulnerability 3; highest-Armor ally receives 80% Jin Gui Defense Armor and Block 2.
- `Npc.JinGui.ZaYiChouBei`: cost 1/Mana 3; draw 3, discard 1, refund 1; highest-Attack ally Charge 3, Heavy 40%, Mana 1.
- `Npc.JinGui.HouXiangTuoShen`: Epic cost 2/Mana 6; team Agility 2, lowest enemy Mark 2, Jin Gui Armor 196%, Block 2.
- `Npc.QiongMeiEr.TengQiaoFeiDu`: Rare cost 0/Mana 3; highest-Attack ally Agility 2, Charge 2, draw 1, Heavy 60%.
- `Npc.QiongMeiEr.GuWuMiZong`: cost 1; target Bleed coefficient 4 and Poison coefficient 6, then explosion.
- `Npc.QiongMeiEr.YinLingZhenXin`: cost 1/Mana 3; Medicine 6; ally clears all Bleed/Poison/Burn; healing coefficient 30.
- `Npc.QiongMeiEr.ShanGeHuanLing`: Epic cost 2/Mana 6; Medicine 6; group healing coefficient 25.

These NPC cards do not open partner prescriptions unless explicitly listed; ownership remains with their NPC source.

### 6.5 Boss reward cards

| Card | Approved semantics |
|---|---|
| `XiongPiPiJia` | Epic cost 2; self Armor 196% Hero Defense; first direct damage received retaliates for 70% Attack. |
| `HanDiYiShi` | Epic cost 3/Mana 10; 252% Attack, Vulnerability 5, secondary Armor 70% Hero Defense. |
| `HuPoZhenDan` | Epic cost 2/Mana 8; group Armor 196% Hero Defense; clear all four DOT reservoirs. |
| `DuKouLieFeng` | Epic cost 2/Mana 6; 196% Attack, +80 points when target is Marked. |
| `FuHuDuanJiang` | Epic cost 3/Mana 14; 322% Attack; consume up to three Vulnerability and add 25 points per consumed layer. |

## 7. Task, formula, phase, and terrain presentation

### 7.1 Sorcerer task status

When a Hero, permanent partner, or task NPC Sorcerer starts a task, create an owner-scoped status group below that character:

- the main task icon is pinned first;
- immediately beside it, show the task's distinct required card mini-thumbnails in a stable order;
- unused thumbnails are gray; playing that card lights its thumbnail;
- hovering the main icon shows task progress and reward;
- hovering a thumbnail shows its card name and whether it is complete;
- counts are built from runtime data: NPC 3, Hero 4, partner 5;
- completion, replay, and reward occur at most once per player round;
- completed thumbnails stay lit through replay/reward, then display a completed-this-round state and disappear at the next player round;
- Exhausted cards still count and completed state reconstructs exactly after load.

### 7.2 Healer formula status

Each formula uses the source card's miniature artwork as its status icon. Hero and partner formula sets remain separate even when their source-card identity is similar.

Hover text shows card name, exact persistent formula, per-round cap, current progress, and owner. The separate Medicine icon shows the owner's current numeric reservoir. Formula state, cumulative Medicine remainder, per-round budgets, and icon order survive save/load.

Task and formula icons are state presentation, not removable combat buffs. Cleanse, Snatch, and other status-removal effects cannot select or erase them.

### 7.3 Enemy phase icons

Phase icons are immutable ink marks above the enemy status row:

- Hard: `二`;
- Hell: `三 二`;
- consume `二` first, then `三`;
- the remaining mark stays visible after a transition;
- hover describes the current phase, remaining phases, and the transition rule;
- cleanse and positive-status removal never select these icons.

### 7.4 Terrain benefits

The six conceptual terrain benefits are:

| Terrain | Benefit |
|---|---|
| Plain | All enemies receive Burn coefficient 2. |
| Cliff | All enemies receive Vulnerability 2 and Mark 1. |
| Forest | All allies receive healing coefficient 10. |
| Water shore/Ferry | All allies gain Mana 3. |
| Village | Draw 1; all allies receive Armor equal to 20% of the trigger owner's Defense. |
| Cave | All allies receive Armor equal to 40% of the trigger owner's Defense and Block 1. |

Terrain payload coefficients use an implicit quality multiplier of 1.0. The quality of a card that explicitly triggers terrain changes only its printed trigger count or other card effects; it does not multiply the terrain payload again.

Only the terrain **benefit payload** is group-wide. A card's independent base target may remain single-target.

A card whose only actionable payload is changing or triggering terrain requires no manual unit target. This includes all six permanent-partner terrain switches and Hero `YiZhenHuiXiang`. Cards with an independent single-target attack/debuff may keep that base target; the terrain payload itself still resolves group-wide. The approved no-target NPC cards (`CanJuanPiZhu`, `ShanHeCanTu`, and `DiZhiMoTu`) remain no-target.

There are two trigger paths:

1. **Automatic round-start trigger.** It occurs on the first player round and every later player round only while a living deployed permanent Formation Master partner exists. Use that partner's stats. If the partner dies, automatic benefits stop.
2. **Explicit card trigger.** A card with `TriggerTerrainBenefit` always works, even without a Formation Master partner. For Armor terrain, use that card owner's stats. Printed counts remain authoritative (`DiZhi` 2, `CanJuan` reward 3, and so on).

Terrain still exists visually and for card conditions when no Formation Master is deployed, but it has no automatic income. Current Training defaults remain non-Boss Plain and Boss Cave unless a later scoped design changes the authored encounter terrain.

## 8. Settlement and Boss-card ownership

Current gameplay is single-map. Defeating the stage Boss must not offer a Boss card. It creates one unique settlement receipt, atomically grants its rewards, and opens a dedicated victory page showing:

- experience;
- equipment/chest/currency rewards;
- first-clear and unlock results;
- combat statistics;
- a single confirmation action returning to the desktop workbench.

Saving or restarting while the page is pending reopens the same receipt and never grants it twice. The current direct return from Boss victory is obsolete.

Future multi-map mode uses a non-final-map Boss choice containing exactly one Boss card, one carried-card upgrade, and one relic; choose one. Boss-card slots are run-local, start empty, persist between maps of that run, and clear at final settlement. If Boss-card slots are full, replace the Boss-card option with a relic. A final-map Boss goes directly to final settlement.

## 9. Training progression and enemy roster

There are 27 stages. Each step raises enemy combat level by five:

| Difficulty | Stage levels | Phase rule |
|---|---|---|
| Normal | 5, 10, 15, 20, 25, 30, 35, 40, 45 | No extra phase. |
| Hard | 50, 55, 60, 65, 70, 75, 80, 85, 90 | Every Elite and Boss has two total phases. |
| Hell | 95, 100, 105, 110, 115, 120, 125, 130, 135 | Every Elite and Boss has three total phases. |

Use all 21 enemies:

| Chapter | Ordinary | Elite | Boss |
|---|---|---|---|
| 1 | Rooster, Goat, Weasel, Civet | Ironfeather Rooster, Bluehorn Goat King | Money Rat |
| 2 | Gray Wolf, Boar, Macaque, Porcupine | Graymane Wolf King, Redtusk Boar King | Black Bear |
| 3 | Venom Snake, Wildcat, Vulture, Giant Toad | White Ape, Spiral-Horn Deer | Tiger |

The nine stage-end cores, repeated across the three difficulties, are:

| Chapter | Stage 1 | Stage 2 | Stage 3 |
|---|---|---|---|
| 1 | Ironfeather | Bluehorn | Money Rat |
| 2 | Graymane | Redtusk | Black Bear |
| 3 | White Ape | Spiral-Horn Deer | Tiger |

Level-135 phase-unit stat checkpoints from the retained catalog curves are:

| Enemy | HP | Attack | Defense |
|---|---:|---:|---:|
| Ironfeather | 2128 | 242 | 72 |
| Bluehorn | 2416 | 227 | 94 |
| Money Rat | 3456 | 285 | 109 |
| Graymane | 2570 | 286 | 80 |
| Redtusk | 2868 | 272 | 110 |
| Black Bear | 4340 | 345 | 132 |
| White Ape | 3012 | 316 | 95 |
| Spiral-Horn Deer | 3158 | 301 | 110 |
| Tiger | 4936 | 390 | 146 |

With three total phases on White Ape, Tiger, and Deer, Hell 3-3 has 33,318 raw phase HP before Armor and healing. This is intentional and is balanced around simultaneous AoE damage and the ten-round benchmark rather than pre-nerfing phase counts.

### 9.1 Difficulty status values

The table gives single-target/group base DOT coefficients or discrete status layers:

| Status | Normal single/group | Hard single/group | Hell single/group |
|---|---:|---:|---:|
| Weak | 2 / 1 | 3 / 2 | 4 / 3 |
| Mark | 2 / 1 | 3 / 2 | 5 / 3 |
| Bleed coefficient | 3 / 1 | 5 / 3 | 8 / 5 |
| Poison coefficient | 3 / 2 | 6 / 4 | 9 / 6 |
| Burn coefficient | 2 / 1 | 4 / 2 | 6 / 3 |

Monster DOT coefficients use the same TeamMaxLevel conversion as player cards. At TeamMaxLevel 100, coefficients 1/2/3/4/5/6/8/9 resolve to 5/10/15/20/25/30/40/45.

Keep the approved discrete caps: Vulnerability 5, Mark 5, Weak 5, Wealth 8, Rage 5, Counter 8, and Block 8. DOT reservoirs use their level cap instead.

## 10. Ordinary monster intents

Values in this section are `Normal / Hard / Hell`. Ordinary monsters always use one phase and a three-intent deterministic loop.

### 10.1 Chapter 1

| Enemy / intent | Approved effect |
|---|---|
| Rooster - Peck | 150% / 230% / 310% Attack to lowest HP. |
| Rooster - Double Peck | 100%x2 / 175%x2 / 240%x2 to Marked first. |
| Rooster - Crow | Remaining not-yet-acted enemies gain 20% / 35% / 50% of Rooster current Attack until enemy phase end. |
| Goat - Horn | 140% / 210% / 280%, Weak 2/3/4, Marked first. |
| Goat - Stomp | Self Armor 200% / 280% / 360% of Goat Defense. |
| Goat - Charge | Charge one round; 240% / 340% / 450% to the locked target. |
| Weasel - Harass | 130% / 200% / 270%, then Mark 2/3/5. |
| Weasel - Stink Fog | 80% / 140% / 200% AoE, Weak 1/2/3. |
| Weasel - Escape | Self Armor 200% / 280% / 360% of Defense and Agility 1. |
| Civet - Claw | 150% / 225% / 300% to lowest HP. |
| Civet - Feint | 100% / 165% / 230%, then Mark 2/3/5. |
| Civet - Pickpocket | 120% / 185% / 250%; next-round shared Energy -1/-2/-2; self Armor 120%/180%/240% Defense. |

### 10.2 Chapter 2

| Enemy / intent | Approved effect |
|---|---|
| Gray Wolf - Bite | 160% / 230% / 300%, then Mark 2/3/5. |
| Gray Wolf - Pursuit | Base 140%/200%/260%; if target is Marked use 220%/310%/400%. |
| Gray Wolf - Call Pack | Remaining enemies gain 20%/35%/50% of Gray Wolf current Attack until enemy phase end. |
| Boar - Tusk | 180% / 250% / 330%. |
| Boar - Bristle | Self Armor 200% / 280% / 360% Defense. |
| Boar - Armor-Break Charge | Charge one round; 240%/340%/450%, Weak 2/3/4, Marked first. |
| Macaque - Throw Stone | Stable-random target takes 160%/230%/300%. |
| Macaque - Snatch | Lowest HP takes 100%/160%/220%; remove 1/1/2 positive-status layers, with the exact removal previewed. |
| Macaque - Hasten | All enemies gain Armor equal to 150%/220%/300% Macaque Defense; Macaque gains Agility 1. |
| Porcupine - Quill | 160%/230%/300%, Bleed coefficient 3/5/8, Marked first. |
| Porcupine - Bristle Guard | Self Armor 200%/280%/360% Defense; gain Block 1/2/3. |
| Porcupine - Quill Volley | 100%/155%/210% AoE; Bleed coefficient 1/3/5. |

Porcupine uses visible Block from Bristle Guard; it has no hidden automatic counterattack. Enemy Block mirrors player Block and uses the Porcupine's post-card Armor.

### 10.3 Chapter 3

| Enemy / intent | Approved effect |
|---|---|
| Venom Snake - Venom Bite | 150%/215%/280%, Poison coefficient 3/6/9, Marked first. |
| Venom Snake - Coil | Self Armor 200%/280%/360% Defense and Agility 1. |
| Venom Snake - Toxic Pursuit | Base 160%/230%/300%; if target is Poisoned use 220%/310%/400% and trigger Poison once. |
| Wildcat - Rake | 160%/230%/300%, Bleed coefficient 3/5/8, Marked first. |
| Wildcat - Stalk | Lowest HP receives Mark 2/3/5; Wildcat gains Agility 1/1/2. |
| Wildcat - Blood Pursuit | Base 160%/230%/300%; if target is Bleeding use 220%/310%/400% and trigger Bleed once. |
| Vulture - Gaze | Lowest HP receives Mark 2/3/5 and Burn coefficient 2/4/6. |
| Vulture - Dive | Marked first; 180%/260%/340%; trigger Burn once when present. |
| Vulture - Wing Cut | 110%/160%/210% AoE; Burn coefficient 1/2/3. |
| Giant Toad - Tongue | Base 160%/230%/300%; if Poisoned use 220%/310%/400%; then heal 4%/5%/6% max HP. |
| Giant Toad - Poison Fog | 90%/140%/190% AoE; Poison coefficient 2/4/6. |
| Giant Toad - Inflate | Self Armor 200%/280%/360% Defense; refresh one healing amplification adding 4%/5%/6% max HP to the next Tongue. |

Giant Toad healing amplification is not Medicine. It stores at most one use, refreshes rather than stacks, and Tongue consumes it even when the heal overheals.

Burn-capable enemies are intentionally limited to Ironfeather, Redtusk, and Vulture.

## 11. Elite and Boss phase-one intents

Values are `Normal / Hard / Hell` unless stated otherwise.

### 11.1 Chapter 1

**Ironfeather passive:** the first player direct-attack card in each phase that deals positive HP damage has its HP damage halved. Armor-nullified, Agility-avoided, and DOT packets do not consume it. Rearm on each phase transition.

| Intent | Approved effect |
|---|---|
| Rapid Peck | 80%x3 / 115%x3 / 150%x3, Marked first. |
| Iron Guard | Self Armor 160%/220%/280% Defense. |
| Battle Cry | Remaining enemies gain 20%/35%/50% of Ironfeather current Attack until enemy phase end. |
| Blood Fight | Only below or equal to 50% of the current phase HP; 160%/190%/210%, Burn coefficient 2/4/6. |

**Bluehorn passive:** retain 50% of remaining Armor at the next own-phase boundary in phase one, 75% in phase two, and 100% in phase three.

| Intent | Approved effect |
|---|---|
| Pierce | 120%/150%/180%, Weak 2/3/4, Marked first. |
| Herd Stomp | 80%/110%/140% AoE. |
| Guard Herd | All enemies gain Armor equal to 80%/100%/120% Bluehorn Defense. |
| Rage Charge | Charge one round; 210%/240%/270%. |

**Money Rat phase-one passive:** before each player round gain Wealth 1/1/2. Wealth cap is 8 and persists across phase transitions.

| Intent | Approved effect |
|---|---|
| Coin Volley | 80%/100%/120% AoE. |
| Hoard | Self Armor 160%/200%/240% Defense; Wealth 2/2/3. |
| Greedy Mark | Stable-random party target gets Mark 2/3/5. |
| Pickpocket | 100%/125%/150%; next-round shared Energy -1/-2/-2; Wealth 1. |
| Break Wealth | Consume up to 3 Wealth; heal 3%/4%/4% max HP per consumed Wealth. |
| Coin Crash | Base 110%/125%/140%; +10/+12/+15 points per Wealth, counting at most 4 without consuming. |

### 11.2 Chapter 2

**Graymane passive:** its single-target intent coefficient is multiplied by 1.2 against a Marked target; normal Mark amplification also applies and is previewed.

| Intent | Approved effect |
|---|---|
| Hunt Mark | 110%/130%/150%, then Mark 2/3/5. |
| Continuous Hunt | Requires a Marked target; 80%x3/105%x3/130%x3. |
| Pack Order | Remaining enemies gain 20%/35%/50% of Graymane current Attack; stable-random target gets Mark 2/3/5. |
| Sidestep | Self Armor 160%/220%/280% Defense; Agility 1/1/2. |

**Redtusk passive:** each player active card that deals any HP damage to Redtusk grants Rage 1, once per card rather than per hit. DOT and reactions do not grant Rage. Cap 5; preserve across phases.

| Intent | Approved effect |
|---|---|
| Heavy Armor | Self Armor 200%/280%/360% Defense. |
| Earthquake | 80%/100%/120% AoE, Weak 1/2/3. |
| Rage Strike | Base 150%/180%/210%; +20 points per Rage, do not consume. |
| Red Charge | Charge one round; 210%/250%/290%. |

**Black Bear passive:** after Defense and Armor, reduce remaining HP damage from player direct attacks by 15%. DOT and non-direct damage bypass Thick Hide. Keep 15% in all phases.

| Intent | Approved effect |
|---|---|
| Sweep | 90%/110%/130% AoE. |
| Pounce | 170%/200%/230%, Marked first. |
| Weak Roar | Weak 1/2/3 to all party members. |
| Rend | 150%/180%/210%, Bleed coefficient 3/5/8, Marked first. |
| Counter Posture | Counter 1/2/3. |
| Quake | 110%/135%/160% AoE, Weak 1/2/3. |

The old phase-two Black Bear Attack 130%, Defense 75%, and extra Pounce/Rend hits are retired.

### 11.3 Chapter 3

**White Ape phase-one passive:** the first negative status actually added to White Ape each player round grants self Armor equal to 80%/100%/120% White Ape Defense. Phase two changes this to 100% White Ape Defense for each enemy's first received negative status; phase three uses 160%. White Ape must be alive.

| Intent | Approved effect |
|---|---|
| Throw Rock | 120%/140%/160%, Prey first then lowest HP. |
| Disturb | Queue one +1 Energy surcharge on the player's next active card; refresh, never stack. |
| Boulder Charge | Charge one round; 220%/245%/260%, Prey first. |
| Wide Sweep | 85%/100%/115% AoE. |

| Spiral-Horn Deer intent | Approved effect |
|---|---|
| Horn | 110%/125%/140%, Prey first then lowest HP. |
| Terrain Blessing | Every enemy's next direct attack gains +20/+30/+40 points; one use per enemy, refresh rather than stack. |
| Herd Armor | All enemies gain Armor equal to 80%/100%/120% Deer Defense. |
| Spring Heal | Lowest-HP enemy heals 6%/8%/10% max HP; cooldown 2 enemy rounds. |

**Tiger phase-one rules:** Mark Prey selects a stable-random party unit. Phase-one Tiger Pounce clears Prey after resolving. If Tiger changes phase first, preserve the current Prey and use persistent phase-two rules. One Tiger card that deals HP damage to a Bleeding target heals 8% of Tiger's missing HP at most once per card.

| Intent | Approved effect |
|---|---|
| Mark Prey | Assign Prey 1 to a stable-random party unit. |
| Tiger Pounce | 150%/170%/190% to Prey; clear phase-one Prey afterward. |
| Tail Sweep | 95%/110%/125% AoE. |
| Bleeding Rend | 120%/150%/180%, Bleed coefficient 3/5/8, Marked first. |
| Dread Roar | Weak 1/2/3 to all party members. |
| Ambush | Charge one round; 240%/270%/300% to the locked target. |

## 12. Phase-two and phase-three decks

Entering a phase replaces the previous deck and resets that enemy's intent cursor to the first listed card. Phase two exists in Hard and Hell. Values written `Hard/Hell` are difficulty-specific status layers or DOT coefficients. Phase three is Hell-only.

### 12.1 Chapter 1

#### Ironfeather

Phase two, **Ironfeather Burning Formation**:

| Order | Card | Effect |
|---:|---|---|
| 1 | Ironfeather Burning Formation | 80% AoE; Burn coefficient 2/3 to all; lowest HP gets Mark 3/5. |
| 2 | Chase-Fire Rapid Peck | 160%x3 to Marked first; trigger target Burn once afterward. |
| 3 | Ironfeather Guard the Flock | All enemies gain Armor equal to 100% Ironfeather Defense; Ironfeather gains Agility 1. |
| 4 | Blood-Feather Pounce | 220% to Marked first; Burn coefficient 4/6. |

Phase three, **Blood Feather Unextinguished**:

| Order | Card | Effect |
|---:|---|---|
| 1 | Blood Feather Burns the Sky | 105% AoE; Burn coefficient 3 to all; lowest HP gets Mark 5. |
| 2 | Life-Bound Rapid Peck | 180%x4 to Marked first; trigger Burn once afterward. |
| 3 | Unfallen Ironfeather | All enemies gain Armor equal to 150% Ironfeather Defense; Ironfeather gains Agility 1 and rearms its first-hit reduction if spent. |
| 4 | Death-Burning Beak | 260% to Marked first; add Burn coefficient 6 and immediately trigger Burn once. |

#### Bluehorn Goat King

Phase two, **Bluehorn Suppresses the Mountains**:

| Order | Card | Effect |
|---:|---|---|
| 1 | Fire-Stepping Pierce | 190% to Marked first; Weak 3/4; trigger Burn once when present. |
| 2 | Bluehorn Suppresses the Mountains | All enemies gain Armor equal to 120% Bluehorn Defense. |
| 3 | Formation-Breaking Herd Stomp | 100% AoE; Weak 2/3. |
| 4 | King-Horn Breaks the Pass | Charge one round; 280% to Marked first, then Vulnerability 3/5. |

Phase three, **King Horn Treads the Heavenly Pass**:

| Order | Card | Effect |
|---:|---|---|
| 1 | Heavenly-Pass Pierce | 230% to Marked first; Weak 4; trigger Burn once when present. |
| 2 | Ten-Thousand Horns Guard the Mountain | All enemies gain Armor equal to 180% Bluehorn Defense. |
| 3 | Falling Peaks | 120% AoE, Weak 3, remove one positive-status layer from each party member. |
| 4 | King Horn Treads the Heavenly Pass | Charge one round; 340% to Marked first, then Vulnerability 5; trigger Burn once when present. |

#### Money Rat

Phase-two passive, **Compound Interest**: when another enemy direct-attack card deals HP damage to a Marked or Burning party unit, Money Rat gains Wealth 1, maximum two grants per enemy phase. DOT and reactions do not trigger it.

| Order | Card | Effect |
|---:|---|---|
| 1 | Close-Gate Collection | 150% to Marked first; next-round shared Energy -2; then Vulnerability 3/5; Wealth 1. |
| 2 | Hoard Behind Closed Gates | Wealth 3; all enemies gain Armor equal to 120% Money Rat Defense. |
| 3 | Compound Interest | 95% AoE plus 20 points per Wealth, count at most 4, do not consume. |
| 4 | Spend Wealth to Continue Life | Consume up to 3 Wealth; heal 5% max HP each; all enemies gain Armor equal to 80% Money Rat Defense. |
| 5 | Coin Tide Crushes the Vault | Charge one round; consume up to 4 Wealth; 240% plus 25 points per consumed Wealth to Marked first. |

Phase-three passive, **Heavy Interest**: the same trigger grants Wealth at most three times per enemy phase.

| Order | Card | Effect |
|---:|---|---|
| 1 | Empty the Golden Mountain | 120% AoE, then Mark 3 to all party members. |
| 2 | Heavy-Interest Lockdown | Wealth 3; all enemies gain Armor equal to 180% Money Rat Defense; queue one non-stacking +1 Energy surcharge. |
| 3 | Life-Pressing Collection | 200% to Marked first; next-round shared Energy -3; trigger Burn once when present; Wealth 2. |
| 4 | Spend Wealth to Continue Life | Consume up to 3 Wealth; heal 4% max HP each; all enemies gain Armor equal to 120% Money Rat Defense. |
| 5 | Ten-Thousand-Coin Crush | Charge one round; consume up to 6 Wealth; 110% AoE plus 10 points per consumed Wealth. |

### 12.2 Chapter 2

#### Graymane Wolf King

Phase two, **Moonlit Hunting Order**:

| Order | Card | Effect |
|---:|---|---|
| 1 | Moonlit Hunting Order | 90% to lowest HP, then Mark 3/5. |
| 2 | Throat-Rending Chain Hunt | 150%x3 to Marked first, then Bleed coefficient 5/8. |
| 3 | Pack Position Swap | All enemies gain Armor equal to 80% Graymane Defense; Graymane gains Agility 2; lowest HP gets Mark 3/5. |
| 4 | Blood-Moon Pursuit | 230% to Marked first; trigger Bleed once when present. |

Phase three, **Blood Moon Severs the Throat**:

| Order | Card | Effect |
|---:|---|---|
| 1 | Blood-Moon Hunt Mark | 90% AoE, then Mark 3 and Bleed coefficient 5 to all. |
| 2 | Throat-Severing Pack Hunt | 180%x3 to Marked first; trigger Bleed once afterward. |
| 3 | Afterimage Hunt Swap | All enemies gain Armor equal to 100% Graymane Defense and Agility 1; lowest HP gets Mark 5. |
| 4 | Blood Moon Severs the Throat | 260% to Marked first; add Bleed coefficient 8 and immediately trigger once. |

#### Redtusk Boar King

Phase two, **Red-Flame Rage Stomp**:

| Order | Card | Effect |
|---:|---|---|
| 1 | Rage-Tusk Breaks Formation | 170% to Marked first; Weak 3/4 and Burn coefficient 4/6. |
| 2 | Red-Flame Rage Stomp | 100% AoE; Weak 2/3 and Burn coefficient 2/3. |
| 3 | Heavy Armor Stores Rage | All enemies gain Armor equal to 100% Redtusk Defense; Redtusk gains Rage 2. |
| 4 | Red-Tusk Charge | Charge one round; consume all Rage; 250% plus 20 points per consumed Rage to Marked first. |

Phase three, **Redtusk Burns the Mountain**:

| Order | Card | Effect |
|---:|---|---|
| 1 | Redtusk Burns the Mountain | 210% to Marked first; Weak 4; add Burn coefficient 6 and trigger once. |
| 2 | Mad-Tusk Chain Stomp | 105%x3 to Marked first; each Rage adds 8 points to every hit; do not consume. |
| 3 | Fire-Bathed Heavy Armor | All enemies gain Armor equal to 150% Redtusk Defense; Redtusk gains Rage 2. |
| 4 | Mountain-Burning Final Charge | Charge one round; consume all Rage; 280% plus 20 points per Rage; add Burn coefficient 6 and trigger once. |

#### Black Bear

Phase two, **Earth-Splitting Mad Bear**:

| Order | Card | Effect |
|---:|---|---|
| 1 | Blood-Claw Rend | 170% to Marked first; Bleed coefficient 5/8; +40 points when target is Weak. |
| 2 | Angry-Bear Counterstance | Self Armor 180% Black Bear Defense; Counter 2. |
| 3 | Earth-Splitting Mad Bear | 110% AoE; Weak 2/3 and Bleed coefficient 3/5. |
| 4 | Cornered-Beast Pounce | Charge one round; 260% to Marked first; +50 points when target is Weak. |
| 5 | Mountain-Shaking Sweep | 120% AoE; use 160% against Bleeding targets. |

Phase three, **Cornered Beast Shakes the Mountain**:

| Order | Card | Effect |
|---:|---|---|
| 1 | Last-Stand Counterstance | All enemies gain Armor equal to 120% Black Bear Defense; Black Bear gains Counter 3. |
| 2 | Blood-Battle Throat Rend | 220% to Marked first; +40 points when Weak; trigger Bleed once when present. |
| 3 | Cornered Beast Shakes the Mountain | 130% AoE; Weak 3 and Bleed coefficient 5. |
| 4 | Death Pounce | Charge one round; 290% to Marked first; +50 points when Weak; Bleed coefficient 8. |
| 5 | Blood Battle Never Retreats | 120% AoE, 180% against Bleeding targets; heal 3% max HP per Bleeding target that actually loses HP, maximum 9%. |

### 12.3 Chapter 3

#### White Ape

Phase two, **Chaotic Rocks Seal the Meridians**:

| Order | Card | Effect |
|---:|---|---|
| 1 | Chaotic Rocks Seal the Meridians | 160% to Prey first; remove one positive layer; queue one non-stacking +1 Energy surcharge. |
| 2 | Ape Howl Disrupts Formation | 100% AoE; Weak 2/3. |
| 3 | Flying Rocks Guard the Group | All enemies gain Armor equal to 120% White Ape Defense; White Ape gains Agility 1. |
| 4 | Giant-Rock Charge | Charge one round; 270% to Prey first; trigger Bleed once when present. |

Phase three, **Ape King Shatters the Formation**:

| Order | Card | Effect |
|---:|---|---|
| 1 | Ape King Seals the Meridians | 200% to Prey first; remove two positive layers; queue +1 Energy; trigger Bleed once. |
| 2 | Chaotic Rocks Fall from Heaven | 125% AoE; Weak 3; queue one non-stacking +1 Energy surcharge. |
| 3 | Ten-Thousand Stones Guard Formation | All enemies gain Armor equal to 180% White Ape Defense and Agility 1. |
| 4 | Mountain-Crushing Boulder | Charge one round; 310% to Prey first; trigger Bleed once when present. |

#### Spiral-Horn Deer

Phase two, **Peaks Rejuvenation Formation**:

| Order | Card | Effect |
|---:|---|---|
| 1 | Peaks Guard Formation | All enemies gain Armor equal to 150% Deer Defense. |
| 2 | Spiral Horn Intercepts the Hunt | 140% to Prey first, then Mark 3/5. |
| 3 | Forest-Breath Rejuvenation | Lowest-HP enemy heals 12% max HP and removes one non-DOT negative layer; cooldown 3. |
| 4 | Deer-Cry Guardstance | All enemies gain Armor equal to 100% Deer Defense; lowest-HP enemy gains Agility 1. |

Phase three, **Ten-Thousand Trees Suppress Mountains and Rivers**:

| Order | Card | Effect |
|---:|---|---|
| 1 | Ten-Thousand Trees Suppress Mountains and Rivers | All enemies gain Armor equal to 200% Deer Defense. |
| 2 | Spiral Horn Breaks the Hunt | 170% to Prey first, then Mark 5. |
| 3 | Peaks Rejuvenation | All enemies heal 6% max HP; cooldown 4. |
| 4 | Ancient-Forest Shelter | All enemies gain Armor equal to 160% Deer Defense and Agility 1. |

If a Deer heal is still cooling down when selected, use Spiral Horn Intercepts/Breaks the Hunt as the deterministic fallback. Healing never restores phase marks.

#### Tiger

Phase-two passive: Prey persists after Pounce and retargets lowest HP before the next forecast if defeated. A Tiger card that deals HP damage to a Bleeding Prey heals 8% of Tiger's missing HP at most once per card.

| Order | Card | Effect |
|---:|---|---|
| 1 | Blood-Hunt Lockdown | If absent, assign lowest HP as Prey; 160% to Prey; Bleed coefficient 5/8 and Mark 3/5. |
| 2 | Tail Sweep Changes Prey | 110% AoE; afterward move Prey to current lowest HP and add Mark 3/5. |
| 3 | Wound Pursuit | 120%x2 to Prey; trigger Bleed once afterward. |
| 4 | Intimidating Roar | Weak 2/3 to all; Vulnerability 3/5 to Prey. |
| 5 | Grounded Pounce | Charge one round; 290% to Prey; +50 points when Prey is Bleeding. |

Phase-three passive: keep Prey until it dies; then retarget lowest HP without inserting an attack. Bleeding-Prey healing becomes 12% of missing HP, still once per Tiger card. Remove Tiger-owned Prey when Tiger dies.

| Order | Card | Effect |
|---:|---|---|
| 1 | Blood Moon of a Hundred Beasts | If absent, assign lowest HP as Prey; 120% AoE; Prey receives Bleed coefficient 8 and Mark 5. |
| 2 | Death Pounce | 135%x2 to Prey; trigger Bleed once afterward. |
| 3 | Tail Severs the Mountains | 140% AoE; Weak 3; remove one positive layer from every party member. |
| 4 | Feed on Wounds | 210% to Prey; trigger Bleed twice; passive healing remains once for the card. |
| 5 | Fatal Ambush | Charge one round; 320% to Prey; +40 points when Bleeding. |

Single-target Prey attacks remain redirectable and can be handled by Armor, Agility, Guard, Block, or Counter. If Guard redirects the hit away from a Bleeding Prey, Tiger does not receive Bleeding-Prey healing.

## 13. Authored 27-stage formations

Every stage contains four ordinary encounter definitions, two elite encounter definitions, and one stage-end encounter definition. A route may expose a subset as choices, but all seven authored definitions must validate and be testable.

Formation notation is left-center-right. At equal Speed, resolve left before center before right.

- Chapter 1: R=Rooster, G=Goat, W=Weasel, C=Civet, I=Ironfeather, B=Bluehorn, M=Money Rat.
- Chapter 2: Wf=Gray Wolf, Bo=Boar, Ma=Macaque, P=Porcupine, Gm=Graymane, Rt=Redtusk, Bb=Black Bear.
- Chapter 3: S=Venom Snake, C=Wildcat, V=Vulture, T=Giant Toad, A=White Ape, D=Deer, Ti=Tiger.

### 13.1 Normal

| Stage | Level | Four ordinary formations | Two elite formations | Stage end |
|---|---:|---|---|---|
| Normal 1-1 | 5 | R-G-C / W-R-G / W-C-R / G-W-C | R-B-G / W-B-C | W-I-C |
| Normal 1-2 | 10 | W-R-C / R-G-R / C-G-W / W-G-R | R-I-C / W-I-G | W-B-C |
| Normal 1-3 | 15 | W-R-G / C-R-G / W-C-R / R-G-C | W-I-R / C-B-G | I-M-B |
| Normal 2-1 | 20 | Ma-Wf-Bo / Wf-P-Bo / Ma-P-Bo / Ma-Wf-P | Ma-Rt-Bo / Wf-Rt-P | Wf-Gm-P |
| Normal 2-2 | 25 | Ma-Wf-P / Wf-Bo-P / Ma-Wf-Bo / Ma-Bo-P | Ma-Gm-P / Wf-Gm-Bo | Wf-Rt-P |
| Normal 2-3 | 30 | Wf-Wf-P / Ma-Bo-P / Ma-Wf-Bo / Wf-Bo-Bo | Ma-Gm-P / Wf-Rt-Bo | Gm-Bb-Rt |
| Normal 3-1 | 35 | S-C-T / V-C-T / V-S-T / V-S-C | S-D-T / V-D-C | V-A-T |
| Normal 3-2 | 40 | V-C-T / S-S-T / C-C-V / V-S-C | V-A-C / S-A-T | S-D-C |
| Normal 3-3 | 45 | V-S-C / S-C-T / V-S-T / V-C-T | V-A-C / S-D-T | A-Ti-D |

### 13.2 Hard

| Stage | Level | Four ordinary formations | Two elite formations | Stage end |
|---|---:|---|---|---|
| Hard 1-1 | 50 | W-R-R / C-R-G / W-G-C / R-G-R | W-B-R / C-B-G | W-I-G |
| Hard 1-2 | 55 | W-R-C / C-G-G / W-G-R / C-R-R | W-I-R / C-I-G | C-B-W |
| Hard 1-3 | 60 | W-R-G / C-R-G / W-C-R / W-C-G | W-I-C / R-B-G | I-M-B |
| Hard 2-1 | 65 | Ma-Wf-P / Wf-Wf-Bo / Ma-Bo-P / Wf-Bo-P | Ma-Rt-P / Wf-Rt-Bo | Ma-Gm-P |
| Hard 2-2 | 70 | Ma-Wf-Bo / Wf-Wf-P / Ma-Wf-P / Ma-Bo-Bo | Ma-Gm-P / Wf-Gm-Bo | Wf-Rt-P |
| Hard 2-3 | 75 | Ma-Wf-P / Wf-Bo-P / Ma-Wf-Bo / Wf-Bo-Bo | Ma-Gm-P / Wf-Rt-P | Gm-Bb-Rt |
| Hard 3-1 | 80 | V-C-T / S-C-T / V-S-C / S-S-T | V-D-T / S-D-C | V-A-T |
| Hard 3-2 | 85 | V-S-T / C-C-V / S-C-T / V-S-C | V-A-C / S-A-T | S-D-C |
| Hard 3-3 | 90 | V-S-C / S-C-T / V-C-T / V-S-T | V-A-C / S-D-T | A-Ti-D |

In Hard stage 1 and 2 endings, only the center Elite has two phases. In every Hard stage 3 ending, all three Elite/Boss units have two phases.

### 13.3 Hell

| Stage | Level | Four ordinary formations | Two elite formations | Stage end |
|---|---:|---|---|---|
| Hell 1-1 | 95 | W-R-R / C-R-R / W-G-R / C-G-R | W-B-R / C-B-G | W-I-C |
| Hell 1-2 | 100 | W-G-G / C-G-G / W-R-C / W-G-C | W-I-R / C-I-G | C-B-W |
| Hell 1-3 | 105 | W-R-G / C-R-G / W-C-R / W-C-G | W-I-C / C-B-G | I-M-B |
| Hell 2-1 | 110 | Wf-Wf-P / Ma-Wf-P / Wf-Bo-P / Ma-Bo-Bo | Ma-Rt-P / Wf-Rt-Bo | Ma-Gm-P |
| Hell 2-2 | 115 | Ma-Wf-Bo / Wf-Wf-P / Ma-Wf-P / Wf-Bo-Bo | Ma-Gm-P / Wf-Gm-Bo | Wf-Rt-P |
| Hell 2-3 | 120 | Ma-Wf-P / Wf-Bo-P / Ma-Wf-Bo / Wf-Wf-Bo | Ma-Gm-P / Wf-Rt-P | Gm-Bb-Rt |
| Hell 3-1 | 125 | V-C-T / S-S-T / V-S-C / S-C-T | V-D-C / S-D-T | V-A-T |
| Hell 3-2 | 130 | V-S-T / C-C-V / S-C-T / V-S-C | V-A-C / S-A-T | S-D-C |
| Hell 3-3 | 135 | V-S-C / S-C-T / V-C-T / V-S-T | V-A-C / S-D-T | A-Ti-D |

In Hell stage 1 and 2 endings, only the center Elite has three phases. In every Hell stage 3 ending, all three Elite/Boss units have three phases.

### 13.4 Opening intent authoring

Each formation stores an initial intent per slot. Do not derive all openings from cursor zero.

The percentages and coefficients in sections 10-12 are authoring values. Enemy intent cards show resolved target scope, per-hit final damage, hit count, final DOT/status amounts, Armor/healing, charge remaining, and conditional bonuses; players are not required to reverse-engineer these tables during combat.

- Weasel/Civet opens Mark before Rooster Double Peck or Goat Horn.
- Gray Wolf/Graymane opens Mark before Porcupine Quill, Boar charge, Redtusk, or Black Bear payoff.
- Vulture opens Gaze; Venom Snake opens Venom Bite; Wildcat opens Rake or Stalk as authored; Giant Toad opens Poison Fog in mixed-DOT formations.
- Duplicate Roosters split Crow/attack; duplicate Goats split Horn/Stomp; duplicate Gray Wolves split Bite/Call Pack; duplicate Boars split Tusk/Bristle; duplicate Snakes split Venom Bite/Coil; duplicate Wildcats split Stalk/Rake.
- Full phase-deck trios use the first listed phase card after all relevant units enter that phase.

Charged intents save their source, declared target rule, visible locked target, remaining rounds, and explicit fallback. If the target becomes invalid, refresh to the declared fallback before execution and update the preview; never display target A and silently hit target B.

Avoid duplicate Weasels, Civets, Porcupines, or Vultures in one formation. Their group Weak, Energy denial, repeated Block, or Burn pressure would become a restriction rather than an interaction.

## 14. Current implementation conflicts and required migration

The implementation plan must address these root conflicts rather than patch symptoms:

1. Training currently constructs enemies at `PlayerLevel`; it must use the authored stage level.
2. combat-unit validation currently rejects slotted enemies above level 100; player and enemy maxima must be distinct.
3. difficulty is not carried into card battle as a combat context; add the damage, intent-value, and phase-count context.
4. the current phase model has only `bPhaseTwo`, a 50% threshold, Boss-only enum values, and optional stat mutations; replace it with data-driven total/current phases and phase decks for all nine Elite/Boss identities.
5. current phase transition does not perform lethal clamping, complete heal, negative clear, positive retention, charged-intent cancellation, or phase-deck replacement.
6. `MaxCombatArmor`, legacy Shield projection, validation, and documentation still cap Armor at 99; remove the gameplay cap consistently.
7. DOT currently uses legacy layer damage/consumption behavior; introduce resolved reservoirs, caps, full cleanses, and preview parity.
8. enemy Energy theft currently subtracts pre-refill Energy; introduce a saved next-round penalty.
9. enemy Counter/Block requires side-symmetric, non-recursive resolution. Porcupine's old passive tag must not remain a no-op.
10. Giant Toad currently writes player Medicine with no valid monster consumer; use the approved one-use healing amplification.
11. formation definitions contain only enemy IDs; add per-slot opening intents and deterministic fallback metadata.
12. current forecasts lock independently; build an ordered, non-mutating forecast chain and refresh it after every relevant player-state change.
13. current stage pools omit Ironfeather, Money Rat, Macaque, Porcupine, Vulture, and Giant Toad from parts or all of Training; replace them with section 13.
14. item/chest validation still caps item level at 100; allow 135 and implement the previous+1-to-current idle band.
15. current Boss victory bypasses a dedicated settlement page; introduce the unique pending receipt flow.
16. task/formula/phase presentation is not sufficient to reconstruct the approved owner-scoped status rows.

Save migration requirements:

- map legacy Boss `bPhaseTwo=true` to current phase 2 without retroactively healing or clearing state;
- infer total phases from saved Training difficulty and encounter tier; old non-Boss enemies start at phase 1; an active legacy non-Training battle with an old catalog Boss phase preserves a compatible two-total-phase profile rather than crashing or receiving a new Hell phase;
- convert or safely clear a pending intent that is invalid in the inferred phase, then rebuild a visible intent without executing it;
- preserve HP, positive statuses, deck zones, owner formula/task state, and rewards;
- never grant a phase heal, phase reward, or settlement reward merely because a save was migrated;
- validate old active battles deterministically and fail with a recoverable error rather than corrupting the save.

## 15. Full verification matrix

### 15.1 Exact semantic fixtures

| Scope | Cases |
|---|---:|
| 12 ordinary enemies x 3 cards x 3 difficulties | 108 |
| 9 phase enemies, 42 phase-one cards x 3 difficulties | 126 |
| 39 phase-two cards x Hard/Hell | 78 |
| 39 phase-three cards x Hell | 39 |
| **Resolved monster intent/difficulty cases** | **351** |
| 173 active player cards x up to 3 legal quality states | up to 519 |
| 27 stages x 7 authored encounters | 189 formations |

Every card fixture verifies target and fallback, costs, final damage, hit count, per-hit Defense, quality, upward rounding, DOT added/actual/cap, Armor, healing and overheal, statuses, resources, draw/discard/Exhaust, reactions, task/formula ownership, phase boundaries, save/load, display text, tooltip, preview, and combat-log parity.

Property and boundary tests cover levels 1 through 135, especially 1, 24, 25, 26, 49, 50, 51, 74, 75, 76, 99, 100, 101, 124, 125, 126, and 135; all quality multipliers; Armor above 99; level differences beyond both +/-50 bounds; and DOT additions at one below/equal/above cap.

Phase fixtures include:

- exact 1% boundary and lethal clamp;
- one packet not overflowing;
- a multi-hit card crossing multiple phases through later packets;
- DOT, replay, Counter, Block, and delayed packets crossing a phase;
- several enemies transitioning from one AoE card;
- old charged intent cancellation without an inserted attack;
- complete negative clear and complete positive/Armor retention;
- Ironfeather passive rearming;
- Wealth/Rage retention;
- Deer/Tiger healing not restoring phase marks;
- save/load before, during, and after every transition.

### 15.2 Strategy validation

The current immediate-value greedy policy cannot certify this design. Before aggregate simulation, a deterministic two-ply or bounded beam-search policy must pass profession puzzles for:

- setup then DOT/Armor/Charge consumption;
- lethal-intent defense;
- Sorcerer task completion with an Exhausted card;
- Hero and partner formula ownership;
- terrain setup and explicit trigger use;
- Mark/Prey redirection;
- saving Energy/Mana for a forecast payoff;
- target-priority changes after killing a setup or support enemy.

### 15.3 Aggregate simulations

Enumerate six Hero profession packages and every legal pair of the six permanent-partner roles (up to `6 * C(6,2) = 90` three-person builds). Use four progression cohorts:

1. minimum eligible equipment;
2. mean equipment between previous and current stage;
3. reasonable stage equipment and available gems;
4. maximum expected equipment, with the Hell 3-3 primary cohort using six Treasure pieces and twelve Treasure gems split 4/4/4.

Recommended run sizes:

- tuning gate: `27 stages * up to 90 builds * 4 cohorts * 20 seeds` = up to 194,400 deterministic runs;
- certification: the same matrix at 100 seeds = up to 972,000 runs;
- Hell 3-3 sensitivity: up to 90 builds, four gem distributions, 200 seeds = up to 72,000 runs.

Repeated runs of the same seed must produce identical result and telemetry hashes.

### 15.4 Per-card telemetry and review flags

Collect by CardId: draws, plays, legal-but-rejected plays, insufficient-resource rejection, hand-held rounds, direct/DOT/fixed damage, overkill, healing/overheal, Armor generated/absorbed/wasted, statuses added/naturally changed/cleared, Energy/Mana generated and left unused, task/formula completions, terrain triggers, and phase packets.

Flag for human review, without automatically nerfing:

- less than 10% play rate when legal;
- at least 80% zero effective output or cap/overheal waste;
- more than twice the same-cost/same-role median effective output;
- more than 60% rejection for cost;
- more than 70% overkill, overheal, or unused Armor;
- non-monotonic or invisible Rare/Epic improvement;
- more than 65% selection when comparable alternatives are available;
- a task/formula that almost never completes in a normal ten-round Boss fight.

Mandatory observation cards/systems include uncapped Armor conversion (`XuanJiaZhenYue`), the large `ZhenShaZhen` terrain payoff, Blood Edge/DOT conversion, toxic explosions and multi-Charge Heavy Arrow, `FengShenBu`, the Sorcerer Exhaust/task boundary, multi-trigger terrain cards, Money Rat Energy denial plus White Ape surcharge, and White Ape/Deer group Armor/Agility/healing.

## 16. Acceptance and tuning order

The binding late-game benchmark is the Hell 3-3 stage-end battle with a skilled deterministic policy, level-100 party, six Treasure equipment pieces, and twelve Treasure gems split four Attack/four Defense/four HP:

- aggregate representative win rate 45%-55%;
- winning median 9-11 rounds, with the main result mass in 8-14 rounds;
- no infinite battle, recursive reaction, repeated phase, duplicated reward, or unresolved terminal state;
- every reasonable profession direction has at least one executable defensive line;
- the encounter cannot require Guard exclusively for the second wave or Healer exclusively for DOT;
- the first enemy wave may be threatening, but a correct visible response must prevent a deterministic unavoidable kill.

Other stage length and win rates are diagnostic until observed, but must terminate, progress sensibly with stage level/difficulty, and avoid an earlier stage becoming a hidden wall solely because two initial intents accidentally align.

Tune in this order:

1. fix rule, preview, save, or strategy-policy errors;
2. adjust authored opening-intent offsets;
3. when fights are too short, raise phase-two/three payoff cards before adding HP;
4. when fights exceed 14 rounds, reduce support Armor/healing on Deer, Bluehorn, or Money Rat before reducing the whole enemy roster;
5. only then change base HP/Attack/Defense curves;
6. never solve tuning by removing approved phases, hiding intent information, or reintroducing a hard Armor cap.

## 17. Documentation and implementation handoff

This specification is ready for implementation planning only after its documentation diff is reviewed and committed by itself. The implementation plan must use test-driven development for runtime behavior, preserve the dirty user worktree, stay on root `main`, avoid UnrealBridge and Live Coding, and verify C++ with UBT/the project TDD pipeline after saving and safely closing the editor when required.

The implementation should be split into dependency-ordered units rather than one balance mega-change: shared numeric primitives and migration; active card catalog; player cards; task/formula/terrain UI; enemy phase runtime; enemy catalogs; Training formations/levels; equipment/idle rewards; settlement; simulations and final PIE acceptance.

## 18. Deferred in-run UX and content backlog

The user explicitly recorded the following follow-up scope on 2026-09-03. These are **not yet designed or approved solutions** and must receive their own UX/content analysis before implementation:

### 18.1 In-run interfaces

- optimize the in-run shop UI;
- design and optimize the in-run event UI;
- optimize the in-run reward screen;
- optimize individual reward option cards;
- optimize in-run enemy intent cards for fast reading of targets, hits, final numbers, statuses, charge, and phase interactions.

### 18.2 Event content

- increase event variety;
- improve how interesting, surprising, and decision-relevant events are;
- audit whether choices create meaningful tradeoffs rather than obvious best answers or text-only variants of the same reward.

### 18.3 Relics

- bring in-run relic values into the approved card/equipment/monster budget;
- increase relic variety;
- improve relic strength and build-defining value without creating mandatory universal picks.

Relic calibration is a dependency of final aggregate balance certification. Until relic design is approved, certification must either hold relics fixed/exclude them from the primary cohort or clearly label the results provisional; it must not attribute relic-driven variance to cards or monsters.

### 18.4 Route-map presentation

- create an original in-run route-map background;
- improve route-node art, hierarchy, readability, connection lines, reachable/visited states, and interaction feedback;
- do not use, copy, trace, extract, or ship unpacked *Slay the Spire* map assets. The user has rejected those assets because of copyright risk. Future work must use project-owned or newly created original visual assets and a distinct presentation.

### 18.5 Settlement UX

The settlement screen is currently not connected to the Boss-victory flow. Section 8 defines the required idempotent receipt and navigation behavior, but the page's information architecture, reward hierarchy, statistics presentation, confirmation flow, return transition, and reload experience still require a dedicated later UX analysis.

### 18.6 Packaged HUD cross-machine DPI and white-edge risk

The HUD was previously adjusted and may appear correct in a packaged build on the user's current computer and DPI setting. That local result is not cross-machine acceptance: other displays, Windows scale factors, resolutions, aspect ratios, or window modes may still expose white strips at one or more viewport edges.

Record this as an unresolved packaged-build compatibility risk. A later focused investigation must reproduce and verify the HUD on multiple machines or equivalent display configurations, including common Windows DPI scales, 16:9/16:10/ultrawide resolutions, windowed/borderless/fullscreen modes, and monitor changes. Evidence must use packaged builds and edge-visible screenshots or pixel checks; PIE or one local DPI setting alone cannot close the issue. The eventual diagnosis must distinguish layout/rounding/viewport-fill errors from operating-system window chrome and GPU/display scaling before changing the HUD again.

### 18.7 Remove the 3D town from the main flow and redesign tutorials

The 3D town may remain inside the Unreal project for editor access, isolated experimentation, or explicit legacy testing. It must not appear anywhere in the canonical player main flow, startup/fallback path, normal navigation, tutorial, or packaged product experience.

Package weight remains a concern. A future scoped change must isolate town-only maps and assets from shipping cook references so they do not enter the normal packaged build merely because they remain available in the UE project. Audit direct and indirect references before changing cook configuration; preserve shared assets or code still required by the pure-2D desktop/BattleBoard flow. Existing saves whose current destination is the 3D town must migrate deterministically to the canonical 2D desktop surface. Capture package-size evidence before and after cook isolation.

The current tutorial guidance is not the final tutorial. Record a separate future redesign for the pure-2D desktop-to-BattleBoard experience. Do not place town movement, 3D NPC interaction, or 3D-map guide anchors in the new canonical tutorial. Its teaching order, anchors, skip/replay behavior, persistence, and first-battle flow require their own brainstorming and UX approval.

This decision does not authorize deletion of 3D-town maps or assets from the UE project. This documentation update only corrects their intended product-flow and packaging status.

These backlog items should not silently expand the first runtime implementation plan. That plan may establish required data/runtime hooks for intent display, rewards, relic telemetry, and settlement receipts, while visual/content optimization remains separately scoped and reviewed.
