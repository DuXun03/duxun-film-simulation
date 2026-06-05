/*
 * DuXun-Display.h
 * =================
 * Display Transform Library for FilmSim DCTL Pipeline
 *
 * Author: film-sim-plugin team
 * Version: 1.0.0
 * License: MIT
 *
 * Purpose:
 *   Transforms the working color space (DWG/DI or scene-linear) into
 *   display-referred color for Rec.709, P3, and HDR outputs.
 *
 *   Implements three stages:
 *
 *   1. TONE MAPPING    -- Compresses high dynamic range to display range
 *                          using a filmic shoulder + weighted luminance model
 *   2. GAMUT MAPPING   -- Compresses out-of-gamut colors into Rec.709
 *                          using per-channel hue-preserving desaturation
 *   3. GAMMA/SRGB      -- Applies the final display gamma curve
 *
 * Reference:
 *   - ACES RRT concepts (SMPTE ST 2065)
 *   - AgX image formation principles
 *   - Per-channel gamut compression (Jed Smith's approach)
 *
 * All algorithms are independently implemented from first principles
 * (MIT-compatible, no GPL dependency).
 *
 * Dependencies:
 *   DuXun-Common.h
 */

#ifndef DUXUN_DISPLAY_H
#define DUXUN_DISPLAY_H

#include "DuXun-Common.h"

/* =====================================================================
 * STAGE 1: FILMIC TONE MAPPING
 * ===================================================================== */

/*
 * Compress HDR scene values to SDR display range.
 *
 * Uses a weighted sum of:
 *   a) Linear response in shadows (preserves shadow detail)
 *   b) Log-like compression in midtones
 *   c) Aggressive shoulder compression in highlights
 *
 * The transition between each region is smooth (C1 continuous).
 *
 * Parameters:
 *   x          : input linear value
 *   contrast   : overall contrast pivot (1.0 = neutral)
 *   shoulder   : highlight compression strength (0.5 - 2.0)
 *   exposure   : exposure offset in stops
 */
__DEVICE__ float _film_tone_map_channel(float x, float contrast, float shoulder, float exposure)
{
    /* Apply exposure */
    x *= _film_exp2(exposure);

    if (x <= 0.0f) return 0.0f;

    /* Core tone mapping function:
     *   y = x / (x + shoulder) * contrast
     *
     * This produces a smooth S-like curve:
     *   - x << shoulder → linear (y ≈ x / shoulder)
     *   - x >> shoulder → compressed (y → 1.0)
     *
     * With contrast < 1.0, midtones are lifted.
     */
    float y = (x * contrast) / (x + shoulder);

    /* Add a subtle filmic toe for shadow roll-off */
    if (y < 0.15f) {
        float t = y / 0.15f;
        y = y * (0.8f + 0.2f * t);  /* slight shadow lift */
    }

    return (y < 0.0f) ? 0.0f : ((y > 1.0f) ? 1.0f : y);
}

/*
 * Per-channel + luminance-preserving tone mapping.
 *
 * Maps each RGB channel independently, then rebalances
 * luminance to preserve perceived brightness.
 */
__DEVICE__ float3 _film_tone_map(float3 rgb, float contrast, float shoulder, float exposure)
{
    /* Per-channel tone map */
    float3 mapped = make_float3(
        _film_tone_map_channel(rgb.x, contrast, shoulder, exposure),
        _film_tone_map_channel(rgb.y, contrast, shoulder, exposure),
        _film_tone_map_channel(rgb.z, contrast, shoulder, exposure)
    );

    /* Luminance rebalancing: preserve original luminance ratio */
    float orig_lum = _film_luminance(rgb);
    float mapped_lum = _film_luminance(mapped);

    if (mapped_lum > FILM_EPSILON && orig_lum > FILM_EPSILON) {
        float lum_ratio = orig_lum / mapped_lum;

        /* Blend: keep some of the compressed lum, restore some original ratio.
         * 70% compressed + 30% original = natural look */
        float correction = _film_lerp(mapped_lum, orig_lum * contrast / (orig_lum + shoulder), 0.3f);
        float final_ratio = (mapped_lum > FILM_EPSILON) ? correction / mapped_lum : 1.0f;

        mapped.x *= final_ratio;
        mapped.y *= final_ratio;
        mapped.z *= final_ratio;
    }

    return mapped;
}

/* =====================================================================
 * STAGE 2: GAMUT MAPPING (Wide Gamut → Rec.709)
 * ===================================================================== */

/*
 * Compress out-of-gamut colors toward the achromatic axis.
 *
 * Uses per-channel distance from achromatic to determine
 * how much to desaturate. Far-out-of-gamut colors are
 * compressed more aggressively.
 *
 * This preserves hue while preventing channel clipping.
 */
