# Party Progression, Locked Deck, and Equipment Level Gates

## Scope

This change covers the pure-2D desktop Training flow and its embedded Backpack:

- locked Training difficulties remain viewable;
- the deployed hero, permanent companion, and task NPC receive progression;
- every permanent companion exposes its full eighteen-card profession pool;
- equipment cannot be newly equipped above the target character's level.

It does not redesign the Training panel, change the five-second Travel walk delay,
change the five-card companion combat deck size, or add NPC stars/card unlocks.

## Training difficulty browsing

- Normal, Hard, and Hell always remain selectable in the difficulty dropdown.
- Selecting a locked difficulty only changes the viewed difficulty/chapter and
  `SelectedStageId`. It must not call `StartTrainingTravel`, change
  `CurrentTravelStageId`, rebuild the Travel runtime, heal the party, or reset a
  wave timer.
- Locked difficulty nodes remain visible with the existing dimmed lock state.
- Challenge and Travel remain disabled until their existing progression rules
  allow them.
- Progression remains 27 stages: Normal 3-3 unlocks Hard; Hard 3-3 unlocks Hell.

## Shared level and experience rules

- Hero, permanent companions, and task NPCs have a level range of 1-100.
- Experience required to advance from level `L` is `L * 100` for all three.
- A level-100 character stores zero residual experience.
- Every experience-bearing Training reward grants the full experience amount to
  each currently deployed member: hero, active permanent companion, and active
  task NPC. The value is not split.
- Non-deployed companions and NPCs receive no experience.
- The same rule applies to online Travel, accumulated/offline Travel collection,
  and Training Challenge experience settlement.
- Level-up changes are persisted immediately. The active Travel party projection
  is synchronized at the reward boundary so subsequent waves use the new level
  and derived stats without reopening the Backpack.

## NPC progression storage

- Add a save-compatible `FGameXXKQuestNpcProgression` value containing `Level`
  and `Experience`, plus a per-NPC map owned by the party/card-run save state.
- Normalization creates a level-1, zero-experience entry for every approved named
  task NPC and removes unknown entries.
- Existing saves with no map migrate to those defaults. Existing valid values are
  clamped to level 1-100 and normalized against the unified experience threshold.
- NPC attributes, battle combat level, Travel party stats, Backpack attribute
  text, experience bar, and equipment-level checks all read the NPC's own level.
- NPCs have no star progression. Their existing four fixed cards remain
  unaffected by level.

## Permanent companion card ownership

- Every profession has eighteen catalog cards.
- The companion's existing six seeded birth cards remain the first six entries,
  preserving their order and the current five-card selection.
- The remaining twelve same-role catalog cards are appended once in stable
  catalog order, excluding the six birth cards. No reroll occurs during migration
  or level-up.
- Unlock frontier:
  - levels 1-4: cards 1-6 unlocked;
  - levels 5-9: cards 1-10 unlocked;
  - levels 10-14: cards 1-14 unlocked;
  - levels 15-100: all eighteen unlocked.
- The Backpack deck grid shows all eighteen cards. Unlocked cards are shown first;
  locked cards remain below them with the existing dim tint and lock icon.
- Locked cards retain hover tooltips but cannot be selected into the five-card
  combat deck.
- Locked cards display `5级解锁`, `10级解锁`, or `15级解锁`. That text and the
  lock treatment disappear immediately after the card unlocks.
- Unlocking cards never modifies the currently selected five-card deck.

## Equipment level gate

- A new equip/replace transaction is legal only when
  `Equipment.ItemLevel <= TargetCharacter.Level`.
- Hero checks use hero level; companions use their own level; NPCs use their own
  persisted level.
- The authoritative gate lives in the shared equipment transaction layer so
  left-click placement, right-click quick equip, drag/drop replacement, and
  equipment swaps cannot disagree.
- Rejected operations are atomic and report `需要角色达到 X 级` without moving
  either item.
- Legacy or existing over-level equipped items remain equipped. Once removed,
  they cannot be equipped again until the character reaches the requirement.
- Normalization and save migration must not auto-unequip such legacy items.

## UI refresh

- The embedded Backpack shows live level and experience for hero, companion, and
  NPC without closing/reopening the panel.
- NPC experience text/bar are no longer hidden.
- Companion unlock transitions update card tint, lock icon, unlock text, and
  clickability in place.
- Difficulty browsing updates the viewed nodes while the current Travel label and
  runtime remain unchanged.

## Verification

- A locked Hard/Hell difficulty can be viewed while its Challenge/Travel buttons
  stay disabled and the existing Travel runtime remains byte-for-byte unchanged.
- Online, offline, and Challenge Training rewards advance exactly the deployed
  trio; inactive roster members remain unchanged.
- All three character kinds stop at level 100 with zero residual experience.
- NPC progression survives save/load and drives stats, battle level, UI, and
  equipment gating.
- Companion migration expands six cards to eighteen without changing the first
  six or selected five; unlock counts are 6/10/14/18 at levels 1/5/10/15.
- Locked cards render, expose tooltips, reject selection, and lose lock text/icon
  after unlocking.
- Every equip entry path rejects over-level items; pre-existing over-level gear
  remains equipped until manually removed.
- Cold UBT compilation, focused automation, the full Training/Workbench and
  FinalInventory groups, and visible PIE verification must pass.
