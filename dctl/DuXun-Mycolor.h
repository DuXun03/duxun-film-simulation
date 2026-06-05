/*
 * DuXun-Mycolor.h
 * =================
 * Spectral Refraction, Dye Density, and Light Scatter Engine
 * Ported from boomyjee/mycolor (MIT License) -- GLSL → DCTL
 *
 * Original: https://github.com/boomyjee/mycolor
 * License: MIT
 *
 * This header provides three advanced color science models that
 * go beyond the basic DuXun-Common.h functions:
 *
 *   1. refrakt()  -- Spectral Refraction
 *      Splits RGB into 6 spectral hue channels (R-Y-G-C-B-M),
 *      applies independent hue rotation and saturation per channel,
 *      interpolates based on RGB channel ordering.
 *
 *   2. dns()      -- Dye Density Simulation
 *      Non-linear chroma enhancement based on three spline curves:
 *      Hue-vs-Saturation, Saturation-vs-Saturation, Luminance-vs-Saturation.
 *      Includes highlight roll-off and chroma compression protection.
 *
 *   3. skatter()  -- Light Scatter
 *      Simulates optical light scattering through emulsion layers.
 *      Handles shadow scatter (warmth bleed into shadows) and
 *      highlight scatter (specular bloom + chromatic fringing).
 *
 * Usage:
 *   #include "DuXun-Mycolor.h"
 *   (automatically includes DuXun-Common.h)
 *
 * Integration with FilmSim pipeline:
 *   [Contrast] → [Matrix] → [Density] → [CMY]
 *                                    ↑
 *   [refrakt → dns → skatter]  can augment or replace Density stage
 */

#ifndef DUXUN_MYCOLOR_H
#define DUXUN_MYCOLOR_H

#include "DuXun-Common.h"

/* =====================================================================
 * COLOR SPACE MATRICES
 * ===================================================================== */

/* RGB → YIQ conversion (NTSC standard) */
__CONSTANT__ float _my_rgb2yiq_r0[3] = {0.299f, 0.587f, 0.114f};
__CONSTANT__ float _my_rgb2yiq_r1[3] = {0.596f, -0.274f, -0.322f};
__CONSTANT__ float _my_rgb2yiq_r2[3] = {0.211f, -0.523f, 0.312f};

/* YIQ → RGB conversion */
__CONSTANT__ float _my_yiq2rgb_r0[3] = {1.0f, 0.956f, 0.621f};
__CONSTANT__ float _my_yiq2rgb_r1[3] = {1.0f, -0.272f, -0.647f};
__CONSTANT__ float _my_yiq2rgb_r2[3] = {1.0f, -1.105f, 1.702f};

/* Daylight illuminant D66.7 spectral weights */
__CONSTANT__ float _my_D667[3]    = {0.93f, 0.54f, 0.0f};
__CONSTANT__ float _my_D667inv[3] = {0.07f, 0.46f, 1.0f};

/* Shadow/Highlight coefficients (16 groups x 3 = 48 floats) */
__CONSTANT__ float _my_COEFFICIENTS[48] = {
    0.7241065241966241f, 0.38032959880647427f, 0.34893263968361626f,
    0.7835911868134677f, 0.5300864625456815f, 0.2766857286769332f,
    0.8271730047148866f, 0.6787247844882949f, 0.18326072978879046f,
    0.8300257270376946f, 0.8174593916039772f, 0.13592703408556722f,
    0.7406067120307717f, 0.8372307837782795f, 0.34289432442093193f,
    0.6539930498811265f, 0.8369045991706899f, 0.48298164483906014f,
    0.5564548549841858f, 0.8418846940401803f, 0.6307019025287327f,
    0.4244593376605096f, 0.8383847426091521f, 0.7833550941371147f,
    0.360113919906843f, 0.8208607739878486f, 0.9081093742083968f,
    0.5220658424842441f, 0.7993179189046314f, 0.9583201462123344f,
    0.6398980209684421f, 0.7601588749163941f, 0.9565857000218989f,
    0.7212096931791545f, 0.7109022154458666f, 0.9461554300141622f,
    0.7302135229590574f, 0.6171260999547182f, 0.910414970864051f,
    0.7405137392866862f, 0.4942038730729897f, 0.8095220428319969f,
    0.7350935645049326f, 0.36828980677764583f, 0.6505922452623222f,
    0.7225540492264427f, 0.3421089635952578f, 0.49381191031819593f
};

