/*
 * DuXun-Common.h
 * ================
 * Shared Math Library for FilmSim DCTL Pipeline
 *
 * Author: Senior Developer (film-sim-plugin team)
 * Version: 1.0.0
 * License: MIT
 *
 * Purpose:
 *   Single source of truth for all shared color science functions,
 *   constants, and utility math used across the FilmSim pipeline.
 *   Every module includes this header instead of duplicating code.
 *
 * DCTL Include:
 *   #include "DuXun-Common.h"
 *
 * Design Principles:
 *   1. All functions prefixed with _film_ to avoid name collisions
 *   2. All magic numbers extracted as named constants
 *   3. 32-bit float throughout (DCTL requirement)
 *   4. Branch-free where possible for GPU performance
 *   5. Guard against NaN/Inf at every stage
 */

#ifndef DUXUN_COMMON_H
#define DUXUN_COMMON_H

/* =====================================================================
 * CONSTANTS
 * ===================================================================== */

/* Luminance Coefficients (BT.709 / Rec.709 standard) */
#define FILM_LUM_R_COEF  0.2126f
#define FILM_LUM_G_COEF  0.7152f
#define FILM_LUM_B_COEF  0.0722f

/* Middle Gray References */
#define FILM_MIDDLE_GRAY      0.18f      /* 18% gray card (photographic standard) */
#define FILM_MIDDLE_GRAY_STOP -2.4739f   /* log2(0.18) */

/* Cineon Encoding Constants */
#define CINEON_BLACK_CV    95.0f         /* D-min = 0.2 */
#define CINEON_WHITE_CV   685.0f         /* D-max = 2.0 */
#define CINEON_GAMMA      685.0f
#define CINEON_OFFSET     470.0f
#define CINEON_REF_WHITE  685.0f

/* Numeric Guards */
#define FILM_EPSILON 1e-10f             /* Minimum value to avoid log2(0) */
#define FILM_MAX_FLOAT 100.0f            /* Soft ceiling for output clamping */

/* =====================================================================
 * SAFE MATH WRAPPERS
 * ===================================================================== */

/*
 * Safe log2 -- returns floor value for near-zero inputs
 * Avoids -INF that would propagate NaN through the pipeline.
 */
__DEVICE__ float _film_log2(float x)
{
    return (x > FILM_EPSILON) ? _log2f(x) : _log2f(FILM_EPSILON);
}

/*
 * Safe exponential 2^x -- guards against overflow
 */
__DEVICE__ float _film_exp2(float x)
{
    return (x > 20.0f) ? _powf(2.0f, 20.0f)
         : (x < -20.0f) ? 0.0f
         : _powf(2.0f, x);
}

/*
 * Linear interpolation (lerp)
 * Equivalent to: a + (b - a) * t
 */
__DEVICE__ float _film_lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

/*
 * Smoothstep (GLSL-compatible)
 * Returns 0 for edge0, 1 for edge1, smooth cubic transition between.
 */
