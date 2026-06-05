/**
 * DuXunProcess.cpp
 * ================
 * Image processing pipeline for 独寻胶片模拟.
 *
 * Implements the full film simulation pipeline:
 *   S-Curve → Color Matrix → Dye Density → CMY Warmth → Display Gamma
 *
 * All math is inline float (no external GPU dependencies).
 * Designed to be replaced with OpenCL/CUDA/Metal for production.
 */

#include "DuXunProcess.h"
#include <cmath>
#include <cstring>

namespace {

// BT.709 luminance coefficients
constexpr float LUM_R = 0.2126f;
constexpr float LUM_G = 0.7152f;
constexpr float LUM_B = 0.0722f;
constexpr float EPS   = 1e-10f;

inline float lum(float r, float g, float b) {
    return LUM_R * r + LUM_G * g + LUM_B * b;
}

inline float clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

/**
 * Film S-Curve: per-channel film characteristic curve.
 */
float filmSCurve(float x, float contrast, float pivot,
                  float shoulder, float toe, float blackPt) {
    if (x < EPS) return blackPt > 0 ? blackPt : 0;

    float sign = (x >= 0) ? 1.0f : -1.0f;
    x = fabsf(x);

    float exposure = x / pivot;
    float logExp = log2f(exposure) * contrast;

    // Toe: shadow compression
    float toeBlend = 1.0f / (1.0f + expf(-8.0f * (logExp + 2.0f)));
    float toeFactor = toe * 0.8f;
    float shadowScale = 1.0f - toeFactor * toeBlend;

    // Guard against division by zero in deep toe
    float toeDeep = 1.0f - logExp * 0.2f;
    if (fabsf(toeDeep) < 0.001f) toeDeep = (toeDeep > 0) ? 0.001f : -0.001f;
    float toeDeepFactor = 1.0f - 1.0f / toeDeep;
    shadowScale *= 1.0f - toeFactor * toeBlend * toeDeepFactor * 0.5f;
    logExp *= shadowScale;

    // Shoulder: highlight roll-off
    if (logExp > 2.0f) {
        float s = (logExp - 2.0f) * shoulder * 0.5f;
        logExp = 2.0f + log2f(1.0f + s * 2.0f);
    }

    float y = powf(2.0f, logExp) * pivot + blackPt;
    if (y < 0) y = 0;
    return y * sign;
}

/**
 * Apply 3x3 film color matrix.
 */
void applyMatrix(float& r, float& g, float& b,
                  const float mat[9], float mix, float lumKeep) {
    float rm = mat[0]*r + mat[1]*g + mat[2]*b;
    float gm = mat[3]*r + mat[4]*g + mat[5]*b;
    float bm = mat[6]*r + mat[7]*g + mat[8]*b;

    // Luminance preservation
    if (lumKeep > 0) {
        float origLum = lum(r, g, b);
        float newLum  = lum(rm, gm, bm);
        float ratio = (newLum > EPS) ? origLum / newLum : 1.0f;
        float corr = 1.0f + (ratio - 1.0f) * lumKeep;
        rm *= corr; gm *= corr; bm *= corr;
    }

    // Blend
    r = lerpf(r, rm, mix);
    g = lerpf(g, gm, mix);
    b = lerpf(b, bm, mix);
}

/**
 * Dye density chroma model.
 */
void applyDensity(float& r, float& g, float& b,
                   float density, float chromaRoll, float shadowSat) {
    float l = lum(r, g, b);
    if (l < EPS) l = EPS;

    // Chroma ratios
    float cr = r / l, cg = g / l, cb = b / l;

    // Density proxy
    float d = 1.0f - l;
    d = clampf(d, 0.0f, 1.0f);

    // Chroma boost peak around d=0.58
    float densityPeak = density * 0.4f;
    float d2 = d * d;
    float chromaBoost = 1.0f + densityPeak * (1.0f - d2) * d;

    // Highlight chroma roll-off
    float brightFactor = 1.0f;
    if (l > 0.5f) {
        float bf = (l - 0.5f) * 2.0f;
        brightFactor = 1.0f - chromaRoll * bf * bf;
        if (brightFactor < 0.2f) brightFactor = 0.2f;
    }

    // Shadow saturation preservation
    float shadowFactor = 1.0f;
    if (l < 0.1f) {
        float s = l / 0.1f;
        shadowFactor = shadowSat + (1.0f - shadowSat) * s;
    }

    float chromaMult = chromaBoost * brightFactor * shadowFactor;
    if (chromaMult < 0.1f) chromaMult = 0.1f;

    // Rebuild with 80% blend
    r = lerpf(r, l * cr * chromaMult, 0.8f);
    g = lerpf(g, l * cg * chromaMult, 0.8f);
    b = lerpf(b, l * cb * chromaMult, 0.8f);
}

/**
 * CMY subtractive warmth adjustment.
 */
void applyCMY(float& r, float& g, float& b, float warmth) {
    if (warmth == 0) return;

    float logR = (r > EPS) ? log2f(r) : -10.0f;
    float logG = (g > EPS) ? log2f(g) : -10.0f;
    float logB = (b > EPS) ? log2f(b) : -10.0f;

    logR += warmth * 0.15f;
    logB -= warmth * 0.1f;

    r = powf(2.0f, logR);
    b = powf(2.0f, logB);
}

/**
 * Simplified halation (red scatter).
 */
void applyHalation(float& r, float& g, float& b, float strength) {
    if (strength <= 0) return;

    float redExcess = r - g;
    if (redExcess <= 0) return;

    float l = lum(r, g, b);
    float scatter = expf(-(l - 0.5f)*(l - 0.5f) / 0.09f);
    float decay = 1.0f - expf(-redExcess / 0.3f);
    float hal = strength * scatter * decay * redExcess * 0.3f;

    r += hal;
    g += hal * 0.15f;
}

/**
 * Simplified grain (hash-based noise).
 */
float grainNoise(int x, int y, int frame, int channel) {
    float h = (float)(x * 127 + y * 311 + frame * 73 + channel * 419);
    float v = sinf(h * 0.001f) * 43758.5453f;
    return v - floorf(v);
}

void applyGrain(float& r, float& g, float& b,
                 int x, int y, int frame, float strength) {
    if (strength <= 0) return;

    float l = lum(r, g, b);
    float response = 1.0f;
    if (l < 0.05f) response = l / 0.05f;
    else if (l > 0.95f) response = (1.0f - l) / 0.05f;

    float gr = (grainNoise(x, y, frame, 0) - 0.5f) * 2.0f * strength * response;
    float gg = (grainNoise(x, y, frame, 1) - 0.5f) * 2.0f * strength * response;
    float gb = (grainNoise(x, y, frame, 2) - 0.5f) * 2.0f * strength * response;

    r += gr * r;
    g += gg * g;
    b += gb * b;

    if (r < 0) r = 0;
    if (g < 0) g = 0;
    if (b < 0) b = 0;
}

/**
 * Display gamma (Rec.709 / sRGB).
 */
void applyGamma(float& r, float& g, float& b, float gamma) {
    float invGamma = 1.0f / gamma;
    r = powf(clampf(r, 0.0f, 100.0f), invGamma);
    g = powf(clampf(g, 0.0f, 100.0f), invGamma);
    b = powf(clampf(b, 0.0f, 100.0f), invGamma);
    r = clampf(r, 0.0f, 1.0f);
    g = clampf(g, 0.0f, 1.0f);
    b = clampf(b, 0.0f, 1.0f);
}

} // anonymous namespace