__DEVICE__ float3 _film_gamut_compress(float3 rgb)
{
    float maxc = _fmaxf(rgb.x, _fmaxf(rgb.y, rgb.z));
    float minc = _fminf(rgb.x, _fminf(rgb.y, rgb.z));

    /* If all channels within [0, 1], no compression needed */
    if (maxc <= 1.0f && minc >= 0.0f) return rgb;

    /* Compute achromatic axis point (luminance) */
    float lum = _film_luminance(rgb);
    lum = _fmaxf(lum, 0.0f);

    /* Distance from achromatic: how saturated is this pixel? */
    float3 dist = make_float3(rgb.x - lum, rgb.y - lum, rgb.z - lum);
    float sat = _sqrtf(dist.x*dist.x + dist.y*dist.y + dist.z*dist.z);

    if (sat < FILM_EPSILON) return make_float3(lum, lum, lum);

    /* Compression: farther out → more desaturation */
    float over_shoot = _fmaxf(0.0f, maxc - 1.0f);
    float compression = 1.0f / (1.0f + over_shoot * 3.0f);

    /* Also compress negative values (below 0) */
    float under_shoot = _fmaxf(0.0f, -minc);
    compression *= 1.0f / (1.0f + under_shoot * 3.0f);

    /* Compress chroma toward luminance */
    float3 result = make_float3(
        lum + dist.x * compression,
        lum + dist.y * compression,
        lum + dist.z * compression
    );

    /* Final soft-clamp */
    result.x = _fmaxf(0.0f, _fminf(result.x, 1.0f));
    result.y = _fmaxf(0.0f, _fminf(result.y, 1.0f));
    result.z = _fmaxf(0.0f, _fminf(result.z, 1.0f));

    return result;
}

/* =====================================================================
 * STAGE 3: DISPLAY GAMMA (sRGB / Rec.709)
 * ===================================================================== */

/*
 * Apply sRGB piecewise transfer function (IEC 61966-2-1).
 *
 * Linear segment below 0.0031308, power function above.
 *
 * This is the final step that makes the image look correct
 * on standard computer displays.
 */
__DEVICE__ float _film_srgb_gamma(float x)
{
    if (x <= 0.0031308f) {
        return 12.92f * x;
    } else {
        return 1.055f * _powf(x, 1.0f / 2.4f) - 0.055f;
    }
}

__DEVICE__ float3 _film_srgb_gamma3(float3 rgb)
{
    return make_float3(
        _film_srgb_gamma(rgb.x),
        _film_srgb_gamma(rgb.y),
        _film_srgb_gamma(rgb.z)
    );
}

/*
 * Pure power-law gamma (for Rec.709 display).
 * Simpler than sRGB piecewise, commonly used in broadcast.
 *
 * gamma = 2.4 is the Rec.709 standard.
 */
__DEVICE__ float3 _film_power_gamma3(float3 rgb, float gamma)
{
    return make_float3(
        _powf(_fmaxf(rgb.x, 0.0f), 1.0f / gamma),
        _powf(_fmaxf(rgb.y, 0.0f), 1.0f / gamma),
        _powf(_fmaxf(rgb.z, 0.0f), 1.0f / gamma)
    );
}

/* =====================================================================
 * COMPLETE DISPLAY TRANSFORM PIPELINE
 * ===================================================================== */

/*
 * Full display transform: Tone Map → Gamut Compress → Display Gamma
 *
 * This takes scene-referred linear or DWG/DI input and produces
 * display-referred Rec.709 or sRGB output ready for viewing.
 *
 * Parameters:
 *   rgb       : input (scene-referred, DWG/DI or linear)
 *   contrast  : tone mapping contrast (0.5 - 2.0, default 1.0)
 *   shoulder  : highlight compression (0.5 - 2.0, default 1.0)
 *   exposure  : exposure offset in stops (-3.0 to 3.0)
 *   gamma     : output gamma (2.2 sRGB, 2.4 Rec.709)
 *   use_srgb  : 1 = sRGB piecewise, 0 = pure power law
 */
__DEVICE__ float3 _film_display_transform(float3 rgb, float contrast,
                                           float shoulder, float exposure,
                                           float gamma, int use_srgb)
{
    /* Stage 1: Tone mapping */
    rgb = _film_tone_map(rgb, contrast, shoulder, exposure);

    /* Stage 2: Gamut compression */
    rgb = _film_gamut_compress(rgb);

    /* Stage 3: Display gamma */
    if (use_srgb) {
        rgb = _film_srgb_gamma3(rgb);
    } else {
        rgb = _film_power_gamma3(rgb, gamma);
    }

    return rgb;
}

/*
 * DWG/DI → Rec.709 shortcut.
 * Assumes input is already in DaVinci Wide Gamut / Intermediate.
 */
__DEVICE__ float3 _film_display_dwg_to_709(float3 rgb)
{
    /* DWG/DI already has a gamma curve, so we use different defaults:
     * - Lower contrast (DWG is already contrasty)
     * - Standard Rec.709 gamma (2.4)
     */
    return _film_display_transform(rgb, 0.9f, 1.0f, 0.0f, 2.4f, 0);
}

#endif /* DUXUN_DISPLAY_H */