__DEVICE__ float _film_smoothstep(float edge0, float edge1, float x)
{
    float t = (x - edge0) / (edge1 - edge0);
    t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

/*
 * Clamp to valid output range [0, FILM_MAX_FLOAT]
 */
__DEVICE__ float _film_clamp(float x)
{
    return (x < 0.0f) ? 0.0f : ((x > FILM_MAX_FLOAT) ? FILM_MAX_FLOAT : x);
}

/*
 * Clamp float3 to valid output range
 */
__DEVICE__ float3 _film_clamp3(float3 rgb)
{
    rgb.x = _film_clamp(rgb.x);
    rgb.y = _film_clamp(rgb.y);
    rgb.z = _film_clamp(rgb.z);
    return rgb;
}

/* =====================================================================
 * COLORIMETRY
 * ===================================================================== */

/*
 * Perceptual Luminance (BT.709)
 * Input:  linear RGB
 * Output: single luminance value
 */
__DEVICE__ float _film_luminance(float3 rgb)
{
    return FILM_LUM_R_COEF * rgb.x
         + FILM_LUM_G_COEF * rgb.y
         + FILM_LUM_B_COEF * rgb.z;
}

/*
 * Luminance with epsilon guard against zero
 */
__DEVICE__ float _film_luminance_safe(float3 rgb)
{
    float lum = _film_luminance(rgb);
    return (lum > FILM_EPSILON) ? lum : FILM_EPSILON;
}

/*
 * Chroma decomposition: RGB → Luminance + Chrominance ratios
 * Returns chroma ratios (R/L, G/L, B/L) and luminance.
 * Caller provides float* for lum output.
 */
__DEVICE__ float3 _film_chroma_decompose(float3 rgb, float *out_lum)
{
    float lum = _film_luminance_safe(rgb);
    *out_lum = lum;
    return make_float3(rgb.x / lum, rgb.y / lum, rgb.z / lum);
}

/*
 * Rebuild RGB from luminance and chroma ratios
 */
__DEVICE__ float3 _film_chroma_recompose(float lum, float3 chroma)
{
    return make_float3(chroma.x * lum, chroma.y * lum, chroma.z * lum);
}

/* =====================================================================
 * FILM CHARACTERISTIC CURVE (UNIFIED)
 * ===================================================================== */

/*
 * Film S-Curve -- the ONE unified implementation used by all modules.
 *
 * Models Cineon-style D-log-H curve:
 *   1. Normalize to middle gray pivot
 *   2. Apply contrast in log2 space
 *   3. Toe: soft shadow compression via logistic blend
 *   4. Shoulder: highlight roll-off via knee function
 *   5. Convert back to linear
 *
 * Parameters:
 *   x         : input linear value (single channel)
 *   contrast  : midtone contrast multiplier (0.5 - 2.0)
 *   pivot     : middle gray reference point (typ. 0.18)
 *   shoulder  : highlight roll-off strength (0.0 - 1.0)
 *   toe       : shadow compression strength (0.0 - 1.0)
 *   black_pt  : black point output offset (-0.05 - 0.05)
 *
 * Returns: filmic linear value
 */
__DEVICE__ float _film_s_curve(float x, float contrast, float pivot,
                               float shoulder, float toe, float black_pt)
{
    /* Guard against zero and negative */
    if (x < FILM_EPSILON) {
        return (black_pt > 0.0f) ? black_pt : 0.0f;
    }

    float sign = (x >= 0.0f) ? 1.0f : -1.0f;
    x = _fabs(x);

    /* Step 1: Normalize to middle gray */
    float exposure = x / pivot;

    /* Step 2: Log2 contrast */
    float log_exp = _film_log2(exposure) * contrast;

    /* Step 3: Toe -- shadow compression
     * Uses a logistic function to smoothly transition from linear
     * shadow response to log midtone response.
     * The blend region is centered ~2 stops below middle gray.
     */
    float toe_blend = 1.0f / (1.0f + _expf(-8.0f * (log_exp + 2.0f)));
    float toe_factor = toe * 0.8f;

    /* Shadow detail preservation: reduce contrast in deep shadows */
    float shadow_scale = 1.0f - toe_factor * toe_blend;

    /*
     * Secondary toe roll-off for deepest shadows.
     * Guard: clamp (log_exp * 0.2f) denominator away from 1.0
     */
    float toe_deep = 1.0f - log_exp * 0.2f;
    if (_fabs(toe_deep) < 0.001f) {
        toe_deep = (toe_deep > 0.0f) ? 0.001f : -0.001f;
    }
    float toe_deep_factor = 1.0f - 1.0f / toe_deep;
    shadow_scale *= 1.0f - toe_factor * toe_blend * toe_deep_factor * 0.5f;

    log_exp *= shadow_scale;

    /* Step 4: Shoulder -- highlight roll-off
     * Knee function: linear below shoulder_start, log compression above.
     * shoulder_start = 2 stops above middle gray.
     */
    float shoulder_start = 2.0f;
    if (log_exp > shoulder_start) {
        float s = (log_exp - shoulder_start) * shoulder * 0.5f;
        log_exp = shoulder_start + _film_log2(1.0f + s * 2.0f);
    }

    /* Step 5: Back to linear */
    float y = _film_exp2(log_exp) * pivot + black_pt;

    /* Step 6: Clamp negative and restore sign */
    y = (y < 0.0f) ? 0.0f : y;

    return y * sign;
}

/*
 * Per-channel film S-curve for float3 RGB input
 */
__DEVICE__ float3 _film_s_curve3(float3 rgb, float contrast, float pivot,
                                  float shoulder, float toe, float black_pt)
{
    rgb.x = _film_s_curve(rgb.x, contrast, pivot, shoulder, toe, black_pt);
    rgb.y = _film_s_curve(rgb.y, contrast, pivot, shoulder, toe, black_pt);
    rgb.z = _film_s_curve(rgb.z, contrast, pivot, shoulder, toe, black_pt);
    return rgb;
}

/* =====================================================================
 * FILM COLOR MATRICES
 * ===================================================================== */

/* Number of supported film stock presets */
#define FILM_MATRIX_PRESET_COUNT 7

/*
 * Get 3x3 film color matrix by preset index.
 * Matrix format: row-major, 9 floats:
 *   [0-2] = R_out coefficients (R_in, G_in, B_in)
 *   [3-5] = G_out coefficients
 *   [6-8] = B_out coefficients
 *
 * Presets:
 *   0: Kodak 2383 Print -- warm highlights, rich shadows
 *   1: Fuji 3513 Print   -- cooler, cyan-biased
 *   2: Kodak Vision3 Neg -- natural with warm skin tones
 *   3: Fuji Eterna Neg   -- neutral, low contrast
 *   4: Technicolor 3-strip -- strong color separation
 *   5: Bleach Bypass     -- desaturated, high contrast
 */
__DEVICE__ void _film_matrix_get(int preset, float mat[9])
{
    /* Kodak 2383 Print Film */
    const float m_k2383[9] = {
         1.08f, -0.02f, -0.06f,
         0.03f,  1.05f, -0.08f,
        -0.05f, -0.04f,  1.09f
    };

    /* Fuji 3513 Print Film */
    const float m_f3513[9] = {
         1.05f,  0.02f, -0.07f,
        -0.04f,  1.08f, -0.04f,
        -0.02f, -0.06f,  1.08f
    };

    /* Kodak Vision3 250D Negative */
    const float m_v3[9] = {
         1.10f, -0.05f, -0.05f,
         0.02f,  1.04f, -0.06f,
        -0.06f, -0.02f,  1.08f
    };

    /* Fuji Eterna Vivid 160 Negative */
    const float m_eterna[9] = {
         1.04f,  0.01f, -0.05f,
        -0.02f,  1.06f, -0.04f,
        -0.03f, -0.03f,  1.06f
    };

    /* Vintage Technicolor 3-Strip */
    const float m_techni[9] = {
         1.20f, -0.10f, -0.10f,
        -0.08f,  1.18f, -0.10f,
        -0.10f, -0.08f,  1.18f
    };

    /* Bleach Bypass */
    const float m_bleach[9] = {
         0.85f,  0.10f,  0.05f,
         0.10f,  0.85f,  0.05f,
         0.05f,  0.05f,  0.90f
    };

    /* Custom / Identity (fallback) */
    const float m_identity[9] = {
         1.0f, 0.0f, 0.0f,
         0.0f, 1.0f, 0.0f,
         0.0f, 0.0f, 1.0f
    };

    const float *src;
    switch (preset) {
        case 0:  src = m_k2383;     break;
        case 1:  src = m_f3513;     break;
        case 2:  src = m_v3;        break;
        case 3:  src = m_eterna;    break;
        case 4:  src = m_techni;    break;
        case 5:  src = m_bleach;    break;
        case 6:  src = m_identity;  break;
        default: src = m_k2383;     break;
    }

    /* Copy 9 floats */
    for (int i = 0; i < 9; i++) {
        mat[i] = src[i];
    }
}

/*
 * Apply 3x3 color matrix to RGB, with luminance preservation blend.
 *
 * Parameters:
 *   rgb      : input linear RGB
 *   mat      : 9-float row-major matrix
 *   mix      : blend strength (0.0 = original, 1.0 = full matrix)
 *   lum_keep : how much luminance to preserve (0.0 = none, 1.0 = full)
 */
__DEVICE__ float3 _film_matrix_apply(float3 rgb, float mat[9],
                                      float mix, float lum_keep)
{
    float r_mat = mat[0]*rgb.x + mat[1]*rgb.y + mat[2]*rgb.z;
    float g_mat = mat[3]*rgb.x + mat[4]*rgb.y + mat[5]*rgb.z;
    float b_mat = mat[6]*rgb.x + mat[7]*rgb.y + mat[8]*rgb.z;

    /* Luminance preservation */
    if (lum_keep > 0.0f) {
        float orig_lum = _film_luminance(rgb);
        float new_lum  = _film_luminance(make_float3(r_mat, g_mat, b_mat));
        float lum_ratio = (new_lum > FILM_EPSILON) ? orig_lum / new_lum : 1.0f;
        float lum_corr = 1.0f + (lum_ratio - 1.0f) * lum_keep;
        r_mat *= lum_corr;
        g_mat *= lum_corr;
        b_mat *= lum_corr;
    }

    /* Blend */
    rgb.x = _film_lerp(rgb.x, r_mat, mix);
    rgb.y = _film_lerp(rgb.y, g_mat, mix);
    rgb.z = _film_lerp(rgb.z, b_mat, mix);

    return rgb;
}

/* =====================================================================
 * DYE DENSITY SIMULATION
 * ===================================================================== */

/*
 * Dye density chroma model -- unified implementation.
 *
 * Models how CMY dye clouds in emulsion affect perceived saturation:
 *   - Peak chroma at medium density (~0.58)
 *   - Chroma compression in dark areas (dye crowding)
 *   - Highlight roll-off for specular highlights
 *   - Shadow saturation preservation
 *
 * Parameters:
 *   rgb            : input RGB (post-curve)
 *   density        : overall dye density strength (0.0 - 1.5)
 *   chroma_roll    : highlight saturation compression (0.0 - 1.0)
 *   shadow_sat     : shadow saturation preservation (0.0 - 1.0)
 *   hue_shift      : density-dependent hue warmth (0.0 - 0.5)
 *   blend          : mix ratio original vs processed (0.8 typical)
 */
__DEVICE__ float3 _film_density_apply(float3 rgb, float density,
                                       float chroma_roll, float shadow_sat,
                                       float hue_shift, float blend)
{
    /* Luminance extraction */
    float lum = _film_luminance_safe(rgb);

    /* Chroma decomposition */
    float3 chroma = make_float3(rgb.x / lum, rgb.y / lum, rgb.z / lum);

    /* Density proxy: 1.0 - luminance (0 = transparent, 1 = opaque) */
    float d = 1.0f - lum;
    d = (d < 0.0f) ? 0.0f : ((d > 1.0f) ? 1.0f : d);

    /* Chroma boost: peak at d ~= 0.58 */
    float density_peak = density * 0.4f;
    float d2 = d * d;
    float chroma_boost = 1.0f + density_peak * (1.0f - d2) * d;

    /* Highlight chroma roll-off (quadratic from 50% luminance up) */
    float bright_factor = 1.0f;
    if (lum > 0.5f) {
        float b = (lum - 0.5f) * 2.0f;
        bright_factor = 1.0f - chroma_roll * b * b;
        bright_factor = (bright_factor < 0.2f) ? 0.2f : bright_factor;
    }

    /* Shadow saturation preservation */
    float shadow_factor = 1.0f;
    if (lum < 0.1f) {
        float s = lum / 0.1f;
        shadow_factor = shadow_sat + (1.0f - shadow_sat) * s;
    }

    /* Combined chroma multiplier */
    float chroma_mult = chroma_boost * bright_factor * shadow_factor;
    chroma_mult = (chroma_mult < 0.1f) ? 0.1f : chroma_mult;

    /*
     * Density-dependent hue shift: apply to chroma BEFORE rebuilding.
     * (FIX: this was the P0 bug -- previously hue shift was applied to
     *  rgb and then partially overwritten by the blend step)
     */
    if (hue_shift != 0.0f) {
        float hue_amount = hue_shift * (d - 0.3f);
        float r_shift = 1.0f + hue_amount * 0.05f;
        float g_shift = 1.0f;
        float b_shift = 1.0f - hue_amount * 0.03f;
        chroma.x *= r_shift;
        chroma.z *= b_shift;
        /* Renormalize to preserve luminance */
        float new_lum_c = _film_luminance(chroma);
        if (new_lum_c > FILM_EPSILON) {
            float renorm = 1.0f / new_lum_c;
            chroma.x *= renorm;
            chroma.y *= renorm;
            chroma.z *= renorm;
        }
    }

    /* Processed RGB with modified chroma */
    float3 new_rgb = _film_chroma_recompose(lum, make_float3(
        chroma.x * chroma_mult,
        chroma.y * chroma_mult,
        chroma.z * chroma_mult
    ));

    /* Blend original with processed */
    rgb.x = _film_lerp(rgb.x, new_rgb.x, blend);
    rgb.y = _film_lerp(rgb.y, new_rgb.y, blend);
    rgb.z = _film_lerp(rgb.z, new_rgb.z, blend);

    return rgb;
}

/* =====================================================================
 * CMY SUBTRACTIVE COLOR HEAD
 * ===================================================================== */

/*
 * CMY subtractive color adjustment -- unified implementation.
 *
 * Models optical printer color timing using CMY density filters.
 * Operations are performed in log2 (density) space where CMY is linear.
 *
 * Parameters:
 *   rgb      : input linear RGB
 *   cyan     : cyan filter density (-1.0 to 1.0)
 *   magenta  : magenta filter density (-1.0 to 1.0)
 *   yellow   : yellow filter density (-1.0 to 1.0)
 *   density  : overall print density (0.0 - 1.0)
 *   preserve_mg: whether to compensate middle gray exposure (1=yes, 0=no)
 */
__DEVICE__ float3 _film_cmy_apply(float3 rgb, float cyan, float magenta,
                                   float yellow, float density, int preserve_mg)
{
    /* Convert to log2 (density) domain */
    float log_r = _film_log2(rgb.x);
    float log_g = _film_log2(rgb.y);
    float log_b = _film_log2(rgb.z);

    /* Per-channel CMY filter densities */
    float red_density    = cyan * 0.5f;
    float green_density  = magenta * 0.5f;
    float blue_density   = yellow * 0.5f;

    log_r -= red_density;
    log_g -= green_density;
    log_b -= blue_density;

    /* Cross-channel coupling (real filters are not perfectly selective) */
    log_r -= magenta * 0.08f;
    log_b -= magenta * 0.05f;
    log_g -= cyan * 0.06f;
    log_b -= cyan * 0.04f;
    log_g -= yellow * 0.03f;
    log_r -= yellow * 0.02f;

    /* Overall print density */
    float overall = density * 0.6f;
    log_r -= overall;
    log_g -= overall;
    log_b -= overall;

    /* Back to linear */
    float r_out = _film_exp2(log_r);
    float g_out = _film_exp2(log_g);
    float b_out = _film_exp2(log_b);

    /* Optional: compensate to preserve middle gray exposure */
    if (preserve_mg) {
        float mg_in = FILM_MIDDLE_GRAY;
        float mg_log = FILM_MIDDLE_GRAY_STOP;

        float mg_r = _film_exp2(mg_log - red_density - overall);
        float mg_g = _film_exp2(mg_log - green_density - overall);
        float mg_b = _film_exp2(mg_log - blue_density - overall);

        float mg_avg = (mg_r + mg_g + mg_b) / 3.0f;
        float mg_corr = (mg_avg > FILM_EPSILON) ? mg_in / mg_avg : 1.0f;

        r_out *= mg_corr;
        g_out *= mg_corr;
        b_out *= mg_corr;
    }

    return _film_clamp3(make_float3(r_out, g_out, b_out));
}

#endif /* DUXUN_COMMON_H */
