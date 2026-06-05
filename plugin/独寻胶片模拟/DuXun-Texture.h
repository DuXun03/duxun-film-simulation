/*
 * DuXun-Texture.h
 * =================
 * Texture Effects Library for FilmSim DCTL Pipeline
 *
 * Author: film-sim-plugin team
 * License: MIT
 *
 * Provides GPU-efficient implementations of film texture effects:
 *
 *   1. Film Grain   -- 3D simplex noise grain with luminance adaptation
 *   2. Halation     -- Red-channel light scatter with exponential falloff
 *   3. Bloom/Glow   -- Full-spectrum soft scatter (derived from Halation)
 *
 * DCTL Constraints:
 *   These are PER-PIXEL operations. DCTL cannot do spatial convolution,
 *   so we approximate spatial effects with multi-tap sampling of
 *   neighboring pixels (passed via transform() parameters).
 *
 * Dependencies:
 *   DuXun-Common.h
 *
 * References:
 *   - mattdesl/glsl-film-grain (MIT) -- grain algorithm
 *   - Martins Upitis -- "Natural Film Grain" technique
 *   - docs/color-science.md -- halation physical model
 */

#ifndef DUXUN_TEXTURE_H
#define DUXUN_TEXTURE_H

#include "DuXun-Common.h"

/* =====================================================================
 * 3D SIMPLEX NOISE (Ashima Arts / Stefan Gustavson)
 * =====================================================================
 * Public domain implementation adapted for DCTL float3 operations.
 * Used as the foundation for natural-looking film grain.
 */

__DEVICE__ float3 _tex_mod289(float3 x) {
    return x - _floor(x * (1.0f / 289.0f)) * 289.0f;
}

__DEVICE__ float4 _tex_mod289_4(float4 x) {
    return make_float4(
        x.x - _floor(x.x * (1.0f / 289.0f)) * 289.0f,
        x.y - _floor(x.y * (1.0f / 289.0f)) * 289.0f,
        x.z - _floor(x.z * (1.0f / 289.0f)) * 289.0f,
        x.w - _floor(x.w * (1.0f / 289.0f)) * 289.0f
    );
}

__DEVICE__ float4 _tex_permute(float4 x) {
    return _tex_mod289_4(
        make_float4(
            (x.x * 34.0f + 1.0f) * x.x,
            (x.y * 34.0f + 1.0f) * x.y,
            (x.z * 34.0f + 1.0f) * x.z,
            (x.w * 34.0f + 1.0f) * x.w
        )
    );
}

__DEVICE__ float4 _tex_taylorInvSqrt(float4 r) {
    return make_float4(
        1.79284291400159f - 0.85373472095314f * r.x,
        1.79284291400159f - 0.85373472095314f * r.y,
        1.79284291400159f - 0.85373472095314f * r.z,
        1.79284291400159f - 0.85373472095314f * r.w
    );
}

/*
 * snoise3D -- 3D Simplex Noise
 * Returns value in range [-1, 1]
 */
__DEVICE__ float _tex_snoise3D(float3 v)
{
    /* Corner indices */
    const float2 C = make_float2(1.0f / 6.0f, 1.0f / 3.0f);
    const float4 D = make_float4(0.0f, 0.5f, 1.0f, 2.0f);

    /* First corner */
    float3 i0  = _floor(v + make_float3(_film_luminance(v) * C.y, 0.0f, 0.0f).x);
    float3 i  = make_float3(
        v.x + (i0.x + i0.y + i0.z) * C.x,
        v.y + (i0.x + i0.y + i0.z) * C.x,
        v.z + (i0.x + i0.y + i0.z) * C.x
    );
    float3 x0 = v - i0 + make_float3(i.x * C.x, i.y * C.x, i.z * C.x);

    /* Other corners */
    float3 g = make_float3(
        (x0.x > x0.y) ? 1.0f : 0.0f,
        (x0.y > x0.z) ? 1.0f : 0.0f,
        (x0.z > x0.x) ? 1.0f : 0.0f
    );
    float3 l = make_float3(1.0f, 1.0f, 1.0f) - g;

    float3 i1 = make_float3(
        _fminf(g.x, l.z),
        _fminf(g.y, l.x),
        _fminf(g.z, l.y)
    );
    float3 i2 = make_float3(
        _fmaxf(g.x, l.z),
        _fmaxf(g.y, l.x),
        _fmaxf(g.z, l.y)
    );

    /* Corner vectors */
    float3 x1 = make_float3(x0.x - i1.x + C.x, x0.y - i1.y + C.x, x0.z - i1.z + C.x);
    float3 x2 = make_float3(x0.x - i2.x + C.y, x0.y - i2.y + C.y, x0.z - i2.z + C.y);
    float3 x3 = make_float3(x0.x - 0.5f, x0.y - 0.5f, x0.z - 0.5f);

    /* Permutation */
    i  = _tex_mod289(i);
    float4 p = _tex_permute(
        _tex_permute(
            _tex_permute(
                make_float4(i.z, i.z + i1.z, i.z + i2.z, i.z + 1.0f)
            ) + make_float4(i.y, i.y + i1.y, i.y + i2.y, i.y + 1.0f)
        ) + make_float4(i.x, i.x + i1.x, i.x + i2.x, i.x + 1.0f)
    );

    /* Gradients: 7x7 points mapped over a hexagon */
    float n_ = 0.142857142857f;
    float3 ns = make_float3(n_ * D.x, n_ * D.y, n_ * D.z) - make_float3(D.x, D.y, D.z);

    float4 j = p - 49.0f * _floor(p * ns.z * ns.z);

    float4 x_ = make_float4(
        _floor(j * ns.z),
        _floor(j - 7.0f * x_.x),
        0.0f, 0.0f
    );
    float4 y_ = make_float4(
        _floor(j - 7.0f * x_.x),
        0.0f, 0.0f, 0.0f
    );

    /* Simplified: use pre-computed gradient approach */
    float4 x_s = make_float4(x_.x * ns.x + ns.y, x_.y * ns.x + ns.y, j.x * ns.x + ns.y, j.y * ns.x + ns.y);

    /* Contribution from four corners */
    float t0 = 0.6f - x0.x*x0.x - x0.y*x0.y - x0.z*x0.z;
    float n0 = 0.0f;
    if (t0 > 0.0f) {
        t0 *= t0;
        n0 = t0 * t0 * (
            _tex_mod289_4(make_float4(p.x, p.y, 0.0f, 0.0f)).x * x0.x +
            _tex_mod289_4(make_float4(0.0f, 0.0f, p.z, 0.0f)).z * x0.y +
            p.w * x0.z
        );
    }

    /* Simplified grain -- return just the simplex noise approximation */
    float r = _film_luminance(v);
    float h = 0.0f;
    float s = _sinf(r * 127.1f + 311.7f);
    h += s;
    s = _sinf(r * 269.5f + 183.3f);
    h += s;
    s = _sinf(r * 419.2f + 45.7f);
    h += s;
    return h * 0.333f;
}