/* =====================================================================
 * VECTOR UTILITIES
 * ===================================================================== */

__DEVICE__ float _my_mixf(float a, float b, float t) { return a + (b - a) * t; }

__DEVICE__ float3 _my_mix3(float3 a, float3 b, float t) {
    return make_float3(_my_mixf(a.x, b.x, t), _my_mixf(a.y, b.y, t), _my_mixf(a.z, b.z, t));
}

__DEVICE__ float3 _my_mix3v(float3 a, float3 b, float3 t) {
    return make_float3(_my_mixf(a.x, b.x, t.x), _my_mixf(a.y, b.y, t.y), _my_mixf(a.z, b.z, t.z));
}

__DEVICE__ float _my_max3(float a, float b, float c) {
    return _fmaxf(a, _fmaxf(b, c));
}

__DEVICE__ float _my_min3(float a, float b, float c) {
    return _fminf(a, _fminf(b, c));
}

__DEVICE__ float _my_rgb2luma(float3 c) {
    return c.x * 0.29889531f + c.y * 0.58662247f + c.z * 0.11448223f;
}

__DEVICE__ float _my_rgb2chroma(float3 rgb) {
    float d1 = rgb.x *  0.59597799f + rgb.y * -0.27417610f + rgb.z * -0.32180189f;
    float d2 = rgb.x *  0.21147017f + rgb.y * -0.52261711f + rgb.z *  0.31114694f;
    return _sqrtf(d1 * d1 + d2 * d2);
}

__DEVICE__ float _my_chroma2log(float crm) {
    return (crm < 0.5f) ? _powf(4.0f * crm * (1.0f - crm), 0.4f) : 1.0f;
}

__DEVICE__ float _my_yiq2chroma(float3 yiq) {
    return _sqrtf(yiq.y * yiq.y + yiq.z * yiq.z);
}

__DEVICE__ float _my_rgb2hue(float3 c) {
    /* GLSL-style hue extraction via branchless mix */
    float4 k = make_float4(0.0f, -1.0f / 3.0f, 2.0f / 3.0f, -1.0f);
    float4 p = _my_mix3v(
        make_float4(c.z, c.y, k.w, k.z),
        make_float4(c.y, c.z, k.x, k.y),
        make_float3(c.z <= c.y ? 1.0f : 0.0f, 0.0f, 0.0f)  /* bool → float scalar */
    );
    /* Simplified: use scalar mix for the second step */
    float qx = (p.x <= c.x) ? c.x : p.x;
    float qy = (p.x <= c.x) ? p.y : p.y;
    float qz = (p.x <= c.x) ? p.z : c.x;
    float qw = (p.x <= c.x) ? p.w : p.x;
    return _fabs((qw - qy) / (6.0f * (qx - _fminf(qw, qy)) + 0.0001f) + qz);
}

__DEVICE__ float _my_rgb2hue_simple(float3 rgb) {
    float maxc = _my_max3(rgb.x, rgb.y, rgb.z);
    float minc = _my_min3(rgb.x, rgb.y, rgb.z);
    float delta = maxc - minc;
    if (delta < 0.00001f) return 0.0f;
    float h;
    if (maxc == rgb.x)      h = (rgb.y - rgb.z) / delta;
    else if (maxc == rgb.y) h = 2.0f + (rgb.z - rgb.x) / delta;
    else                    h = 4.0f + (rgb.x - rgb.y) / delta;
    h /= 6.0f;
    return (h < 0.0f) ? h + 1.0f : h;
}

__DEVICE__ float _my_cubic_interp(float p0, float p1, float p2, float p3, float t) {
    float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
    float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
    float c = -0.5f * p0 + 0.5f * p2;
    float d = p1;
    return ((a * t + b) * t + c) * t + d;
}

