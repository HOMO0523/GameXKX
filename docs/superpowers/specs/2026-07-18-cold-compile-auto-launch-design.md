# Cold Compile Auto Launch Design

## Goal

Running the existing `scripts/ue_tdd_pipeline.py` with its default arguments must cold-compile GameXXK and then automatically start `GameXXK.uproject` for MCP/PIE verification.

## Design

Keep `ue_tdd_pipeline.py` as the single workflow owner. Its existing save-through-MCP and refusal-to-close behavior remain unchanged. The build command receives `-NoHotReload`, and the editor launch receives `-DDC-ForceMemoryCache` so the current machine's read-only installed DDC graph cannot abort startup.

## Acceptance

- The cold build command includes `-NoHotReload`.
- The automatically launched editor command includes the `.uproject`, MCP server/port flags, and `-DDC-ForceMemoryCache`.
- The existing default cycle remains build first, launch second.
- Unit tests use mocked subprocess boundaries; they never terminate or launch a real editor.
