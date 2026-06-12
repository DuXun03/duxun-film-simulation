# Resolve visual A/B - 2026-06-11

## Scope

Second Resolve A/B pass for the residual risks left after `docs/resolve-visual-ab-2026-06-09.md`.

- Resolve project: `Untitled Project 2`
- Timeline: `Timeline 1`
- OFX: DuXun Film Simulation, OpenFX v5.0 path
- Export folder: `docs/visual-ab/2026-06-11/`
- Not started: HDR, OpenCL, Metal, new GPU expansion, LUT runtime loading

Reference for the CineStill correction: CineStill's remjet notes document that remjet backing suppresses halation, and CineStill 800T is a remjet-free tungsten-balanced stock. The second 800T pass therefore treated red/orange practical-light halation as required, not optional.

- CineStill help: <https://help.cinestillfilm.com/hc/en-us/articles/360028874012-What-is-Remjet>
- CineStill 800T product page: <https://cinestillfilm.com/collections/800t-filmfamily/products/800tungsten-high-speed-color-film-120-format-retail>

## Captures

| Scene | Timeline clip | Source | Frame / timecode | Presets exported |
| --- | ---: | --- | --- | --- |
| `skin_daylight_closeup` | 9 | `01_daylight_skin_closeup_pexels_8531896.mp4` | 2909 / `00:01:36:29` | Identity, Agfa Vista 100/200/400, Fuji Superia 100/200/400/800/1600/HG 1600 |
| `highkey_sky_city` | 10 | `02_highkey_sky_city_buildings_pexels_14569464.mp4` | 3986 / `00:02:12:26` | Identity, Agfa Vista 100/200/400 |
| `warm_restaurant_practical` | 11 | `03_tungsten_warm_restaurant_interior_pexels_31631562.mp4` | 4457 / `00:02:28:17` | Identity, CineStill 800T |
| `night_foodtruck_practical` | 12 | `04_night_warm_food_truck_practical_pexels_5705721.mp4` | 4960 / `00:02:45:10` | Identity, CineStill 800T |

Export note: `warm_restaurant_practical` was re-run and visually checked after the first identity refresh issue from the previous food-truck frame. The final identity/effect pair is the restaurant frame. CineStill exports were re-run after the CUDA halation fix and exported serially, because parallel Resolve scripting shares one timeline state.

## Visual Decision

| Preset family | Risk checked | Result | Decision |
| --- | --- | --- | --- |
| Fuji Superia 100/200/400/800/1600/HG 1600 | Daylight close-up skin: waxy skin, red cast, yellow-green cast, grain naturalness | Skin stays warm and photographic without waxy smoothing. No obvious red push or yellow-green contamination. Grain scales up visibly on 800/1600/HG 1600, but it reads as film grain rather than plastic texture loss. | Freeze visual baseline |
| Agfa Vista 100/200/400 | Daylight close-up skin and bright high-key sky | Skin remains natural with a mild warm move. High-key sky keeps gradient and skyline separation. Shoulder is visible but not heavy; no all-channel white clipping in the sky region. | Freeze visual baseline |
| CineStill 800T | Tungsten-heavy practical night scene: red-orange halation and global haze | Restaurant and food-truck frames now show credible red-orange practical glow around warm lamps, signage, and window highlights. The previous white/yellow-white halo failure was fixed in both CPU and CUDA paths. Shadow floor is not lifted into haze; the image stays contrasty with localized halation. | Freeze visual baseline |

## Stats Notes

Stats are only an aid; visual inspection remains the pass/fail source.

- Skin ROI hue moves by about `+2.1` to `+3.0` degrees versus identity across Agfa/Fuji. Saturation rises moderately, with preserved or increased texture/grain; no texture drop suggesting waxy smoothing.
- Fuji high ISO skin texture ratios versus identity were above `8x` for Superia 800 and above `24x` for 1600/HG 1600, confirming the grain is visible rather than smoothed away.
- Agfa high-key sky p99 luma moved from `216.63` identity to about `232.77`; any-channel clipping appears in blue highlights, but all-channel near-white clipping stayed `0.000%`.
- CineStill 800T ring deltas around bright practicals are now strongly red/orange. Food-truck: `1096` samples, `100.0%` red-dominant, `98.7%` red/orange, median `dR-dG +63`, median `dR-dB +89`. Restaurant: `189` samples, `98.9%` red-dominant, `97.4%` red/orange, median `dR-dG +62`, median `dR-dB +79`.
- CineStill 800T shadow p05 moved down, not up: food-truck `-8.50`, restaurant `-6.99`, so the fix did not introduce global milky haze.

## Tuning

CineStill 800T required tuning after user validation showed the halo was still white rather than red/orange.

- CPU glow mask now stores red/orange leakage for halation instead of blurring the original white RGB highlight.
- CUDA glow mask and CUDA composite now match the CPU red/orange halation model, including blue/green suppression.
- CineStill 800T procedural defaults were adjusted: `halationAmount 0.34`, `threshold 0.74`, `radius 0.60`, `warmth 0.96`, `local 0.72`, `global 0.025`, `hue 0.72`, `blueComp 0.86`, `impact 0.58`, `defringe 0.42`.
- A render-time CineStill 800T guard keeps old Resolve nodes from retaining the previous white-halo defaults.
- No matrix changes.

## Verification

`python -m unittest discover -s tests -q`

```text
Ran 56 tests in 0.339s
OK
```

Build and installed OFX SHA256 match:

```text
build\DuXunFilmSim.ofx
A508CA29788705D29D78E046FB3DF7D2144C29ED32BBE6CC6662DA2063F48062

C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx
A508CA29788705D29D78E046FB3DF7D2144C29ED32BBE6CC6662DA2063F48062
```

Final installed OFX SHA256:

```text
A508CA29788705D29D78E046FB3DF7D2144C29ED32BBE6CC6662DA2063F48062
```

## Final Freeze Call

- Fuji Superia: visual baseline frozen after the second Resolve A/B pass.
- Agfa Vista: visual baseline frozen after the second Resolve A/B pass.
- CineStill 800T: visual baseline frozen after the second Resolve A/B pass and the CPU/CUDA red-orange halation fix.
- CineStill 50D: remains in the first-round daylight exterior pass state from `docs/resolve-visual-ab-2026-06-09.md`; it was not reopened in this second pass.
