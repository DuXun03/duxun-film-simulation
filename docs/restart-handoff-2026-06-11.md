# Restart handoff - 2026-06-11

## Current State

- Mainline remains OpenFX v5.0 for `DuXun Film Simulation`.
- Do not start HDR, OpenCL, Metal, new GPU expansion, or LUT runtime loading from this handoff.
- 2026-06-09 Resolve visual A/B pass is documented in `docs/resolve-visual-ab-2026-06-09.md`.
- Export evidence for the pass is under `docs/visual-ab/2026-06-09/`.
- Current installed OFX path:

```text
C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx
```

## Verified

- `python -m unittest discover -s tests -q` -> `Ran 54 tests`, `OK`.
- `scripts\install.bat` installs the OpenFX v5.0 bundle and prints build/installed SHA256.
- Build and installed OFX SHA256 match:

```text
79AAD6E62C1BDE9614DB5C4EFBA71EE45DF52D084096F509BBF9AA80F7E502E2
```

## Visual Status

- Fuji Superia 100/200/400/800/1600/HG 1600: first-round pass on daylight exterior.
- Agfa Vista 100/200/400: pass with caution after reducing procedural `printAmount` to `0.18`.
- CineStill 800T: first-round pass on neon/night signage after stronger halation defaults.
- CineStill 50D: first-round pass on daylight exterior.

## Remaining Risk

- Need close-up daylight skin validation for Fuji Superia and Agfa Vista.
- Need one more bright sky/high-key scene for Agfa Vista because highlight headroom is still the main caution.
- Need a tungsten-heavy practical night scene for CineStill 800T; current pass mostly covers LED/neon behavior.
- Keep using waveform/vectorscope or PNG stats for subtle shoulder/saturation tweaks before touching matrices.
