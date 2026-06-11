# Resolve visual A/B handoff - 2026-06-09

## Scope

This pass closes the first Resolve visual A/B loop for the current DuXun Film Simulation build. It follows `docs/resolve-visual-ab-2026-06-06.md` and stays inside procedural look tuning only. No HDR, OpenCL, Metal, new GPU path, or matrix retargeting work was started.

- Resolve project: `Untitled Project 2`
- Timeline: `Timeline 1`
- OFX installed SHA256: `79AAD6E62C1BDE9614DB5C4EFBA71EE45DF52D084096F509BBF9AA80F7E502E2`
- Export folder: `docs/visual-ab/2026-06-09/`
- Naming: `<scene>__<preset>__20260609.png`

## Scenes

Eight Resolve clips were available during this pass. Clip 8 was added as the night scene for CineStill 800T.

| Scene | Resolve source | Frame / timecode | Main use |
| --- | --- | --- | --- |
| `kyoto_gate` | Clip 3 | Frame 488 / `00:00:16:08` | Fuji Superia, CineStill 50D daylight high-contrast exterior |
| `pagoda_street` | Clip 2 | Frame 320 / `00:00:10:20` | Agfa Vista daylight street, neutral walls, small skin sample |
| `night_neon` | Clip 8 | Frame 1968 / `00:01:05:18` | CineStill 800T night neon/practical highlights |

## Export Evidence

Identity references:

- `kyoto_gate__identity__20260609.png`
- `pagoda_street__identity__20260609.png`
- `night_neon__identity__20260609.png`

Preset exports:

- `kyoto_gate__fuji_superia_100__20260609.png`
- `kyoto_gate__fuji_superia_200__20260609.png`
- `kyoto_gate__fuji_superia_400__20260609.png`
- `kyoto_gate__fuji_superia_800__20260609.png`
- `kyoto_gate__fuji_superia_1600__20260609.png`
- `kyoto_gate__fuji_superia_hg_1600__20260609.png`
- `pagoda_street__agfa_vista_100__20260609.png`
- `pagoda_street__agfa_vista_200__20260609.png`
- `pagoda_street__agfa_vista_400__20260609.png`
- `night_neon__cinestill_800t__20260609.png`
- `kyoto_gate__cinestill_50d__20260609.png`

## Visual Decisions

| Group | Result | Judgment |
| --- | --- | --- |
| Fuji Superia 100/200/400/800/1600/HG 1600 | Pass, first round | Daylight red/orange and sky separation read clearly. Grain scales up as speed increases; 1600 and HG 1600 are visibly stronger but still intentional. No obvious hue relationship error, high-light bloom is not overextended on the selected frame. Skin is only a small/far sample, so close-up skin remains a risk. |
| Agfa Vista 100/200/400 | Pass with caution | Initial look was too strong in sky/highlight shoulder. After lowering procedural print amount, neutral walls and the small skin sample stay natural, and saturation is moderate. Scope/stat check still shows tight high-light headroom on the sky; watch this stock on other high-key scenes. |
| CineStill 800T | Pass, first round | Clip 8 night scene shows stronger color separation, visible grain, and more red-orange highlight behavior without lifting global haze too much. P99 high-light movement stayed controlled. Needs one more real tungsten-practical scene later, because this clip is mostly LED/neon signage. |
| CineStill 50D | Pass, first round | Daylight exterior keeps stable sky/red relationship with fine texture and mild warm highlight response. No matrix correction needed. |

## Scope / Stats Notes

PNG export checks were used alongside the Resolve monitor and waveform view when changes were subtle. Values below compare each effect export against its matching Identity reference.

| Preset | Key deltas |
| --- | --- |
| Fuji Superia 100/200/400 | Luma mean about `+2.9`; P99 `+2.13`; saturation from `-0.014` to `+0.022`; high-frequency proxy `+0.51` to `+0.65`. |
| Fuji Superia 800/1600/HG 1600 | Luma mean `+2.29` to `+2.86`; saturation `+0.047` to `+0.078`; high-frequency proxy `+1.21` to `+5.62`, matching faster stock grain. |
| Agfa Vista 100/200/400 | Luma mean `+4.13` to `+4.38`; P99 `+11.02`; near-white any-channel pixels remain about `+8.0` to `+8.4` points vs Identity. This is the main residual caution. |
| CineStill 800T | Luma mean `+2.97`; P99 `+0.07`; saturation `+0.143`; near-white any-channel pixels `+3.64`; high-frequency proxy `+2.60`. |
| CineStill 50D | Luma mean `+3.07`; P99 `+2.13`; saturation `-0.018`; high-frequency proxy `+0.50`. |

## Tuning Applied

All changes were procedural defaults. No film matrix or hue relationship edits were made.

- CineStill 800T:
  - `printAmount`: `0.26 -> 0.24`
  - `halationAmount`: `0.16 -> 0.22`
  - `halationThreshold`: `0.83 -> 0.80`
  - `halationRadius`: `0.50 -> 0.52`
  - `halationLocal`: `0.42 -> 0.50`
  - `halationGlobal`: `0.07 -> 0.06`
  - `halationImpact`: `0.30 -> 0.36`
- Agfa Vista:
  - `printAmount`: `0.28 -> 0.22 -> 0.18`

## Verification

- Rebuilt and installed the OFX after procedural default changes.
- Final installed plugin hash: `79AAD6E62C1BDE9614DB5C4EFBA71EE45DF52D084096F509BBF9AA80F7E502E2`.
- Resolve project saved after the final export pass.
- Unit command for this handoff: `python -m unittest discover -s tests -q` -> `Ran 53 tests`, `OK`.

## Remaining Risk / Next Step

- Recheck Fuji Superia and Agfa Vista on a closer daylight skin shot; current frames only include distant or small skin samples.
- Recheck Agfa Vista on another bright sky/high-key scene. If it still reads too shoulder-heavy, lower procedural `printAmount` again before touching any matrix.
- Add one tungsten-heavy night practical scene for CineStill 800T; clip 8 validates neon/signage behavior well but does not fully cover tungsten bulbs.
- Continue using waveform/vectorscope or PNG stats for subtle procedural tweaks, especially high-light shoulder and saturation changes.