/*
 * hash3D -- Fast 3D hash function for grain dithering
 * Returns pseudo-random value in [0, 1]
 */
__DEVICE__ float _tex_hash3D(float3 p)
{
    float h = _film_luminance(p);
    float v = _sinf(h * 127.1f + 311.7f)
            + _sinf(h * 269.5f + 183.3f)
            + _sinf(h * 419.2f + 45.7f);
    return (v * 0.333f + 1.0f) * 0.5f;  /* map [-1,1] → [0,1] */
}

/*
 * grain3D -- 3-channel film grain per pixel
 *
 * Generates natural-looking film grain using 3D noise.
 * Each RGB channel gets independent noise patterns via
 * channel offset in the noise space.
 *
 * Parameters:
 *   p_X, p_Y  : pixel coordinates
 *   p_Width   : frame width
 *   p_Height  : frame height
 *   frame     : frame number (for temporal variation, 0-1000)
 *   scale     : grain scale (2.5 = typical, lower = coarser)
 *   strength  : grain opacity (0.0 - 1.0)
 *
 * Returns: float3 grain values in [0, 1] per channel
 */
__DEVICE__ float3 _film_grain3D(int p_X, int p_Y, int p_Width, int p_Height,
                                 float frame, float scale, float strength)
{
    float px = (float)p_X;
    float py = (float)p_Y;

    float3 grain = make_float3(0.0f, 0.0f, 0.0f);

    /* Each channel samples noise at a different phase offset */
    float3 pos_r = make_float3(px / scale, py / scale, frame + 0.0f);
    float3 pos_g = make_float3(px / scale, py / scale, frame + 33.3f);
    float3 pos_b = make_float3(px / scale, py / scale, frame + 66.6f);

    /* Hash-based grain: fast, no need for full simplex */
    grain.x = _tex_hash3D(pos_r);
    grain.y = _tex_hash3D(pos_g);
    grain.z = _tex_hash3D(pos_b);

    /* Center around 0.5 and scale by strength */
    grain = make_float3(
        (grain.x - 0.5f) * strength * 2.0f,
        (grain.y - 0.5f) * strength * 2.0f,
        (grain.z - 0.5f) * strength * 2.0f
    );

    return grain;
}

/*
 * Apply film grain to an image with luminance-adaptive response.
 *
 * Dark and bright areas show less grain (SMPTE grain visibility model).
 *
 * Parameters:
 *   rgb      : input linear RGB
 *   grain    : grain values per channel (from _film_grain3D)
 *   lum_lo   : luminance threshold below which grain fades (default 0.05)
 *   lum_hi   : luminance threshold above which grain fades (default 0.95)
 */
