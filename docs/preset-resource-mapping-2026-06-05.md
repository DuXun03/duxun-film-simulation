# Preset Resource Mapping - 2026-06-05

## Scope

This document maps the 54 embedded OFX presets to the available `presets/film_stocks` resources. The plugin still uses embedded matrices and procedural controls at render time; it does not load `.cube` files at runtime yet.

## Current implementation

Exact or directly reused stock matrices:

| OFX preset | Resource basis | Implementation |
| --- | --- | --- |
| Agfa Vista 100 / 200 / 400 | Agfa Vista 100 | `kMatAgfaVista100` |
| CineStill 50D | Kodak Vision3 50D 5203 style matrix | `kMatCineStill50D` |
| CineStill 800T | Kodak Vision3 500T 5219 style matrix | `kMatCineStill800T` |
| Fuji Pro 160C | Fuji Pro 160C | `kMatFujiPro160C` |
| Fuji Pro 400H | Fuji Pro 400H | `kMatFujiPro400H` |
| Fuji Pro 800Z | Fuji Pro 400H near neighbor plus tuned high-speed color-negative defaults | `kMatFujiPro400H` + `applyHighValueColorStockDefaults` |
| Fuji Superia 100 / 400 / 800 | Fuji Superia X-Tra 400 family | `kMatFujiSuperia` |
| Fuji Superia 200 | Fuji C200 | `kMatFujiC200` |
| Fuji Superia 1600 / Superia HG 1600 | Fuji Natura 1600 near neighbor | `kMatFujiNatura1600` |
| Fuji Astia 100F | Fuji Provia 100F near neighbor plus tuned soft-slide defaults | `kMatFujiProvia100F` + `applyHighValueColorStockDefaults` |
| Fuji Fortia SP 50 | Fuji Velvia 50 near neighbor plus tuned high-saturation slide defaults | `kMatFujiVelvia50` + `applyHighValueColorStockDefaults` |
| Fuji Provia 100F / 400F / 400X | Fuji Provia 100F | `kMatFujiProvia100F` |
| Fuji Sensia 100 | Fuji Provia 100F near neighbor | `kMatFujiProvia100F` |
| Fuji Velvia 100 / 50 | Fuji Velvia 50 | `kMatFujiVelvia50` |
| Kodak ColorPlus 200 | Kodak Gold 200 near neighbor plus consumer-negative defaults | `kMatKodakGold200` + `applyKodakColorNegativeDefaults` |
| Kodak Ektar 100 | Kodak Ektar 100 plus fine-grain saturated negative defaults | `kMatKodakEktar100` + `applyKodakColorNegativeDefaults` |
| Kodak Gold 200 | Kodak Gold 200 plus consumer-negative defaults | `kMatKodakGold200` + `applyKodakColorNegativeDefaults` |
| Kodak Portra 160 variants | Kodak Portra 160 plus low-grain portrait-negative defaults | `kMatKodakPortra160` + `applyKodakColorNegativeDefaults` |
| Kodak Portra 400 variants | Kodak Portra 400 plus medium-grain portrait-negative defaults | `kMatKodakPortra400` + `applyKodakColorNegativeDefaults` |
| Kodak Portra 800 | Kodak Portra 800 plus high-speed portrait-negative defaults | `kMatKodakPortra800` + `applyKodakColorNegativeDefaults` |
| Kodak Ultra Max 400 | Kodak Ultramax 400 plus consumer high-speed defaults | `kMatKodakUltramax400` + `applyKodakColorNegativeDefaults` |
| Kodak Ektachrome 100 G/GX/VS and Elite Chrome 400 | Kodak Ektachrome 100D near neighbor | `kMatKodakEktachrome100D` |
| Kodak Elite Chrome 400 | Kodak Ektachrome 100D near neighbor plus tuned high-speed slide defaults | `kMatKodakEktachrome100D` + `applyHighValueColorStockDefaults` |
| Kodachrome 25 / 64 / 200 | Kodachrome 64 | `kMatKodachrome64` |

Black-and-white presets use stock-specific procedural defaults rather than color matrices:

| Preset family | Current behavior |
| --- | --- |
| Agfa APX / Fuji Acros / Ilford Delta 100 / Ilford FP4 | fine-grain B&W defaults |
| Kodak Tri-X / Ilford HP5 | classic mid-speed B&W defaults |
| Kodak T-Max 3200 / Ilford Delta 3200 / Fuji Neopan 1600 | high-speed coarse-grain defaults |
| Ilford XP2 | smoother chromogenic B&W defaults |

Kodak color-negative presets now combine the stock matrix with procedural defaults for print amount, color density, vibrance, grain resolution/chroma, and subtle or disabled halation. The matrix sets the broad dye-response direction; `applyKodakColorNegativeDefaults` makes Portra, Ektar, Gold/ColorPlus, and Ultramax behave differently in the user-facing controls.

## Unresolved calibration

These presets still need visual A/B calibration against reference material or better source data:

- Fuji Pro 800Z, now using a Pro 400H near-neighbor matrix and high-speed color-negative defaults
- Fuji Astia 100F, now using a Provia near-neighbor matrix and soft-slide defaults
- Fuji Fortia SP 50, now using a Velvia near-neighbor matrix and high-saturation slide defaults
- Kodak Portra, Ektar, Gold, ColorPlus, and Ultramax, now using stock matrices plus differentiated color-negative defaults
- Kodak Elite Chrome 400, now using an Ektachrome near-neighbor matrix and high-speed slide defaults
- All exact negative/print pairing behavior beyond the current embedded matrix plus print-stock model

## Future work

1. Add a real 3D LUT sampler only after the OFX resource packaging and licensing boundary are clear.
2. Keep `.cube` resources as source material until runtime loading, caching, and install packaging are designed.
3. Add Resolve A/B screenshots for each brand group before marking visual calibration complete.
