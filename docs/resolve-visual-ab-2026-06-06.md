# Resolve visual A/B - 2026-06-06

## Purpose

This checklist turns the latest stock-specific defaults into repeatable Resolve verification. The goal is to compare each tuned preset against `Identity` on the same frame, then judge whether the visible result matches the stock intent without creating obvious color-management or texture artifacts.

Safety boundary: do not automate activation, licensing, crack-related, or account screens. If Resolve opens to an activation screen, stop and ask the user to open a usable project.

## Current stock groups

| Group | Presets | Baseline | Required scene | Inspect |
| --- | --- | --- | --- | --- |
| Fuji Superia | Fuji Superia 100, 200, 400, 800, 1600, HG 1600 | Identity | Daylight skin, foliage, blue sky, and one shadow area | Enhanced color, greens/blues, consumer-negative grain scaling, no waxy skin |
| Agfa Vista | Agfa Vista 100, 200, 400 | Identity | Daylight skin, neutral wall, mixed color objects, and fine detail | Moderate saturation, fine grain, broad exposure latitude, natural skin |
| CineStill 800T | CineStill 800T | Identity | Night tungsten/neon practicals with clipped point highlights | Cool tungsten balance, visible red-orange halation, controlled global haze |
| CineStill 50D | CineStill 50D | Identity | Daylight high-contrast exterior with specular highlights | Very fine grain, mild warm halation in highlights, daylight color stability |

## Capture procedure

1. Use the installed OFX build whose SHA256 matches the current restart handoff.
2. Open a Resolve project that already contains usable test footage.
3. For each scene, export one `Identity` frame before enabling a stock preset.
4. Apply one preset at a time, reset custom overrides unless the test specifically says otherwise, and export the matching frame.
5. Name frames as `<scene>__<preset>__YYYYMMDD.png`.
6. Put the exported frames in `docs/visual-ab/2026-06-06/` or a dated sibling folder.

## Evidence sources used for default targets

- Fuji Superia X-TRA 400: Fujifilm describes smooth fine grain, enhanced color, vibrant but natural reproduction, sharpness, and gray balance: <https://www.fujifilm.com/ca/en/consumer/films/consumer-film/superia-400>
- CineStill remjet behavior: CineStill documents remjet as protective against light piping, scratches, static, and halation, and states CineStill films do not have remjet backing: <https://help.cinestillfilm.com/hc/en-us/articles/360028874012-What-is-Remjet>
- CineStill 800T: CineStill lists remjet-free C-41 processing, slight halation, tungsten balance, and low-light use: <https://cinestillfilm.com/collections/800t-filmfamily/products/800tungsten-high-speed-color-film-120-format-retail>
- CineStill 50D: CineStill states 50D can show almost no grain, and lower exposure index can produce warmer highlights with greater halation: <https://help.cinestillfilm.com/hc/en-us/articles/360029186071-How-do-I-rate-CineStill-50Daylight>
- Agfa Vista plus 200 reference: B&H's archived product copy describes daylight-balanced C-41, excellent grain quality and sharpness, wide exposure latitude, and natural skin reproduction: <https://www.bhphotovideo.com/c/product/1038095-REG/agfa_1175206_vista_plus_200_35mm.html/specs>

## Acceptance notes

- Passing this checklist requires visual exports, not only unit tests.
- The current code/build verification can prove that defaults are wired and installed, but it cannot prove the look is finished.
- If any preset appears too strong, the next edit should lower the procedural default first, not change the matrix unless hue relationships are visibly wrong.