// ============================================================
// Main processing entry point
// ============================================================
void processImage(
    const float* src, float* dst,
    int width, int height,
    int srcRowBytes, int dstRowBytes,
    const DuXunInstanceData* data)
{
    int frame = 0; // TODO: get from time property

    for (int y = 0; y < height; y++) {
        const float* srcRow = (const float*)((const char*)src + y * srcRowBytes);
        float* dstRow = (float*)((char*)dst + y * dstRowBytes);

        for (int x = 0; x < width; x++) {
            float r = srcRow[x*4 + 0];
            float g = srcRow[x*4 + 1];
            float b = srcRow[x*4 + 2];
            float a = srcRow[x*4 + 3];

            // Stage 1: S-Curve
            r = filmSCurve(r, data->contrast, data->pivot,
                           data->shoulder, data->toe, 0.0f);
            g = filmSCurve(g, data->contrast, data->pivot,
                           data->shoulder, data->toe, 0.0f);
            b = filmSCurve(b, data->contrast, data->pivot,
                           data->shoulder, data->toe, 0.0f);

            // Stage 2: Color Matrix
            applyMatrix(r, g, b, data->activeMatrix,
                        data->matrixMix, data->lumKeep);

            // Stage 3: Dye Density
            applyDensity(r, g, b, data->density,
                         data->chromaRoll, data->shadowSat);

            // Stage 4: CMY Warmth
            applyCMY(r, g, b, data->warmth);

            // Stage 5: Halation
            applyHalation(r, g, b, data->halation);

            // Stage 6: Film Grain
            applyGrain(r, g, b, x, y, frame, data->grain);

            // Stage 7: Display Gamma
            applyGamma(r, g, b, data->displayGamma);

            // Write output (preserve alpha)
            dstRow[x*4 + 0] = r;
            dstRow[x*4 + 1] = g;
            dstRow[x*4 + 2] = b;
            dstRow[x*4 + 3] = a;
        }
    }
}