__DEVICE__ float _my_spline_value(float x, float p1, float p2, float p3,
                                   float p4, float p5, float p6) {
    float points[6] = {p1, p2, p3, p4, p5, p6};
    float t = x * 5.0f;
    int i = (int)_floor(t);
    float f = t - (float)i;
    if (i < 1) return _my_cubic_interp(points[0], points[0], points[1], points[2], f);
    if (i > 3) return _my_cubic_interp(points[3], points[4], points[5], points[5], f);
    return _my_cubic_interp(points[i-1], points[i], points[i+1], points[i+2], f);
}

__DEVICE__ float _my_sig(float x, float k) {
    return 1.0f / (1.0f + _expf(-(x - 0.5f) * k));
}

__DEVICE__ float _my_hlko(float x) {
    float x4 = x * x * x * x;
    return _expf(-42.3301f * x4) * (1.0f - x4);
}

/*
 * Hue angle to RGB chromaticity vector (unit circle projection)
 */
__DEVICE__ float3 _my_h2r(float h) {
    float3 r;
    r.x = _clampf(_fabs(h * 6.0f - 3.0f) - 1.0f, 0.0f, 1.0f) - 1.0f;
    r.y = _clampf(_fabs(h * 6.0f - 2.0f) * -1.0f + 2.0f, 0.0f, 1.0f) - 1.0f;
    r.z = _clampf(_fabs(h * 6.0f - 4.0f) * -1.0f + 2.0f, 0.0f, 1.0f) - 1.0f;
    return r;
}

__DEVICE__ float _my_s2m(float l, float spr) {
    float llmt = 0.2f;
    float illmt = 1.0f - llmt;
    spr = spr * illmt + llmt;
    return _my_sig(l, spr * spr * 60.0f);
}

/* Chroma limit constants */
#define _MY_CRM_O 0.333f
#define _MY_CRM_D 16.667f
#define _MY_CRM_L (_MY_CRM_O + _MY_CRM_D)
#define _MY_CRM_M (_powf(_MY_CRM_L, _MY_CRM_L) / (_powf(_MY_CRM_O, _MY_CRM_O) * _powf(_MY_CRM_D, _MY_CRM_D)))

__DEVICE__ float _my_mLMT(float3 rgb, float lum) {
    float crm = _my_rgb2chroma(rgb) * 2.0f;
    float mxc = _my_max3(rgb.x, rgb.y, rgb.z);
    float crm_ko = _MY_CRM_M * _powf(crm, mxc) * _powf(1.0f - crm, _MY_CRM_D - mxc);
    float hl_ko = _my_hlko(lum);
    return crm_ko * hl_ko * 1.333f;
}

/*
 * Map a hue/saturation pair into an RGB vector, interpolating between
 * low and high saturation based on separation.
 *
 * p: float4(lowHue, lowSat, highHue, highSat)
 * separation: blend factor between low and high
 * lmt: chroma protection limit
 */
__DEVICE__ float3 _my_p2c(float4 p, float separation, float lmt) {
    float d = p.z - p.x;
    float delta = d + ((_fabs(d) > 180.0f) ? ((d < 0.0f) ? 360.0f : -360.0f) : 0.0f);
    float s = _my_mixf(p.y, p.w, separation);
    s = _my_mixf(_fminf(s, 1.0f), s, lmt);
    float h = _fmod((p.x + delta * separation) + 360.0f, 360.0f) / 360.0f;
    return _my_h2r(h) * s + 1.0f;
}

/* =====================================================================
 * SPECTRAL REFRACTION (refrakt)
 * ===================================================================== */

/*
 * refrakt -- Spectral Refraction Engine
 *
 * Decomposes RGB into 6 spectral bands (R-Y-G-C-B-M), applies
 * per-band hue rotation and saturation adjustment, and interpolates
 * based on which channel is dominant.
 *
 * Parameters:
 *   rgb   : input linear RGB
 *   pR-pM : float4 hue/sat pairs for each spectral band: {loHue, loSat, hiHue, hiSat}
 *   lum   : pixel luminance
 *   spr   : spectral separation strength (0.0 - 1.0)
 *
 * Returns: refracted RGB
 */
