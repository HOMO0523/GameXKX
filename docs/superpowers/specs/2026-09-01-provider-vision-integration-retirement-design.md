# Project-local vision integration retirement and reviewer-policy cleanup

**Status:** Approved for implementation

**Date:** 2026-09-01

**Workspace:** root project on `main`

## Intent

Remove the project-local, provider-specific image-understanding client and its automated PIE review flow. Separately, retire earlier project wording that requires, prefers, or forbids a named visual reviewer. Future agents remain responsible for proportionate verification, but may choose the most suitable visual-review method for the task.

## Decisions

1. The provider-specific client is removed rather than deprecated. There will be no compatibility shim, environment-variable fallback, network endpoint, command-line entry point, or provider-specific test suite left in the project.
2. Generic PIE screenshot capture is not part of the retired integration. The one unrelated locomotion probe that imports the old PIE module will instead use the existing neutral Slate/MCP screenshot primitives.
3. Rules may describe required evidence or acceptance outcomes, but must not mandate, prefer, or prohibit a named AI reviewer. This applies to both positive and negative restrictions.
4. Past-tense evidence remains factual. Existing reports that record which reviewer was used may stay, as may optional helper capabilities, provided they are not presented as mandatory policy.
5. Dedicated temporary reports, prompts, sessions, and screenshots produced solely by the retired provider-specific integration are removed. Ordinary screenshots, art assets, gameplay saves, and unrelated visual-review evidence are preserved.
6. Git history and tools installed outside this project are out of scope.

## Project changes

### Provider-specific integration

- Delete `scripts/gamexxk_vision.py` and `scripts/gamexxk_vision_pie.py`.
- Delete their two dedicated unit-test modules.
- Remove their commands, API-key guidance, imports, and feature description from `scripts/README.md` and project documentation.
- Remove the dedicated provider-named handoff document and repair every live link that points to it.
- Remove provider-specific generated evidence under `Saved` using exact, reviewed file targets. Do not use a broad recursive deletion against `Saved`.

### Neutral screenshot dependency

`scripts/run_town_hero_horizontal_pie_probe.py` currently imports screenshot capture from the retired PIE module. Replace that import with the neutral preview-window lookup, PNG decoding, and size helpers already owned by `scripts/gamexxk_real_play_flow_mcp.py`. Preserve the probe's current map, PIE lifecycle, output paths, validation semantics, and leave-PIE-running behavior.

### Named-reviewer policy

- Rewrite active source-of-truth documents and actionable, unfinished plan steps that say a named reviewer is required, preferred, prohibited, or a completion gate.
- Express acceptance in observable terms such as screenshot coverage, geometry, readability, clipping, alpha, dimensions, hashes, runtime state, or user confirmation.
- Keep historical statements such as a past report result or an already-recorded evidence path unchanged unless they are also phrased as a future rule.
- Keep optional reviewer scripts available, but revise comments or documentation that describe them as compulsory.
- Do not introduce a replacement restriction such as “AI review is forbidden” or “manual review is mandatory.”

## Resulting verification flow

After the change, a future agent selects verification according to the work:

- deterministic checks for dimensions, alpha, hashes, manifests, geometry, logs, and runtime state where applicable;
- screenshots or direct visual inspection when presentation must be judged;
- any suitable optional visual-review tool when it adds value;
- user confirmation when the user requests or owns the final visual decision.

The project specifies the evidence needed, not the model or reviewer that must produce it.

## Safety and error handling

- Preserve all unrelated tracked, untracked, and binary user changes in the dirty worktree.
- Do not edit protected Unreal assets, maps, character sprites, PaperZD assets, camera transforms, or HD2D plane values.
- Resolve and review every `Saved` deletion target before removal. Delete only provider-specific review artifacts, never an enclosing broad directory.
- If a live import remains after source deletion, fix the dependency rather than restoring a compatibility wrapper.
- Broken documentation links or manifests are implementation failures and must be repaired before completion.

## Verification

Implementation acceptance requires:

1. No project source, configuration, command, import, environment-variable name, endpoint, or tracked filename remains for the retired provider-specific integration.
2. No actionable project instruction requires, prefers, or prohibits a named visual reviewer.
3. Historical reviewer evidence and unrelated visual assets remain intact.
4. The neutral town locomotion probe and its imported Python modules compile successfully.
5. Focused Python tests and the project documentation/state validator pass.
6. `git diff --check` passes.
7. The final diff contains only this retirement work and does not absorb unrelated user changes.

No UBT build, editor restart, or PIE session is required because the design changes only project automation and documentation, not Unreal runtime code or assets.
