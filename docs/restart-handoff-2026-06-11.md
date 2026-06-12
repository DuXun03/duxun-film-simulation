# Restart handoff - 2026-06-11

## Current State

- Mainline remains OpenFX v5.0 for `DuXun Film Simulation`.
- Do not start HDR, OpenCL, Metal, new GPU expansion, or LUT runtime loading from this handoff.
- First Resolve visual A/B pass: `docs/resolve-visual-ab-2026-06-09.md`.
- Second Resolve visual A/B pass: `docs/resolve-visual-ab-2026-06-11.md`.
- Second-pass export evidence: `docs/visual-ab/2026-06-11/`.
- Current installed OFX path:

```text
C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx
```

## Verified

- Fresh pre-commit verification on 2026-06-12:
  - `python -m unittest discover -s tests -q` -> `Ran 56 tests`, `OK`.
  - `cmd /c scripts\install.bat` -> exit 0, reinstall path completed, build/installed SHA256 matched.
- Build and installed OFX SHA256 match:

```text
A508CA29788705D29D78E046FB3DF7D2144C29ED32BBE6CC6662DA2063F48062
```

- Installed OFX SHA256:

```text
A508CA29788705D29D78E046FB3DF7D2144C29ED32BBE6CC6662DA2063F48062
```

## Visual Status

- Fuji Superia 100/200/400/800/1600/HG 1600: second-pass daylight close-up skin evidence passed. Skin does not read waxy, red, or yellow-green; grain scales naturally.
- Agfa Vista 100/200/400: second-pass daylight close-up skin and high-key sky evidence passed. Skin remains natural; sky shoulder is present but not excessive.
- CineStill 800T: second-pass tungsten practical restaurant and warm food-truck evidence passed after the white-halo regression was fixed. Red-orange halation is now credible and localized; global haze is controlled.
- CineStill 50D: first-round daylight exterior pass remains unchanged.

## Visual Baseline Freeze

- Fuji Superia: visual baseline frozen.
- Agfa Vista: visual baseline frozen.
- CineStill 800T: visual baseline frozen.
- CineStill 50D: keep the first-round daylight exterior pass state; do not treat it as reopened or retuned in the second pass.

## Tuning Notes

- CineStill 800T was tuned during the 2026-06-11 pass because user validation showed the prior halo remained white/yellow-white.
- CPU and CUDA halation paths now both use red/orange leakage instead of blurring original white RGB highlights.
- CineStill 800T procedural defaults now use `halationAmount 0.34`, `threshold 0.74`, `radius 0.60`, `warmth 0.96`, `local 0.72`, `global 0.025`, `hue 0.72`, `blueComp 0.86`, `impact 0.58`, `defringe 0.42`.
- A render-time CineStill 800T guard keeps old Resolve nodes from retaining the previous white-halo defaults.
- No matrix changes.
- If future visual regressions appear, stay within procedural defaults first: `printAmount`, `colorDensity`, `vibrance`, `filmGrainAmount`, `filmGrainResolution`, `filmGrainChroma`, `halationAmount`, `halationThreshold`, `halationRadius`, `halationLocal`, `halationGlobal`, `halationImpact`.

## Release Prep Entry

- Release packaging, reinstall/uninstall, Resolve OFX cache cleanup, and minimal user install docs now start from `docs/release-preflight-2026-06-12.md`.
- `scripts/install.bat` remains the OpenFX v5.0 install/reinstall path. It overwrites the installed OFX file, then verifies build and installed SHA256.
- `scripts/uninstall.bat` removes only `C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle`.
- Resolve OFX cache cleanup should be done only after closing Resolve. The local cache path observed on this workstation is `%APPDATA%\Blackmagic Design\DaVinci Resolve\Support\OFXPluginCacheV2.xml`.
- Trial, activation, and buyout remain design-only follow-up work. Do not implement licensing in the current visual-baseline stabilization package.