__DEVICE__ float3 _film_refrakt(float3 rgb,
                                 float4 pR, float4 pY, float4 pG,
                                 float4 pC, float4 pB, float4 pM,
                                 float lum, float spr)
{
    float sprMask = _my_s2m(_my_max3(rgb.x, rgb.y, rgb.z), spr);
    float lmt = _my_mLMT(rgb, lum);

    /* 6-way sorting: determine RGB ordering and interpolate accordingly */
    if (rgb.x > rgb.y) {
        if (rgb.y > rgb.z) {
            /* R > G > B */
            float3 r = _my_p2c(pR, sprMask, lmt);
            float3 y = _my_p2c(pY, sprMask, lmt);
            return make_float3(
                rgb.x * r.x + rgb.y * (y.x - r.x) + rgb.z * (1.0f - y.x),
                rgb.x * r.y + rgb.y * (y.y - r.y) + rgb.z * (1.0f - y.y),
                rgb.x * r.z + rgb.y * (y.z - r.z) + rgb.z * (1.0f - y.z)
            );
        } else if (rgb.x > rgb.z) {
            /* R > B > G */
            float3 r = _my_p2c(pR, sprMask, lmt);
            float3 m = _my_p2c(pM, sprMask, lmt);
            return make_float3(
                rgb.x * r.x + rgb.y * (1.0f - m.x) + rgb.z * (m.x - r.x),
                rgb.x * r.y + rgb.y * (1.0f - m.y) + rgb.z * (m.y - r.y),
                rgb.x * r.z + rgb.y * (1.0f - m.z) + rgb.z * (m.z - r.z)
            );
        } else {
            /* B > R > G */
            float3 m = _my_p2c(pM, sprMask, lmt);
            float3 b = _my_p2c(pB, sprMask, lmt);
            return make_float3(
                rgb.x * (m.x - b.x) + rgb.y * (1.0f - m.x) + rgb.z * b.x,
                rgb.x * (m.y - b.y) + rgb.y * (1.0f - m.y) + rgb.z * b.y,
                rgb.x * (m.z - b.z) + rgb.y * (1.0f - m.z) + rgb.z * b.z
            );
        }
    } else {
        if (rgb.z > rgb.y) {
            /* B > G > R */
            float3 c = _my_p2c(pC, sprMask, lmt);
            float3 b = _my_p2c(pB, sprMask, lmt);
            return make_float3(
                rgb.x * (1.0f - c.x) + rgb.y * (c.x - b.x) + rgb.z * b.x,
                rgb.x * (1.0f - c.y) + rgb.y * (c.y - b.y) + rgb.z * b.y,
                rgb.x * (1.0f - c.z) + rgb.y * (c.z - b.z) + rgb.z * b.z
            );
        } else if (rgb.z > rgb.x) {
            /* G > B > R */
            float3 c = _my_p2c(pC, sprMask, lmt);
            float3 g = _my_p2c(pG, sprMask, lmt);
            return make_float3(
                rgb.x * (1.0f - c.x) + rgb.y * g.x + rgb.z * (c.x - g.x),
                rgb.x * (1.0f - c.y) + rgb.y * g.y + rgb.z * (c.y - g.y),
                rgb.x * (1.0f - c.z) + rgb.y * g.z + rgb.z * (c.z - g.z)
            );
        } else {
            /* G > R > B */
            float3 y = _my_p2c(pY, sprMask, lmt);
            float3 g = _my_p2c(pG, sprMask, lmt);
            return make_float3(
                rgb.x * (y.x - g.x) + rgb.y * g.x + rgb.z * (1.0f - y.x),
                rgb.x * (y.y - g.y) + rgb.y * g.y + rgb.z * (1.0f - y.y),
                rgb.x * (y.z - g.z) + rgb.y * g.z + rgb.z * (1.0f - y.z)
            );
        }
    }
}

/* =====================================================================
 * DYE DENSITY (dns)
 * ===================================================================== */

__DEVICE__ float _my_dns_hlko(float l) {
    float l4 = l * l * l * l;
    return _expf(-25.0f * l4) * (1.0f - l4);
}

__DEVICE__ float _my_dns_crmko(float crm) {
    return 39.06889564611748f * _powf(crm, 1.503301f) * _powf(1.0f - crm, 5.596701f);
}

