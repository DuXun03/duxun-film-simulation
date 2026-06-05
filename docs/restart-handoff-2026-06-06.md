# 2026-06-06 restart handoff: DuXun Film Simulation

## Current direction

Continue product-value work. HDR remains removed, and hardware acceleration expansion remains paused unless a measured commercial blocker appears. The current priority is film-look differentiation, Resolve visual A/B, stability, installer/licensing, and release packaging.

## Verified in this pass

- Added `applyFujiAgfaCineStillDefaults` for Fuji Superia, Agfa Vista, CineStill 800T, and CineStill 50D.
- Refactored CineStill no-remjet defaults out of the main preset-default body and into the stock-specific helper.
- Updated Fuji Superia defaults for consumer-negative color and ISO-scaled grain, including stronger high-speed defaults for Superia 1600 / HG 1600.
- Updated Agfa Vista defaults for restrained daylight C-41 color, fine grain, and no default halation.
- Updated CineStill 800T / 50D defaults for no-remjet halation, grain, density, and bloom-off behavior.
- Added `scripts/resolve_visual_ab_plan.py`.
- Added `docs/resolve-visual-ab-2026-06-06.md`.
- Updated `docs/preset-resource-mapping-2026-06-05.md` and `docs/gpu-acceleration-plan.md`.
- Built and installed the OFX binary.

Installed OFX SHA256:

```text
1C7609A81AB283167071E949D4C4438EB493DF5E1CCD1D20A6ED9B482350870D
```

## Verification commands

```text
python -m unittest discover -s tests -q
rg -n "HDR|hdr|Rec\.2100|TRANSFORM_REC2100" ofx\DuXunFilm\DuXunFilmSim.cpp
cmd /c build_plugin.bat
python scripts\resolve_visual_ab_plan.py
```

Results:

```text
Ran 53 tests OK (skipped=1)
HDR source scan: no matches
build_plugin.bat exit code 0
build/install hash match: 1C7609A81AB283167071E949D4C4438EB493DF5E1CCD1D20A6ED9B482350870D
```

## Resolve visual A/B status

The A/B plan is prepared but real visual exports are not yet captured in this pass. Use `docs/resolve-visual-ab-2026-06-06.md` as the checklist and put exports in a dated `docs/visual-ab/` folder.

Do not automate Resolve activation, licensing, crack-related, or account screens. If Resolve opens to an activation screen, stop UI automation and ask the user to open a usable project.

## Next product work

1. Capture Resolve A/B exports for Fuji Superia, Agfa Vista, CineStill 800T, and CineStill 50D.
2. If A/B shows over-strong defaults, tune procedural defaults first; change matrices only when hue relationships are visibly wrong.
3. Continue preset depth with remaining lower-value stocks and print-stock behavior.
4. After preset differentiation stabilizes, move to installer/licensing/trial/buyout flow.