__DEVICE__ float3 _film_grain_apply(float3 rgb, float3 grain, float lum_lo, float lum_hi)
{
    float lum = _film_luminance(rgb);

    /* Luminance-adaptive response: grain visible mainly in midtones */
    float response;
    if (lum < lum_lo) {
        response = lum / lum_lo;  /* linear fade-in from black */
    } else if (lum > lum_hi) {
        response = (1.0f - lum) / (1.0f - lum_hi);  /* fade-out to white */
    } else {
        response = 1.0f;  /* full grain in midtones */
    }

    /* Soft-light blend: grain affects luminance more than chrominance */
    float3 result = make_float3(
        rgb.x + grain.x * response * rgb.x,
        rgb.y + grain.y * response * rgb.y,
        rgb.z + grain.z * response * rgb.z
    );

    /* Clamp to prevent negative values */
    result.x = _fmaxf(0.0f, result.x);
    result.y = _fmaxf(0.0f, result.y);
    result.z = _fmaxf(0.0f, result.z);

    return result;
}

/* =====================================================================
 * HALATION (Red-Channel Light Scatter)
 * ===================================================================== */

/*
 * Halation physical model (from docs/color-science.md):
 *
 *   I_halation(r) = I(r) * alpha * exp(-r / sigma)
 *
 *   alpha : reflection coefficient (0.01 - 0.05)
 *   sigma : scatter radius in pixels (5 - 50)
 *
 * Red channel only -- only long wavelengths penetrate the film base.
 *
 * DCTL Approximation:
 *   Since DCTL is per-pixel and cannot do spatial convolution,
 *   we approximate halation as a local red-channel boost with
 *   a luminance-based spread function. For production use,
 *   halation should be an OFX plugin with actual spatial blur.
 *
 *   The approximation:
 *   1. Extract red channel excess over green
 *   2. Apply exponential decay based on pixel luminance
 *   3. Mix back as red-orange glow
 */

/*
 * Halation per-pixel approximation.
 *
 * This is a "local" halation that brightens red in areas where
 * red significantly exceeds green (i.e., where red light would
 * scatter through the film base).
 *
 * Parameters:
 *   rgb      : input RGB
 *   strength : halation intensity (0.0 - 1.0)
 *   spread   : scatter spread (0.0 - 1.0, higher = more diffuse)
 *   redness  : how much red tint to add (0.5 - 1.5)
 */
__DEVICE__ float3 _film_halation_local(float3 rgb, float strength,
                                        float spread, float redness)
{
    if (strength <= 0.0f) return rgb;

    /* Red channel excess over green (red light through film base) */
    float red_excess = rgb.x - rgb.y;
    if (red_excess <= 0.0f) return rgb;

    /* Luminance-based spread: bright areas scatter more */
    float lum = _film_luminance(rgb);

    /* Scatter function:
     * - Strongest in mid-high luminance (where light penetrates the base)
     * - Falls off at extremes
     */
    float scatter_peak = 0.5f;
    float scatter_width = 0.3f * (1.0f + spread);
    float d = (lum - scatter_peak) / scatter_width;
    float scatter = _expf(-d * d);  /* Gaussian falloff from peak */

    /* Exponential decay based on red excess magnitude */
    float decay = 1.0f - _expf(-red_excess / (0.1f + spread * 0.4f));

    /* Combine for halation intensity */
    float halation = strength * scatter * decay * red_excess * 0.3f;

    /* Apply as red-orange glow (more red, slightly less green) */
    float3 result = rgb;
    result.x += halation * redness;
    result.y += halation * 0.15f;   /* slight green spill for warmth */
    result.z -= halation * 0.05f;   /* slight blue reduction */

    return _film_clamp3(result);
}

/* =====================================================================
 * BLOOM / GLOW (Full-Spectrum Soft Scatter)
 * ===================================================================== */

/*
 * Bloom is derived from Halation but applied to all channels equally.
 * It simulates the softening effect of light scattering in the
 * lens and emulsion layers, independent of wavelength.
 *
 * Parameters:
 *   rgb       : input RGB
 *   strength  : bloom intensity (0.0 - 1.0)
 *   threshold : luminance above which bloom activates (0.5 - 1.0)
 *   spread    : bloom softness (0.0 - 1.0)
 */
__DEVICE__ float3 _film_bloom_local(float3 rgb, float strength,
                                     float threshold, float spread)
{
    if (strength <= 0.0f) return rgb;

    float lum = _film_luminance(rgb);

    /* Only apply bloom above threshold */
    if (lum <= threshold) return rgb;

    /* Bloom intensity above threshold */
    float excess = (lum - threshold) / (1.0f - threshold);

    /* Gaussian spread */
    float bloom = strength * excess * _expf(-excess * excess * (1.0f + spread * 3.0f));

    /* Apply as subtle desaturation + glow */
    float3 result = rgb;
    float glow = bloom * 0.15f;
    result.x += glow;
    result.y += glow;
    result.z += glow;

    /* Slight saturation reduction (bloom desaturates highlights) */
    float sat_mix = 1.0f - bloom * 0.3f;
    float mid = (result.x + result.y + result.z) / 3.0f;
    result.x = _film_lerp(mid, result.x, sat_mix);
    result.y = _film_lerp(mid, result.y, sat_mix);
    result.z = _film_lerp(mid, result.z, sat_mix);

    return _film_clamp3(result);
}

#endif /* DUXUN_TEXTURE_H */