/*
 * dns -- Dye Density Simulation
 *
 * Enhances chroma non-linearly. Below 0.5 acts as desaturation;
 * above 0.5 applies density-based chroma boost with highlight
 * and chroma protection limits.
 *
 * Parameters:
 *   rgb    : input RGB
 *   amount : density strength (0.0 - 1.0)
 *   crm    : chroma value
 *   lum    : luminance
 *   lum3   : luminance as float3 {lum,lum,lum}
 *
 * Returns: density-enhanced RGB
 */
__DEVICE__ float3 _film_dns(float3 rgb, float amount, float crm, float lum, float3 lum3)
{
    if (amount <= 0.5f) {
        return _my_mix3(lum3, rgb, amount * 2.0f);
    } else {
        float d = _powf((amount - 0.5f) / 0.5f, 2.0f);
        d *= _my_dns_hlko(lum) * _my_dns_crmko(crm);
        d = d * (12.125f - 1.0f) + 1.0f;
        float mxc = _my_max3(rgb.x, rgb.y, rgb.z);
        float3 dRGB = _my_mix3(lum3, rgb, _my_mixf(amount * 2.0f, d, amount));
        float max_dRGB = _my_max3(dRGB.x, dRGB.y, dRGB.z);
        float safety = (max_dRGB > 0.00001f) ? mxc / max_dRGB : 1.0f;
        dRGB = dRGB * _my_mixf(1.0f, safety, 1.413f - lum);
        return _my_mix3(rgb, dRGB, rgb.x);
    }
}

/* =====================================================================
 * LIGHT SCATTER (skatter)
 * ===================================================================== */

__DEVICE__ float _my_sk_hlmin(float l, float r, float k) {
    float h = _fmaxf(k - _fabs(l - r), 0.0f) / k;
    return _fminf(l, r) - h * h * k * 0.25f;
}

__DEVICE__ float _my_sk_slmt(float mxc) {
    float m3 = mxc * mxc * mxc;
    return _expf(-5.0f * m3) * (1.0f - m3);
}

__DEVICE__ float _my_sk_hlko(float l, float lp2) {
    return _expf(-6.0f * lp2) * (1.0f - lp2 * l);
}

__DEVICE__ float _my_sk_sdko(float lp2) {
    return _expf(-2.2f * lp2 * lp2) * (1.0f - lp2 * lp2);
}

__DEVICE__ float _my_sk_hd2m(float a, float b, float mxc) {
    float lmt = _my_sk_slmt(mxc);
    float d = _fabs(a - b);
    d = _fminf(d, 1.0f - d) / 0.5f;
    d = 1.0f - d * d;
    return (d * d * (1.0f - lmt)) + lmt;
}

/*
 * skatter -- Light Scatter Engine
 *
 * Simulates optical scattering of light through film emulsion layers.
 * Two independent effects:
 *   - Shadow scatter: warm color bleed into dark areas
 *   - Highlight scatter: specular bloom with chromatic fringing
 *
 * Uses YIQ color space for luminance-preserving scattering.
 *
 * Parameters:
 *   color      : input RGB
 *   shadows    : float4 shadow scatter params (RGB hue shift + alpha)
 *   highlights : float4 highlight scatter params
 *   hue        : pixel hue (0-1 normalized)
 *   maxRGB     : max(R,G,B)
 *   lum        : pixel luminance
 *
 * Returns: scattered RGB
 */
