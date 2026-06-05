# 2026-06-05 Remove HDR and pause GPU expansion

## Decision

Remove the HDR workflow from the plugin because the current mapping is visually incorrect and creates product risk. Keep the already working CUDA path, but pause remaining hardware acceleration expansion until real user demand or measurable performance pressure justifies it.

## Scope

1. Replace HDR-positive tests with removal tests.
2. Remove Rec.2100 PQ/HLG choices, HDR params, HDR CPU mapping, HDR CUDA helpers, and HDR kernel launch arguments.
3. Rebuild and reinstall the OFX bundle.
4. Update GPU planning docs to reflect the new product direction.

## Verification

Run the focused unit tests, build the plugin, install the resulting OFX binary, and compare build/install hashes. Do not automate Resolve activation or licensing screens.