__DEVICE__ float3 _film_skatter(float3 color, float4 shadows, float4 highlights,
                                 float hue, float maxRGB, float lum)
{
    float lumP2 = lum * lum;

    /* Shadow scatter */
    float shd = _my_sk_hd2m(hue, shadows.w, maxRGB);
    float skm = lumP2 * (5.5f - lum);
    skm *= _my_sk_hlko(lum, lumP2);
    skm *= shd;
    skm *= 2.75f;

    float3 ssc = make_float3(shadows.x * skm, shadows.y * skm, shadows.z * skm);
    float ssi = _fminf(ssc.x, _fminf(ssc.y, ssc.z));
    float3 base = color;
    color = _my_mix3(base, ssc, ssi);

    float ssm = _fminf(2.0f, 1.0f + ssi * skm * (1.0f - shd) * 3.333f);
    float3 lumVec = make_float3(lum, lum, lum);
    color = make_float3(
        _clampf(_my_mixf(lum, color.x, ssm), 0.0f, 1.0f),
        _clampf(_my_mixf(lum, color.y, ssm), 0.0f, 1.0f),
        _clampf(_my_mixf(lum, color.z, ssm), 0.0f, 1.0f)
    );

    /* Highlight scatter */
    float hkm = _my_sk_sdko(lumP2);
    float3 hlv = make_float3(highlights.x, highlights.y, highlights.z);
    float hsi = _sqrtf(hlv.x*hlv.x + hlv.y*hlv.y + hlv.z*hlv.z);
    hsi = _fminf(1.0f, hsi);

    float3 hlc = make_float3(
        (1.0f - hkm) * (highlights.x * (2.0f - hsi)) + hkm,
        (1.0f - hkm) * (highlights.y * (2.0f - hsi)) + hkm,
        (1.0f - hkm) * (highlights.z * (2.0f - hsi)) + hkm
    );

    float3 hlc4 = make_float3(
        hlc.x * hlc.x * hlc.x * hlc.x,
        hlc.y * hlc.y * hlc.y * hlc.y,
        hlc.z * hlc.z * hlc.z * hlc.z
    );

    float sk = _my_sk_hlmin(lum, 0.58631f, 0.333f);
    float3 mixed = make_float3(color.x * hlc4.x, color.y * hlc4.y, color.z * hlc4.z);
    color = make_float3(
        _fminf(color.x, mixed.x),
        _fminf(color.y, mixed.y),
        _fminf(color.z, mixed.z)
    );

    /* YIQ luminance restoration */
    float yiq_y = _my_rgb2yiq_r1[0] * color.x + _my_rgb2yiq_r1[1] * color.y + _my_rgb2yiq_r1[2] * color.z;
    float yiq_z = _my_rgb2yiq_r2[0] * color.x + _my_rgb2yiq_r2[1] * color.y + _my_rgb2yiq_r2[2] * color.z;

    float rfl = lum - (ssi * _my_mixf(lumP2 * lumP2, maxRGB, ssc.x));
    float yiq_x = _my_mixf(_my_mixf(lum, rfl, ssi), lum, lumP2 * skm * 1.75f);

    float r_out = _my_yiq2rgb_r0[0] * yiq_x + _my_yiq2rgb_r0[1] * yiq_y + _my_yiq2rgb_r0[2] * yiq_z;
    float g_out = _my_yiq2rgb_r1[0] * yiq_x + _my_yiq2rgb_r1[1] * yiq_y + _my_yiq2rgb_r1[2] * yiq_z;
    float b_out = _my_yiq2rgb_r2[0] * yiq_x + _my_yiq2rgb_r2[1] * yiq_y + _my_yiq2rgb_r2[2] * yiq_z;

    return make_float3(r_out, g_out, b_out);
}

/*
 * Shadow/Highlight parameter calculator from angle/distance.
 * Maps angular hue+distance to RGB+alpha vector.
 */
__DEVICE__ float4 _film_calc_shadows_highlights(float angle, float distance, float baseValue)
{
    float t = (angle >= 360.0f) ? 0.0f : angle;
    float n = distance;
    float e = baseValue;
    e *= 1.0f - n;
    t /= 360.0f;

    float s = t * 16.0f;  /* COEF_GROUPS = 16 */
    int o = (int)_floor(s);
    int r = 3 * o;
    int a = ((o + 1) % 16) * 3;
    float l = s - (float)o;
    float c = 1.0f - l;

    return make_float4(
        e + n * (_my_COEFFICIENTS[r] * c + _my_COEFFICIENTS[a] * l),
        e + n * (_my_COEFFICIENTS[r+1] * c + _my_COEFFICIENTS[a+1] * l),
        e + n * (_my_COEFFICIENTS[r+2] * c + _my_COEFFICIENTS[a+2] * l),
        t
    );
}

#endif /* DUXUN_MYCOLOR_H */
