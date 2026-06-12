/**
 * DuXunFilmSim.cpp - v5.0
 * ==================
 * OpenFX plugin for DaVinci Resolve
 * 54 film presets from t3mujinpack, brand-cascading dropdowns,
 * LUT-optimized rendering, Chinese UI
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Film grain, breath, gate weave, halation, and diffusion/bloom ideas
 * are adapted for CPU OFX from NativeEnhancer-FE (MIT).
 */

#include <cstring>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxGPURender.h"
#include "ofxProperty.h"
#include "ofxParam.h"
#include "ofxMemory.h"
#include "ofxMultiThread.h"
#include "ofxMessage.h"

#include "DuXunLicense.h"

// ============================================================
// Plugin Constants
// ============================================================
static const char* PLUGIN_UID   = "com.duxun.filmsim";
static const char* PLUGIN_LABEL = "DuXun Film Simulation";

#ifndef DUXUN_ENABLE_OPENCL_RENDER
#define DUXUN_ENABLE_OPENCL_RENDER 0
#endif

#ifndef DUXUN_ENABLE_CUDA_RENDER
#define DUXUN_ENABLE_CUDA_RENDER 0
#endif

#define DUXUN_GPU_ACCELERATION_AVAILABLE (DUXUN_ENABLE_OPENCL_RENDER || DUXUN_ENABLE_CUDA_RENDER)

// ============================================================
// Brand & Preset Data
// ============================================================
enum { BRAND_AGFA=0, BRAND_CINESTILL, BRAND_FUJI, BRAND_ILFORD, BRAND_KODAK, BRAND_CUSTOM, NUM_BRANDS };

struct FilmPreset {
    const char* name;
    double contrast, shoulder, toe, density, halation, grain, saturation, vignette;
    const double* matrix;
};

// Organized by brand: Agfa(5) CineStill(2) Fuji(19) Ilford(6) Kodak(21) Custom(1) = 54
static const FilmPreset gPresets[] = {
    // --- Agfa (0-4) ---
    { "Agfa APX 100",           0.99, 0.20, 0.24, 0.87, 0.10, 0.12, 0.00, 0.00 },
    { "Agfa APX 25",            0.86, 0.20, 0.17, 0.91, 0.05, 0.04, 0.00, 0.00 },
    { "Agfa Vista 100",         1.12, 0.22, 0.10, 0.98, 0.12, 0.06, 0.98, 0.00 },
    { "Agfa Vista 200",         1.17, 0.22, 0.10, 0.95, 0.13, 0.10, 1.00, 0.00 },
    { "Agfa Vista 400",         1.02, 0.22, 0.17, 0.98, 0.15, 0.15, 1.02, 0.00 },
    // --- CineStill (5-6) ---
    { "CineStill 50D",          1.00, 0.30, 0.15, 0.98, 0.35, 0.05, 0.95, 0.00 },
    { "CineStill 800T",         0.76, 0.35, 0.17, 0.96, 0.50, 0.22, 0.88, 0.00 },
    // --- Fuji (7-25) ---
    { "Fuji Neopan 1600",       1.41, 0.25, 0.14, 0.98, 0.15, 0.40, 0.00, 0.00 },
    { "Fuji Neopan Acros 100",  1.20, 0.15, 0.10, 1.02, 0.05, 0.06, 0.00, 0.00 },
    { "Fuji Pro 160C",          1.01, 0.22, 0.10, 0.96, 0.12, 0.05, 0.92, 0.00 },
    { "Fuji Pro 400H",          1.20, 0.25, 0.15, 0.97, 0.12, 0.08, 0.85, 0.00 },
    { "Fuji Pro 800Z",          0.91, 0.28, 0.10, 0.98, 0.18, 0.18, 0.95, 0.00 },
    { "Fuji Superia 100",       1.17, 0.20, 0.11, 0.98, 0.12, 0.06, 1.02, 0.00 },
    { "Fuji Superia 1600",      1.18, 0.28, 0.10, 0.97, 0.20, 0.30, 1.08, 0.00 },
    { "Fuji Superia 200",       1.08, 0.20, 0.16, 0.97, 0.13, 0.09, 1.05, 0.00 },
    { "Fuji Superia 400",       1.13, 0.22, 0.10, 0.99, 0.14, 0.12, 1.05, 0.00 },
    { "Fuji Superia 800",       1.12, 0.25, 0.10, 0.98, 0.16, 0.18, 1.00, 0.00 },
    { "Fuji Superia HG 1600",   1.00, 0.28, 0.13, 0.98, 0.22, 0.32, 1.10, 0.00 },
    { "Fuji Astia 100F",        1.13, 0.25, 0.11, 0.97, 0.10, 0.03, 1.00, 0.00 },
    { "Fuji Fortia SP 50",      1.54, 0.18, 0.10, 0.94, 0.10, 0.03, 1.25, 0.00 },
    { "Fuji Provia 100F",       1.17, 0.22, 0.10, 0.97, 0.10, 0.03, 1.05, 0.00 },
    { "Fuji Provia 400F",       1.00, 0.22, 0.10, 1.00, 0.12, 0.06, 1.02, 0.00 },
    { "Fuji Provia 400X",       0.88, 0.22, 0.10, 1.00, 0.12, 0.08, 1.08, 0.00 },
    { "Fuji Sensia 100",        1.36, 0.20, 0.12, 0.95, 0.10, 0.05, 1.08, 0.00 },
    { "Fuji Velvia 100",        1.08, 0.20, 0.10, 0.99, 0.08, 0.04, 1.25, 0.00 },
    { "Fuji Velvia 50",         1.18, 0.18, 0.10, 1.01, 0.08, 0.03, 1.30, 0.00 },
    // --- Ilford (26-31) ---
    { "Ilford Delta 100",       1.06, 0.18, 0.13, 0.98, 0.05, 0.05, 0.00, 0.00 },
    { "Ilford Delta 3200",      1.00, 0.30, 0.10, 0.96, 0.20, 0.45, 0.00, 0.00 },
    { "Ilford Delta 400",       1.14, 0.20, 0.10, 1.00, 0.10, 0.15, 0.00, 0.00 },
    { "Ilford FP4 125",         1.07, 0.18, 0.15, 0.98, 0.08, 0.08, 0.00, 0.00 },
    { "Ilford HP5 Plus 400",    1.00, 0.22, 0.10, 1.01, 0.12, 0.22, 0.00, 0.00 },
    { "Ilford XP2",             1.15, 0.20, 0.15, 0.85, 0.08, 0.14, 0.00, 0.00 },
    // --- Kodak (32-52) ---
    { "Kodak T-Max 3200",       1.52, 0.35, 0.10, 0.94, 0.18, 0.42, 0.00, 0.00 },
    { "Kodak Tri-X 400",        1.23, 0.22, 0.10, 0.95, 0.15, 0.28, 0.00, 0.00 },
    { "Kodak ColorPlus 200",    1.65, 0.20, 0.10, 0.93, 0.14, 0.13, 1.02, 0.00 },
    { "Kodak Ektar 100",        1.20, 0.20, 0.10, 0.99, 0.10, 0.03, 1.18, 0.00 },
    { "Kodak Gold 200",         1.43, 0.22, 0.10, 0.95, 0.15, 0.14, 1.08, 0.00 },
    { "Kodak Portra 160 NC",    1.21, 0.28, 0.10, 0.96, 0.10, 0.04, 0.82, 0.00 },
    { "Kodak Portra 160 VC",    1.25, 0.22, 0.10, 0.96, 0.12, 0.05, 0.98, 0.00 },
    { "Kodak Portra 160",       1.17, 0.25, 0.11, 0.96, 0.12, 0.04, 0.88, 0.00 },
    { "Kodak Portra 400 NC",    1.22, 0.28, 0.10, 0.97, 0.13, 0.07, 0.85, 0.00 },
    { "Kodak Portra 400 UC",    1.16, 0.25, 0.13, 0.96, 0.14, 0.08, 0.95, 0.00 },
    { "Kodak Portra 400 VC",    1.28, 0.22, 0.10, 1.00, 0.15, 0.09, 1.05, 0.00 },
    { "Kodak Portra 400",       1.22, 0.28, 0.10, 0.96, 0.15, 0.08, 0.90, 0.00 },
    { "Kodak Portra 800",       1.35, 0.30, 0.10, 0.96, 0.20, 0.18, 0.92, 0.00 },
    { "Kodak Ultra Max 400",    1.32, 0.22, 0.10, 0.94, 0.16, 0.16, 1.06, 0.00 },
    { "Kodak Ektachrome 100 G", 1.22, 0.22, 0.10, 1.00, 0.10, 0.03, 1.05, 0.00 },
    { "Kodak Ektachrome 100 GX",1.00, 0.22, 0.10, 0.99, 0.10, 0.03, 1.02, 0.00 },
    { "Kodak Ektachrome 100 VS",1.01, 0.20, 0.10, 0.99, 0.10, 0.04, 1.18, 0.00 },
    { "Kodak Elite Chrome 400", 1.00, 0.22, 0.10, 1.02, 0.12, 0.08, 1.08, 0.00 },
    { "Kodak Kodakchrome 200",  1.13, 0.20, 0.10, 0.98, 0.10, 0.05, 1.10, 0.00 },
    { "Kodak Kodakchrome 25",   1.34, 0.15, 0.10, 0.99, 0.08, 0.02, 1.15, 0.00 },
    { "Kodak Kodakchrome 64",   1.13, 0.18, 0.10, 0.98, 0.08, 0.03, 1.12, 0.00 },
    // --- Custom (53) ---
    { "Custom",                 1.15, 0.20, 0.15, 0.85, 0.10, 0.10, 1.00, 0.00 },
};

static const int gNumPresets = 54;
static const int gBrandStart[] = { 0, 5, 7, 26, 32, 53 };
static const int gBrandCount[] = { 5, 2, 19, 6, 21, 1 };

enum ColorTransform {
    TRANSFORM_TIMELINE = 0,
    TRANSFORM_REC709_G24 = 1,
    TRANSFORM_DWG_INTERMEDIATE = 2
};

enum EnableChoice {
    ENABLE_SKIP = 0,
    ENABLE_ON = 1
};

enum StockHalationClass {
    HALATION_STOCK_NONE = 0,
    HALATION_STOCK_SUBTLE = 1,
    HALATION_STOCK_NO_REMJET = 2
};

enum ProfileAutoChoice {
    PROFILE_AUTO = 0,
};

enum GrainProfileChoice {
    GRAIN_65MM_FINE = 1,
    GRAIN_35MM_STANDARD = 2,
    GRAIN_35MM_HIGH_ISO = 3,
    GRAIN_16MM = 4,
    GRAIN_8MM = 5,
    GRAIN_CUSTOM = 6
};

enum HalationProfileChoice {
    HALATION_65MM_STANDARD = 1,
    HALATION_35MM_STANDARD = 2,
    HALATION_16MM_STANDARD = 3,
    HALATION_8MM_STANDARD = 4,
    HALATION_65MM_NO_REMJET = 5,
    HALATION_35MM_NO_REMJET = 6,
    HALATION_16MM_NO_REMJET = 7,
    HALATION_8MM_NO_REMJET = 8,
    HALATION_CUSTOM = 9
};

enum BloomProfileChoice {
    BLOOM_65MM = 1,
    BLOOM_35MM = 2,
    BLOOM_16MM = 3,
    BLOOM_8MM = 4,
    BLOOM_CUSTOM = 5
};

enum DamageProfileChoice {
    DAMAGE_65MM_CLEAN = 1,
    DAMAGE_35MM_PRINT = 2,
    DAMAGE_16MM_ARCHIVE = 3,
    DAMAGE_8MM_HOME = 4,
    DAMAGE_CUSTOM = 5
};

enum BreathProfileChoice {
    BREATH_65MM_SUBTLE = 1,
    BREATH_35MM_LIGHT = 2,
    BREATH_16MM_VISIBLE = 3,
    BREATH_8MM_STRONG = 4,
    BREATH_CUSTOM = 5
};

enum WeaveProfileChoice {
    WEAVE_65MM_STABLE = 1,
    WEAVE_35MM_CAMERA = 2,
    WEAVE_16MM_CAMERA = 3,
    WEAVE_8MM_PROJECTOR = 4,
    WEAVE_CUSTOM = 5
};

enum PrintStockChoice {
    PRINT_LINEAR = 0,
    PRINT_CINEON_LOG = 1,
    PRINT_KODAK_2383 = 2,
    PRINT_FUJI_3513 = 3,
    PRINT_KODAK_ENDURA = 4,
    PRINT_BW_BROMOPORTRAIT = 5
};

enum FilmGrainModeChoice {
    GRAIN_MODE_ANALOG = 0,
    GRAIN_MODE_DETAIL = 1,
    GRAIN_MODE_DYE_CLOUD = 2
};

enum EffectModeChoice {
    EFFECT_FULL = 0,
    EFFECT_COLOR_ONLY = 1,
    EFFECT_TEXTURE_ONLY = 2,
    EFFECT_GRAIN_HALATION = 3
};

enum OverscanPresetChoice {
    OVERSCAN_NONE = 0,
    OVERSCAN_ACADEMY_133 = 1,
    OVERSCAN_STANDARD_166 = 2,
    OVERSCAN_WIDESCREEN_185 = 3,
    OVERSCAN_SCOPE_239 = 4,
    OVERSCAN_SMALL_GATE = 5
};

enum PreviewQualityChoice {
    PREVIEW_FAST = 0,
    PREVIEW_BALANCED = 1,
    PREVIEW_QUALITY = 2
};

enum BloomMaskPreviewChoice {
    BLOOM_MASK_OFF = 0,
    BLOOM_MASK_SHOW = 1
};

enum GlowBufferKind {
    GLOW_HALATION = 0,
    GLOW_BLOOM = 1
};

struct GrainProfile {
    float gain;
    float size;
    float color;
    float highlightWeight;
    float shadows;
    float mids;
    float highlights;
    float resolution;
    float chroma;
    float grainTextureSeed;
    float grainClump;
    float grainSilverRetention;
};

static inline float clampf(float x, float lo, float hi);

static const double kMatIdentity[9] = {
    1.0000, 0.0000, 0.0000,
    0.0000, 1.0000, 0.0000,
    0.0000, 0.0000, 1.0000
};

static const double kMatAgfaVista100[9] = {
    1.1286, -0.0486, -0.0800,
    0.0217, 1.0799, -0.1016,
    -0.0311, -0.0177, 1.0488
};

static const double kMatCineStill50D[9] = {
    1.1147, -0.0436, -0.0711,
    0.0238, 1.0764, -0.1002,
    -0.0348, -0.0179, 1.0527
};

static const double kMatCineStill800T[9] = {
    1.1316, -0.0475, -0.0841,
    0.0224, 1.0875, -0.1099,
    -0.0296, -0.0164, 1.0461
};

static const double kMatFujiPro160C[9] = {
    1.1855, -0.0707, -0.1148,
    0.0158, 1.0850, -0.1007,
    -0.0226, -0.0178, 1.0404
};

static const double kMatFujiPro400H[9] = {
    1.1663, -0.0664, -0.0999,
    0.0164, 1.0762, -0.0926,
    -0.0257, -0.0193, 1.0449
};

static const double kMatFujiSuperia[9] = {
    1.1127, -0.0440, -0.0688,
    0.0235, 1.0724, -0.0959,
    -0.0359, -0.0187, 1.0546
};

static const double kMatFujiC200[9] = {
    1.1172, -0.0458, -0.0713,
    0.0226, 1.0728, -0.0954,
    -0.0347, -0.0188, 1.0535
};

static const double kMatFujiNatura1600[9] = {
    1.1028, -0.0428, -0.0600,
    0.0237, 1.0613, -0.0850,
    -0.0411, -0.0210, 1.0621
};

static const double kMatFujiProvia100F[9] = {
    1.1323, -0.0466, -0.0858,
    0.0230, 1.0919, -0.1149,
    -0.0290, -0.0158, 1.0448
};

static const double kMatFujiVelvia50[9] = {
    1.4764, -0.1415, -0.3349,
    0.0103, 1.1444, -0.1548,
    -0.0095, -0.0120, 1.0215
};

static const double kMatKodakEktar100[9] = {
    1.1069, -0.0382, -0.0687,
    0.0272, 1.0841, -0.1113,
    -0.0357, -0.0163, 1.0520
};

static const double kMatKodakGold200[9] = {
    1.1207, -0.0465, -0.0742,
    0.0224, 1.0756, -0.0980,
    -0.0334, -0.0183, 1.0518
};

static const double kMatKodakPortra160[9] = {
    1.1132, -0.0411, -0.0721,
    0.0254, 1.0833, -0.1087,
    -0.0341, -0.0166, 1.0507
};

static const double kMatKodakPortra400[9] = {
    1.1120, -0.0422, -0.0698,
    0.0246, 1.0773, -0.1019,
    -0.0353, -0.0177, 1.0530
};

static const double kMatKodakPortra800[9] = {
    1.1052, -0.0419, -0.0633,
    0.0244, 1.0678, -0.0921,
    -0.0389, -0.0195, 1.0583
};

static const double kMatKodakUltramax400[9] = {
    1.1122, -0.0437, -0.0685,
    0.0236, 1.0723, -0.0959,
    -0.0361, -0.0187, 1.0548
};

static const double kMatKodakEktachrome100D[9] = {
    1.1113, -0.0386, -0.0728,
    0.0272, 1.0904, -0.1176,
    -0.0337, -0.0154, 1.0492
};

static const double kMatKodachrome64[9] = {
    1.0642, -0.0204, -0.0437,
    0.0489, 1.0838, -0.1327,
    -0.0547, -0.0139, 1.0686
};

static int getBrandForFilm(int idx) {
    for (int b = 0; b < NUM_BRANDS - 1; b++)
        if (idx < gBrandStart[b + 1]) return b;
    return BRAND_CUSTOM;
}

static bool presetNameHas(int idx, const char* needle) {
    if (idx < 0 || idx >= gNumPresets || !needle) return false;
    return strstr(gPresets[idx].name, needle) != nullptr;
}

static int stockIsoForPreset(int idx) {
    if (presetNameHas(idx, "3200")) return 3200;
    if (presetNameHas(idx, "1600")) return 1600;
    if (presetNameHas(idx, "800")) return 800;
    if (presetNameHas(idx, "400")) return 400;
    if (presetNameHas(idx, "200")) return 200;
    if (presetNameHas(idx, "160")) return 160;
    if (presetNameHas(idx, "125")) return 125;
    if (presetNameHas(idx, "100")) return 100;
    if (presetNameHas(idx, "64")) return 64;
    if (presetNameHas(idx, "50")) return 50;
    if (presetNameHas(idx, "25")) return 25;
    return 400;
}

static bool isBlackAndWhitePreset(int idx) {
    return (idx >= 0 && idx <= 1) || (idx >= 7 && idx <= 8) || (idx >= 26 && idx <= 33);
}

static StockHalationClass stockHalationClassForPreset(int idx) {
    if (presetNameHas(idx, "CineStill 800T") || presetNameHas(idx, "CineStill 50D")) {
        return HALATION_STOCK_NO_REMJET;
    }
    if (presetNameHas(idx, "Portra 800") || presetNameHas(idx, "Superia 1600") ||
        presetNameHas(idx, "Pro 800Z") || presetNameHas(idx, "Ultra Max 400")) {
        return HALATION_STOCK_SUBTLE;
    }
    return HALATION_STOCK_NONE;
}

static int stockFormatForPreset(int idx) {
    int iso = stockIsoForPreset(idx);
    if (iso <= 100) return 1;     // fine/large-format leaning response
    if (iso >= 800) return 3;     // high-speed stock response
    return 2;                     // 35mm standard response
}

static GrainProfile stockGrainDefaultsForPreset(int idx) {
    bool bw = isBlackAndWhitePreset(idx);
    int iso = stockIsoForPreset(idx);
    float speed = clampf(((float)iso - 50.0f) / 1550.0f, 0.0f, 1.0f);
    GrainProfile p = {
        bw ? 0.72f + speed * 0.92f : 0.58f + speed * 0.80f,
        0.18f + speed * 0.46f,
        bw ? 0.00f : 0.18f + speed * 0.22f,
        0.82f + speed * 0.28f,
        0.48f + speed * 0.18f,
        0.64f + speed * 0.16f,
        0.28f + speed * 0.18f,
        0.78f - speed * 0.26f,
        bw ? 0.02f : 0.16f + speed * 0.18f,
        (float)(idx * 17 + 43),
        0.12f + speed * 0.34f,
        bw ? 0.70f + speed * 0.18f : 0.18f + speed * 0.10f
    };

    if (presetNameHas(idx, "CineStill 800T")) {
        p.gain = 1.28f; p.size = 0.58f; p.color = 0.48f; p.chroma = 0.38f;
        p.grainClump = 0.42f; p.grainSilverRetention = 0.18f; p.highlights = 0.40f;
    } else if (presetNameHas(idx, "CineStill 50D") || presetNameHas(idx, "Ektar 100") ||
               presetNameHas(idx, "Velvia 50") || presetNameHas(idx, "Kodakchrome 25")) {
        p.gain *= 0.72f; p.size *= 0.72f; p.color *= 0.80f; p.resolution = 0.88f;
        p.grainClump *= 0.70f;
    } else if (presetNameHas(idx, "Portra 160") || presetNameHas(idx, "Pro 160")) {
        p.gain *= 0.82f; p.size *= 0.82f; p.color *= 0.78f; p.chroma *= 0.75f;
        p.highlights *= 0.82f;
    } else if (presetNameHas(idx, "Portra 800") || presetNameHas(idx, "Superia 1600") ||
               presetNameHas(idx, "Delta 3200") || presetNameHas(idx, "T-Max 3200")) {
        p.gain *= 1.18f; p.size *= 1.12f; p.grainClump *= 1.18f; p.shadows *= 1.10f;
    } else if (presetNameHas(idx, "Tri-X") || presetNameHas(idx, "HP5")) {
        p.gain *= 1.08f; p.size *= 1.04f; p.grainClump *= 1.12f; p.grainSilverRetention = 0.86f;
    }

    return p;
}

static GrainProfile physicalGrainProfileForPreset(int idx, int choice) {
    int resolved = choice;
    if (resolved == PROFILE_AUTO) {
        int stockFormat = stockFormatForPreset(idx);
        resolved = stockFormat == 1 ? GRAIN_65MM_FINE :
                   stockFormat == 3 ? GRAIN_35MM_HIGH_ISO : GRAIN_35MM_STANDARD;
    }

    GrainProfile p = stockGrainDefaultsForPreset(idx);
    bool bw = isBlackAndWhitePreset(idx);
    if (resolved == GRAIN_65MM_FINE) {
        p.gain *= 0.72f; p.size *= 0.62f; p.color *= 0.72f;
        p.resolution = fmaxf(p.resolution, 0.86f); p.grainClump *= 0.72f; p.highlightWeight *= 0.82f;
    } else if (resolved == GRAIN_35MM_HIGH_ISO) {
        p.gain *= 1.18f; p.size *= 1.20f; p.color = bw ? 0.0f : fmaxf(p.color, 0.38f);
        p.resolution *= 0.82f; p.grainClump *= 1.18f; p.highlightWeight *= 1.10f;
    } else if (resolved == GRAIN_16MM) {
        p.gain *= 1.35f; p.size = fmaxf(p.size, 0.70f); p.grainClump *= 1.28f;
        p.resolution *= 0.72f; p.highlightWeight *= 1.14f;
    } else if (resolved == GRAIN_8MM) {
        p.gain *= 1.62f; p.size = fmaxf(p.size, 0.88f); p.grainClump *= 1.42f;
        p.resolution *= 0.60f; p.highlightWeight *= 1.22f;
    }
    if (resolved == GRAIN_CUSTOM) {
        p = stockGrainDefaultsForPreset(idx);
    }
    p.color = bw ? 0.0f : clampf(p.color, 0.0f, 1.0f);
    p.chroma = bw ? 0.02f : clampf(p.chroma, 0.0f, 1.0f);
    p.size = clampf(p.size, 0.02f, 1.0f);
    p.gain = clampf(p.gain, 0.20f, 2.4f);
    p.grainClump = clampf(p.grainClump, 0.0f, 1.0f);
    p.grainSilverRetention = clampf(p.grainSilverRetention, 0.0f, 1.0f);
    return p;
}

static GrainProfile stockGrainProfileForPreset(int idx, int choice) {
    return physicalGrainProfileForPreset(idx, choice);
}

static int resolveFormatProfile(int idx, int choice) {
    if (choice != PROFILE_AUTO) return choice;
    int stockFormat = stockFormatForPreset(idx);
    if (stockFormat == 1) return 1;
    if (stockFormat == 3) return 3;
    return 2;
}

static float halationGainForProfile(int idx, int choice, float& radiusGain, float& warmthGain) {
    int resolved = resolveFormatProfile(idx, choice);
    radiusGain = 1.0f;
    warmthGain = 1.0f;
    if (presetNameHas(idx, "CineStill")) {
        resolved = choice == PROFILE_AUTO ? HALATION_35MM_NO_REMJET : resolved;
    }
    switch (resolved) {
        case HALATION_65MM_STANDARD: radiusGain = 0.72f; warmthGain = 0.88f; return 0.58f;
        case HALATION_16MM_STANDARD: radiusGain = 1.08f; warmthGain = 0.98f; return 0.92f;
        case HALATION_8MM_STANDARD: radiusGain = 1.22f; warmthGain = 1.02f; return 1.04f;
        case HALATION_65MM_NO_REMJET: radiusGain = 1.02f; warmthGain = 1.12f; return 1.00f;
        case HALATION_35MM_NO_REMJET: radiusGain = 1.18f; warmthGain = 1.18f; return 1.25f;
        case HALATION_16MM_NO_REMJET: radiusGain = 1.34f; warmthGain = 1.22f; return 1.42f;
        case HALATION_8MM_NO_REMJET: radiusGain = 1.50f; warmthGain = 1.25f; return 1.58f;
        default: radiusGain = 1.0f; warmthGain = 1.0f; return 1.0f;
    }
}

static float stockHalationRenderAmount(int idx, int choice, float customAmount) {
    if (customAmount > 0.0001f) return customAmount;
    if (choice == PROFILE_AUTO) return 0.0f;
    StockHalationClass halationClass = stockHalationClassForPreset(idx);
    if (choice >= HALATION_65MM_NO_REMJET && choice <= HALATION_8MM_NO_REMJET) return 0.090f;
    if (halationClass == HALATION_STOCK_NO_REMJET) return 0.075f;
    if (halationClass == HALATION_STOCK_SUBTLE) return 0.032f;
    return 0.045f;
}

static float bloomGainForProfile(int idx, int choice, float& radiusGain) {
    int resolved = resolveFormatProfile(idx, choice);
    radiusGain = 1.0f;
    switch (resolved) {
        case BLOOM_65MM: radiusGain = 0.82f; return 0.78f;
        case BLOOM_16MM: radiusGain = 1.24f; return 1.18f;
        case BLOOM_8MM: radiusGain = 1.48f; return 1.36f;
        default: (void)idx; return 1.0f;
    }
}

static float damageGainForProfile(int idx, int choice, float& dustGain, float& scratchGain) {
    int resolved = resolveFormatProfile(idx, choice);
    dustGain = 1.0f;
    scratchGain = 1.0f;
    switch (resolved) {
        case DAMAGE_65MM_CLEAN: dustGain = 0.45f; scratchGain = 0.40f; return 0.40f;
        case DAMAGE_16MM_ARCHIVE: dustGain = 1.35f; scratchGain = 1.25f; return 1.25f;
        case DAMAGE_8MM_HOME: dustGain = 1.70f; scratchGain = 1.45f; return 1.65f;
        default: (void)idx; return 1.0f;
    }
}

static float breathGainForProfile(int idx, int choice, float& exposureGain, float& saturationGain) {
    int resolved = resolveFormatProfile(idx, choice);
    exposureGain = 1.0f;
    saturationGain = 1.0f;
    switch (resolved) {
        case BREATH_65MM_SUBTLE: exposureGain = 0.45f; saturationGain = 0.55f; return 0.42f;
        case BREATH_16MM_VISIBLE: exposureGain = 1.25f; saturationGain = 1.15f; return 1.22f;
        case BREATH_8MM_STRONG: exposureGain = 1.55f; saturationGain = 1.35f; return 1.55f;
        default: (void)idx; return 1.0f;
    }
}

static float weaveGainForProfile(int idx, int choice, float& xGain, float& yGain) {
    int resolved = resolveFormatProfile(idx, choice);
    xGain = 1.0f;
    yGain = 1.0f;
    switch (resolved) {
        case WEAVE_65MM_STABLE: xGain = 0.48f; yGain = 0.42f; return 0.48f;
        case WEAVE_16MM_CAMERA: xGain = 1.20f; yGain = 1.22f; return 1.16f;
        case WEAVE_8MM_PROJECTOR: xGain = 1.55f; yGain = 1.65f; return 1.55f;
        default: (void)idx; return 1.0f;
    }
}

static float overscanScaleForPreset(int choice) {
    switch (choice) {
        case OVERSCAN_ACADEMY_133: return 0.030f;
        case OVERSCAN_STANDARD_166: return 0.038f;
        case OVERSCAN_WIDESCREEN_185: return 0.046f;
        case OVERSCAN_SCOPE_239: return 0.060f;
        case OVERSCAN_SMALL_GATE: return 0.085f;
        default: return 0.0f;
    }
}

static int qualityGlowScale(int previewQuality) {
    if (previewQuality == PREVIEW_FAST) return 8;
    if (previewQuality == PREVIEW_QUALITY) return 4;
    return 6;
}

static const double* matrixForPreset(int idx) {
    if (idx < 0 || idx >= gNumPresets) return kMatIdentity;
    if (gPresets[idx].matrix) return gPresets[idx].matrix;

    switch (idx) {
        case 2:
        case 3:
        case 4:  return kMatAgfaVista100;
        case 5:  return kMatCineStill50D;
        case 6:  return kMatCineStill800T;
        case 9:  return kMatFujiPro160C;
        case 10: return kMatFujiPro400H;
        case 11: return kMatFujiPro400H;
        case 12:
        case 15:
        case 16: return kMatFujiSuperia;
        case 13:
        case 17: return kMatFujiNatura1600;
        case 14: return kMatFujiC200;
        case 18: return kMatFujiProvia100F;
        case 19: return kMatFujiVelvia50;
        case 20:
        case 21:
        case 22: return kMatFujiProvia100F;
        case 23: return kMatFujiProvia100F;
        case 24:
        case 25: return kMatFujiVelvia50;
        case 34: return kMatKodakGold200;
        case 35: return kMatKodakEktar100;
        case 36: return kMatKodakGold200;
        case 39:
        case 37:
        case 38: return kMatKodakPortra160;
        case 43:
        case 40:
        case 41:
        case 42: return kMatKodakPortra400;
        case 44: return kMatKodakPortra800;
        case 45: return kMatKodakUltramax400;
        case 46:
        case 47:
        case 48:
        case 49: return kMatKodakEktachrome100D;
        case 50:
        case 51:
        case 52: return kMatKodachrome64;
        default: return kMatIdentity;
    }
}

// ============================================================
// Globals
// ============================================================
static const OfxHost*               gHost = nullptr;
static OfxPropertySuiteV1*          gPropSuite   = nullptr;
static OfxImageEffectSuiteV1*       gEffectSuite = nullptr;
static OfxParameterSuiteV1*         gParamSuite  = nullptr;
static OfxMemorySuiteV1*            gMemorySuite = nullptr;
static OfxMultiThreadSuiteV1*       gThreadSuite = nullptr;
static OfxMessageSuiteV2*           gMessageSuite = nullptr;

// ============================================================
// Pixel Processing
// ============================================================
#define LUT_SIZE 1024

static inline float clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static float sCurveFunc(float x, float c, float s, float t) {
    float pivot = 0.18f;
    float x0 = pivot - pivot * c * 0.3f;
    float x1 = pivot + (1.0f - pivot) * c * 0.3f;
    if (x < x0) {
        float ratio = x / x0;
        return x0 * powf(clampf(ratio, 0.0f, 1.0f), 1.0f + t);
    } else if (x <= x1) {
        return x;
    } else {
        float ratio = (x - x1) / (1.0f - x1);
        return x1 + (1.0f - x1) * (1.0f - powf(clampf(1.0f - ratio, 0.0f, 1.0f), 1.0f + s));
    }
}

static void buildSCurveLUT(float* lut, float contrast, float shoulder, float toe) {
    for (int i = 0; i <= LUT_SIZE; i++) {
        float x = (float)i / (float)LUT_SIZE;
        lut[i] = sCurveFunc(x, contrast, shoulder, toe);
    }
}

static inline float lutLookup(const float* lut, float x) {
    if (x <= 0.0f) return lut[0];
    if (x >= 1.0f) return lut[LUT_SIZE];
    float fi = x * (float)LUT_SIZE;
    int i = (int)fi;
    if (i >= LUT_SIZE) return lut[LUT_SIZE];
    float frac = fi - (float)i;
    return lut[i] + (lut[i + 1] - lut[i]) * frac;
}

static inline unsigned int hashXY(int x, int y, int seed) {
    unsigned int h = (unsigned int)(x * 374761393 + y * 668265263 + seed);
    h = ((h ^ (h >> 13)) * 1274126177);
    h = (h ^ (h >> 16));
    return h;
}

static inline float grainNoise(int x, int y, int seed) {
    return (float)(hashXY(x, y, seed) & 0x7FFFFFFF) / (float)0x7FFFFFFF;
}

static inline float rec709ToLinear(float x) {
    return powf(clampf(x, 0.0f, 1.0f), 2.4f);
}

static inline float linearToRec709(float x) {
    return powf(clampf(x, 0.0f, 1.0f), 1.0f / 2.4f);
}

static inline float intermediateToLinear(float x) {
    return powf(clampf(x, 0.0f, 1.0f), 2.0f);
}

static inline float linearToIntermediate(float x) {
    return powf(clampf(x, 0.0f, 1.0f), 0.5f);
}

static inline void applyInputTransform(int transform, float& r, float& g, float& b) {
    if (transform == TRANSFORM_REC709_G24) {
        r = rec709ToLinear(r);
        g = rec709ToLinear(g);
        b = rec709ToLinear(b);
    } else if (transform == TRANSFORM_DWG_INTERMEDIATE) {
        r = intermediateToLinear(r);
        g = intermediateToLinear(g);
        b = intermediateToLinear(b);
    }
}

static inline void applyOutputTransform(int transform, float& r, float& g, float& b) {
    if (transform == TRANSFORM_REC709_G24) {
        r = linearToRec709(r);
        g = linearToRec709(g);
        b = linearToRec709(b);
    } else if (transform == TRANSFORM_DWG_INTERMEDIATE) {
        r = linearToIntermediate(r);
        g = linearToIntermediate(g);
        b = linearToIntermediate(b);
    }
}

static inline void applyFilmMatrix(const double* matrix, float strength, float& r, float& g, float& b) {
    if (!matrix || strength <= 0.001f) return;

    float mr = (float)(matrix[0] * r + matrix[1] * g + matrix[2] * b);
    float mg = (float)(matrix[3] * r + matrix[4] * g + matrix[5] * b);
    float mb = (float)(matrix[6] * r + matrix[7] * g + matrix[8] * b);
    float s = clampf(strength, 0.0f, 2.0f);

    r = r + (mr - r) * s;
    g = g + (mg - g) * s;
    b = b + (mb - b) * s;
}

static inline float fractf(float x) {
    return x - floorf(x);
}

static inline float lerpf(float a, float b, float t) {
    return a + (b - a) * t;
}

// Ported from NativeEnhancer-FE's random texture idea: sin(dot()) seeded noise.
static inline float nativeEnhancerNoise(float x, float y, float seed) {
    return fractf(sinf(x * 12.9898f + y * 78.233f + seed * 37.719f) * 43758.5453f);
}

static inline float timeSeed(double renderTime, float fps, float offset) {
    return floorf((float)renderTime * fps + offset);
}

static inline void sampleSourceBilinear(const float* src, int strideFloats, int width, int height,
                                        float fx, float fy, float& r, float& g, float& b, float& a) {
    fx = clampf(fx, 0.0f, (float)(width - 1));
    fy = clampf(fy, 0.0f, (float)(height - 1));

    int x0 = (int)floorf(fx);
    int y0 = (int)floorf(fy);
    int x1 = x0 + 1 < width ? x0 + 1 : x0;
    int y1 = y0 + 1 < height ? y0 + 1 : y0;
    float tx = fx - (float)x0;
    float ty = fy - (float)y0;

    const float* p00 = src + y0 * strideFloats + x0 * 4;
    const float* p10 = src + y0 * strideFloats + x1 * 4;
    const float* p01 = src + y1 * strideFloats + x0 * 4;
    const float* p11 = src + y1 * strideFloats + x1 * 4;

    float r0 = lerpf(p00[0], p10[0], tx);
    float g0 = lerpf(p00[1], p10[1], tx);
    float b0 = lerpf(p00[2], p10[2], tx);
    float a0 = lerpf(p00[3], p10[3], tx);
    float r1 = lerpf(p01[0], p11[0], tx);
    float g1 = lerpf(p01[1], p11[1], tx);
    float b1 = lerpf(p01[2], p11[2], tx);
    float a1 = lerpf(p01[3], p11[3], tx);

    r = lerpf(r0, r1, ty);
    g = lerpf(g0, g1, ty);
    b = lerpf(b0, b1, ty);
    a = lerpf(a0, a1, ty);
}

static inline float smoothstepf(float edge0, float edge1, float x) {
    float t = clampf((x - edge0) / fmaxf(0.0001f, edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

static inline float screenBlendf(float base, float glow, float mix) {
    float screened = 1.0f - (1.0f - base) * (1.0f - clampf(glow, 0.0f, 1.0f));
    return lerpf(base, screened, clampf(mix, 0.0f, 1.0f));
}

static inline void applyPrintResponse(float amount, float contrast, float& r, float& g, float& b) {
    if (amount <= 0.001f) return;
    float a = clampf(amount, 0.0f, 2.0f);
    float c = 1.0f + (0.22f + 0.46f * clampf(contrast, 0.0f, 1.0f)) * a;
    float pivot = 0.42f;

    r = pivot + (r - pivot) * c;
    g = pivot + (g - pivot) * c;
    b = pivot + (b - pivot) * c;

    float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    float hi = smoothstepf(0.52f, 1.08f, fmaxf(fmaxf(r, g), b));
    float sh = 1.0f - smoothstepf(0.08f, 0.48f, lum);
    r *= 1.0f + 0.055f * a * hi - 0.012f * a * sh;
    g *= 1.0f + 0.010f * a * hi - 0.010f * a * sh;
    b *= 1.0f - 0.050f * a * hi + 0.030f * a * sh;

    float density = 1.0f - 0.045f * a * (1.0f - clampf(lum, 0.0f, 1.0f));
    r *= density; g *= density; b *= density;
}

static inline void applyPushPullResponse(float pushPull, float& r, float& g, float& b) {
    if (fabsf(pushPull) <= 0.001f) return;
    float pp = clampf(pushPull, -2.0f, 2.0f);
    float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    float contrast = 1.0f + pp * 0.105f;
    float density = 1.0f - pp * 0.025f;
    r = (lum + (r - lum) * contrast) * density;
    g = (lum + (g - lum) * contrast) * density;
    b = (lum + (b - lum) * contrast) * density;
}

static inline void printStockChannelBias(int printStock, float amount,
                                         float& rBias, float& gBias, float& bBias, float& contrastBias) {
    rBias = gBias = bBias = 1.0f;
    contrastBias = 1.0f;
    float a = clampf(amount, 0.0f, 2.0f);
    if (printStock == PRINT_LINEAR || a <= 0.001f) return;
    if (printStock == PRINT_CINEON_LOG) {
        contrastBias = 0.92f;
        rBias = 1.00f; gBias = 0.99f; bBias = 1.02f;
    } else if (printStock == PRINT_KODAK_2383) {
        contrastBias = 1.10f;
        rBias = 1.035f; gBias = 1.010f; bBias = 0.965f;
    } else if (printStock == PRINT_FUJI_3513) {
        contrastBias = 1.04f;
        rBias = 0.995f; gBias = 1.025f; bBias = 1.015f;
    } else if (printStock == PRINT_KODAK_ENDURA) {
        contrastBias = 1.08f;
        rBias = 1.020f; gBias = 1.005f; bBias = 0.985f;
    } else if (printStock == PRINT_BW_BROMOPORTRAIT) {
        contrastBias = 1.12f;
        rBias = 1.0f; gBias = 1.0f; bBias = 1.0f;
    }
    rBias = lerpf(1.0f, rBias, a);
    gBias = lerpf(1.0f, gBias, a);
    bBias = lerpf(1.0f, bBias, a);
    contrastBias = lerpf(1.0f, contrastBias, a);
}

static inline void applyPrinterLights(float amount, float red, float green, float blue,
                                      float& r, float& g, float& b) {
    if (amount <= 0.001f && fabsf(red) <= 0.001f && fabsf(green) <= 0.001f && fabsf(blue) <= 0.001f) return;
    float a = clampf(amount, 0.0f, 2.0f);
    r *= powf(2.0f, clampf(red, -1.0f, 1.0f) * 0.13f * a);
    g *= powf(2.0f, clampf(green, -1.0f, 1.0f) * 0.13f * a);
    b *= powf(2.0f, clampf(blue, -1.0f, 1.0f) * 0.13f * a);
}

static inline void applyVibrance(float amount, float& r, float& g, float& b) {
    if (amount <= 0.001f) return;
    float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    float mx = fmaxf(fmaxf(r, g), b);
    float mn = fminf(fminf(r, g), b);
    float chroma = clampf(mx - mn, 0.0f, 1.0f);
    float gain = 1.0f + clampf(amount, 0.0f, 1.0f) * (0.55f * (1.0f - chroma));
    r = lum + (r - lum) * gain;
    g = lum + (g - lum) * gain;
    b = lum + (b - lum) * gain;
}

static inline void applySplitTone(float amount, float warmth, float& r, float& g, float& b) {
    if (amount <= 0.001f) return;
    float a = clampf(amount, 0.0f, 1.0f);
    float w = clampf(warmth, 0.0f, 1.0f);
    float lum = clampf(0.2126f * r + 0.7152f * g + 0.0722f * b, 0.0f, 1.0f);
    float shadow = 1.0f - smoothstepf(0.18f, 0.55f, lum);
    float high = smoothstepf(0.45f, 0.92f, lum);
    r += a * (shadow * (0.030f - 0.020f * w) + high * (0.050f * w));
    g += a * (shadow * 0.006f + high * 0.010f);
    b += a * (shadow * (0.050f * (1.0f - w)) - high * 0.030f * w);
}

static inline float sourceLumaAt(const float* src, int sS, int width, int height, int x, int y) {
    if (x < 0) x = 0; else if (x >= width) x = width - 1;
    if (y < 0) y = 0; else if (y >= height) y = height - 1;
    const float* p = src + y * sS + x * 4;
    return 0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2];
}

static inline void applyAcutance(float amount, const float* src, int sS, int width, int height, int x, int y,
                                 float& r, float& g, float& b) {
    if (fabsf(amount) <= 0.001f || !src) return;
    float center = sourceLumaAt(src, sS, width, height, x, y);
    float around = (
        sourceLumaAt(src, sS, width, height, x - 1, y) +
        sourceLumaAt(src, sS, width, height, x + 1, y) +
        sourceLumaAt(src, sS, width, height, x, y - 1) +
        sourceLumaAt(src, sS, width, height, x, y + 1)) * 0.25f;
    float hp = (center - around) * clampf(amount, -1.0f, 1.0f) * 0.55f;
    r += hp; g += hp; b += hp;
}

static inline void computeGateWeaveSample(float x, float y, int width, int height,
                                          float overscanScale, float weaveX, float weaveY,
                                          float weaveRotation,
                                          float& sampleX, float& sampleY) {
    float cx = (float)width * 0.5f;
    float cy = (float)height * 0.5f;
    float px = (x - cx) / fmaxf(0.0001f, overscanScale);
    float py = (y - cy) / fmaxf(0.0001f, overscanScale);
    float cr = cosf(weaveRotation);
    float sr = sinf(weaveRotation);
    sampleX = px * cr - py * sr + cx + weaveX;
    sampleY = px * sr + py * cr + cy + weaveY;
}

static inline void applyExpand(float amount, float blackTrim, float whiteBoost, float& r, float& g, float& b) {
    if (amount <= 0.001f) return;
    float a = clampf(amount, 0.0f, 2.0f);
    float black = clampf(blackTrim, 0.0f, 1.0f) * 0.08f * a;
    float white = 1.0f + clampf(whiteBoost, 0.0f, 1.0f) * 0.22f * a;
    float scale = 1.0f / (white - black);
    r = (r - black) * scale;
    g = (g - black) * scale;
    b = (b - black) * scale;
    float c = 1.0f + 0.26f * a;
    r = 0.5f + (r - 0.5f) * c;
    g = 0.5f + (g - 0.5f) * c;
    b = 0.5f + (b - 0.5f) * c;
}

static inline void applyColorHead(float amount, float cyan, float magenta, float yellow,
                                  float& r, float& g, float& b) {
    if (amount <= 0.001f && fabsf(cyan) <= 0.001f && fabsf(magenta) <= 0.001f && fabsf(yellow) <= 0.001f) return;
    float a = clampf(amount, 0.0f, 2.0f);
    float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    r *= powf(2.0f, (-cyan * 0.35f + 0.10f) * a);
    g *= powf(2.0f, (-magenta * 0.35f - 0.02f) * a);
    b *= powf(2.0f, (-yellow * 0.35f - 0.08f) * a);
    float keepLum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    float comp = keepLum > 0.001f ? lum / keepLum : 1.0f;
    r *= lerpf(1.0f, comp, 0.35f);
    g *= lerpf(1.0f, comp, 0.35f);
    b *= lerpf(1.0f, comp, 0.35f);
}

static inline void applyFilmBreath(float amount, float exposure, float saturation, double renderTime,
                                   float& r, float& g, float& b) {
    if (amount <= 0.001f) return;
    float a = clampf(amount, 0.0f, 2.0f);
    float seed = timeSeed(renderTime, 12.0f, 0.0f);
    float brightAmp = 0.18f * clampf(exposure, 0.0f, 1.0f) * a;
    float satAmp = 0.16f * clampf(saturation, 0.0f, 1.0f) * a;
    float bright = lerpf(1.0f - brightAmp, 1.0f + brightAmp, nativeEnhancerNoise(seed, 1.7f, 4.0f));
    float gamma = lerpf(1.0f - 0.07f * a, 1.0f + 0.07f * a, nativeEnhancerNoise(seed, 2.9f, 9.0f));
    float sat = lerpf(1.0f - satAmp, 1.0f + satAmp, nativeEnhancerNoise(seed, 4.1f, 15.0f));

    r = powf(fabsf(r * bright), 1.0f / gamma);
    g = powf(fabsf(g * bright), 1.0f / gamma);
    b = powf(fabsf(b * bright), 1.0f / gamma);
    float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    r = lum + (r - lum) * sat;
    g = lum + (g - lum) * sat;
    b = lum + (b - lum) * sat;
}

static inline void applyNativeEnhancerFilmGrain(float amount, float size, float color, int x, int y, double renderTime,
                                                float& r, float& g, float& b) {
    if (amount <= 0.001f) return;
    float a = clampf(amount, 0.0f, 2.0f);
    float frame = timeSeed(renderTime, 18.0f, 0.0f);
    float scale = 0.025f + 0.11f * a;
    float grainSize = 0.45f + 5.5f * clampf(size, 0.0f, 1.0f);
    float nx = (float)x / grainSize;
    float ny = (float)y / grainSize;

    float nr = nativeEnhancerNoise(nx + frame * 1.425f, ny, 1.0f) * 2.0f - 1.0f;
    float ng = nativeEnhancerNoise(nx, ny + frame * 3.892f, 2.0f) * 2.0f - 1.0f;
    float nb = nativeEnhancerNoise(nx + frame * 5.835f, ny + frame, 3.0f) * 2.0f - 1.0f;
    float mono = 0.2126f * nr + 0.7152f * ng + 0.0722f * nb;
    float chroma = clampf(color, 0.0f, 1.0f);
    nr = lerpf(mono, nr, chroma);
    ng = lerpf(mono, ng, chroma);
    nb = lerpf(mono, nb, chroma);
    float lum = clampf(fmaxf(fmaxf(r, g), b), 0.0f, 1.0f);
    float highlightSafe = clampf(1.20f - 0.70f * lum, 0.25f, 1.20f);
    float fine = nativeEnhancerNoise((float)x + frame * 11.0f, (float)y - frame * 7.0f, 9.0f) * 2.0f - 1.0f;

    r += (nr * scale + fine * scale * 0.25f) * highlightSafe;
    g += (ng * scale + fine * scale * 0.22f) * highlightSafe;
    b += (nb * scale + fine * scale * 0.28f) * highlightSafe;
}

static inline void applyProfiledFilmGrain(float amount, float size, float color,
                                          float shadows, float mids, float highlights,
                                          float resolution, float chroma, float grainClump,
                                          float grainSilverRetention, float grainTextureSeed, int mode,
                                          int x, int y, double renderTime,
                                          float& r, float& g, float& b) {
    if (amount <= 0.001f) return;
    float lum = clampf(0.2126f * r + 0.7152f * g + 0.0722f * b, 0.0f, 1.0f);
    float shadowW = 1.0f - smoothstepf(0.12f, 0.52f, lum);
    float highW = smoothstepf(0.52f, 0.96f, lum);
    float midW = clampf(1.0f - fabsf(lum - 0.48f) * 2.2f, 0.0f, 1.0f);
    float tonal = 0.18f +
                  shadowW * clampf(shadows, 0.0f, 1.0f) +
                  midW * clampf(mids, 0.0f, 1.0f) +
                  highW * clampf(highlights, 0.0f, 1.0f);
    float clump = clampf(grainClump, 0.0f, 1.0f);
    float silver = clampf(grainSilverRetention, 0.0f, 1.0f);
    float fineSize = clampf(size * (1.22f - 0.68f * clampf(resolution, 0.0f, 1.0f)) * (1.0f + clump * 0.28f), 0.02f, 1.0f);
    float colorMix = clampf(color + chroma * 0.32f, 0.0f, 1.0f) * (1.0f - silver * 0.76f);
    float modeGain = mode == GRAIN_MODE_DETAIL ? 0.78f : (mode == GRAIN_MODE_DYE_CLOUD ? 1.12f : 1.0f);
    if (mode == GRAIN_MODE_DYE_CLOUD) colorMix = clampf(colorMix + 0.22f, 0.0f, 1.0f);

    float frame = timeSeed(renderTime, 24.0f, grainTextureSeed);
    float clumpNoise = nativeEnhancerNoise((float)x * 0.015f + grainTextureSeed,
                                           (float)y * 0.015f + frame * 0.11f,
                                           101.0f + grainTextureSeed);
    float clumpGain = lerpf(0.88f, 1.34f, clumpNoise) * (1.0f + clump * 0.35f);
    applyNativeEnhancerFilmGrain(amount * tonal * modeGain * clumpGain, fineSize, colorMix,
                                 x + (int)grainTextureSeed, y - (int)(grainTextureSeed * 0.5f),
                                 renderTime + grainTextureSeed * 0.013, r, g, b);
}

struct GlowBuffer {
    int w, h, scale;
    float* data;
};

static void freeGlowBuffer(GlowBuffer& gb) {
    if (gb.data) free(gb.data);
    gb.data = nullptr;
    gb.w = gb.h = gb.scale = 0;
}

static bool buildGlowBuffer(GlowBuffer& gb, const float* src, int strideFloats, int width, int height,
                            float threshold, float radius, int glowKind,
                            float bloomRegion, float bloomSoftness,
                            int inputTransform,
                            int downsampleScale = 4) {
    gb.scale = downsampleScale < 2 ? 2 : (downsampleScale > 12 ? 12 : downsampleScale);
    gb.w = (width + gb.scale - 1) / gb.scale;
    gb.h = (height + gb.scale - 1) / gb.scale;
    int count = gb.w * gb.h * 3;
    gb.data = (float*)calloc((size_t)count, sizeof(float));
    float* temp = (float*)calloc((size_t)count, sizeof(float));
    if (!gb.data || !temp) {
        if (temp) free(temp);
        freeGlowBuffer(gb);
        return false;
    }

    for (int gy = 0; gy < gb.h; gy++) {
        int y = gy * gb.scale;
        if (y >= height) y = height - 1;
        for (int gx = 0; gx < gb.w; gx++) {
            int x = gx * gb.scale;
            if (x >= width) x = width - 1;
            const float* p = src + y * strideFloats + x * 4;
            float pr = p[0], pg = p[1], pb = p[2];
            applyInputTransform(inputTransform, pr, pg, pb);
            float lum = 0.2126f * pr + 0.7152f * pg + 0.0722f * pb;
            float redKey = pr * 0.92f + lum * 0.08f;
            float key = fmaxf(lum, redKey);
            float w = 0.0f;
            if (glowKind == GLOW_BLOOM) {
                key = lum;
                float bloomHighlightFloor = lerpf(0.56f, 0.92f, clampf(bloomRegion, 0.0f, 1.0f));
                float edge0 = fmaxf(threshold, bloomHighlightFloor);
                float edge1 = edge0 + lerpf(0.22f, 0.055f, clampf(bloomSoftness, 0.0f, 1.0f));
                w = smoothstepf(edge0, edge1, key);
            } else {
                w = clampf((key - threshold) / fmaxf(0.001f, 1.0f - threshold), 0.0f, 1.0f);
                w = w * w * (3.0f - 2.0f * w);
            }
            int i = (gy * gb.w + gx) * 3;
            if (glowKind == GLOW_HALATION) {
                float redLeak = fmaxf(pr, lum * 1.12f) * w;
                gb.data[i] = redLeak;
                gb.data[i + 1] = redLeak * 0.30f;
                gb.data[i + 2] = redLeak * 0.025f;
            } else {
                gb.data[i] = pr * w;
                gb.data[i + 1] = pg * w;
                gb.data[i + 2] = pb * w;
            }
        }
    }

    int passes = 1 + (int)clampf(radius * 0.25f, 0.0f, 4.0f);
    for (int pass = 0; pass < passes; pass++) {
        for (int gy = 0; gy < gb.h; gy++) {
            for (int gx = 0; gx < gb.w; gx++) {
                float ar = 0.0f, ag = 0.0f, ab = 0.0f, total = 0.0f;
                for (int oy = -1; oy <= 1; oy++) {
                    int yy = gy + oy;
                    if (yy < 0) yy = 0; else if (yy >= gb.h) yy = gb.h - 1;
                    for (int ox = -1; ox <= 1; ox++) {
                        int xx = gx + ox;
                        if (xx < 0) xx = 0; else if (xx >= gb.w) xx = gb.w - 1;
                        float wt = (ox == 0 && oy == 0) ? 2.0f : 1.0f;
                        int si = (yy * gb.w + xx) * 3;
                        ar += gb.data[si] * wt;
                        ag += gb.data[si + 1] * wt;
                        ab += gb.data[si + 2] * wt;
                        total += wt;
                    }
                }
                int di = (gy * gb.w + gx) * 3;
                temp[di] = ar / total;
                temp[di + 1] = ag / total;
                temp[di + 2] = ab / total;
            }
        }
        float* swap = gb.data;
        gb.data = temp;
        temp = swap;
    }

    free(temp);
    return true;
}

static inline void sampleGlowBuffer(const GlowBuffer& gb, float x, float y, float& r, float& g, float& b) {
    if (!gb.data || gb.w <= 0 || gb.h <= 0) { r = g = b = 0.0f; return; }
    float gx = clampf(x / (float)gb.scale, 0.0f, (float)(gb.w - 1));
    float gy = clampf(y / (float)gb.scale, 0.0f, (float)(gb.h - 1));
    int x0 = (int)floorf(gx), y0 = (int)floorf(gy);
    int x1 = x0 + 1 < gb.w ? x0 + 1 : x0;
    int y1 = y0 + 1 < gb.h ? y0 + 1 : y0;
    float tx = gx - (float)x0, ty = gy - (float)y0;
    const float* p00 = gb.data + (y0 * gb.w + x0) * 3;
    const float* p10 = gb.data + (y0 * gb.w + x1) * 3;
    const float* p01 = gb.data + (y1 * gb.w + x0) * 3;
    const float* p11 = gb.data + (y1 * gb.w + x1) * 3;
    r = lerpf(lerpf(p00[0], p10[0], tx), lerpf(p01[0], p11[0], tx), ty);
    g = lerpf(lerpf(p00[1], p10[1], tx), lerpf(p01[1], p11[1], tx), ty);
    b = lerpf(lerpf(p00[2], p10[2], tx), lerpf(p01[2], p11[2], tx), ty);
}

static inline void applyCineStillHalation(float amount, float warmth, float br, float bg, float bb,
                                          float& r, float& g, float& b) {
    if (amount <= 0.001f) return;
    float a = clampf(amount, 0.0f, 2.0f);
    float warm = clampf(warmth, 0.0f, 1.0f);
    float lum = 0.2126f * br + 0.7152f * bg + 0.0722f * bb;
    float redKey = fmaxf(br * 1.28f, lum * 0.92f);
    float mix = clampf(a * 0.52f, 0.0f, 0.85f);
    float veil = clampf(redKey * mix, 0.0f, 0.95f);
    float baseLum = clampf(0.2126f * r + 0.7152f * g + 0.0722f * b, 0.0f, 1.4f);
    float ringBias = 1.15f - 0.75f * smoothstepf(0.72f, 1.10f, baseLum);
    ringBias = clampf(ringBias, 0.32f, 1.15f);

    r += veil * ringBias * (0.96f + 0.48f * warm);
    g += veil * ringBias * (0.035f + 0.135f * warm);
    b += veil * ringBias * (0.003f + 0.020f * (1.0f - warm));

    float haloTint = clampf(veil * ringBias * (0.48f + 0.38f * warm), 0.0f, 0.72f);
    float orangeG = r * (0.24f + 0.10f * (1.0f - warm));
    float redB = r * (0.018f + 0.045f * (1.0f - warm));
    g = lerpf(g, fminf(g, orangeG), haloTint);
    b = lerpf(b, fminf(b, redB), haloTint);
}

static inline void applyHalationControls(float amount, float local, float global, float hue,
                                         float blueComp, float impact, float defringe,
                                         float& r, float& g, float& b) {
    if (amount <= 0.001f) return;
    float a = clampf(amount, 0.0f, 2.0f);
    float lum = clampf(0.2126f * r + 0.7152f * g + 0.0722f * b, 0.0f, 1.4f);
    float hi = smoothstepf(0.68f, 1.08f, lum);
    float loc = clampf(local, 0.0f, 1.0f);
    float glob = clampf(global, 0.0f, 1.0f);
    float h = clampf(hue, 0.0f, 1.0f);
    float mix = a * (0.030f + 0.120f * clampf(impact, 0.0f, 1.0f)) * hi;
    r += mix * (0.74f + 0.34f * loc + 0.08f * glob);
    g += mix * (0.065f + 0.115f * h);
    b += mix * (0.010f + 0.070f * (1.0f - h)) * (1.0f - 0.78f * clampf(blueComp, 0.0f, 1.0f));
    float chromaBias = clampf(mix * (2.20f + 1.80f * clampf(impact, 0.0f, 1.0f)), 0.0f, 0.45f);
    g = lerpf(g, fminf(g, r * (0.26f + 0.16f * (1.0f - h))), chromaBias);
    b = lerpf(b, fminf(b, r * (0.020f + 0.065f * (1.0f - h))), chromaBias);
    float fringe = clampf(defringe, 0.0f, 1.0f) * hi * a;
    b = lerpf(b, fminf(b, lum * 0.92f + r * 0.08f), fringe);
}

static inline void applyBloomDiffusion(float amount, float radius, float br, float bg, float bb,
                                       float& r, float& g, float& b) {
    if (amount <= 0.001f) return;
    float a = clampf(amount, 0.0f, 1.25f);
    float gain = 1.08f + 1.12f * clampf(radius, 0.0f, 1.0f);
    float mix = clampf(a * 0.28f, 0.0f, 0.55f);
    br = clampf(br * gain, 0.0f, 1.8f);
    bg = clampf(bg * gain, 0.0f, 1.8f);
    bb = clampf(bb * gain, 0.0f, 1.8f);
    r = screenBlendf(r, br, mix);
    g = screenBlendf(g, bg, mix);
    b = screenBlendf(b, bb, mix);

    float spill = clampf((br + bg + bb) * 0.0045f * mix, 0.0f, 0.055f);
    r += spill; g += spill; b += spill;
}

static inline void applyBloomControls(float amount, float detail, float saveLights, float defringe,
                                      float br, float bg, float bb,
                                      float& r, float& g, float& b) {
    if (amount <= 0.001f) return;
    float protect = clampf(saveLights, 0.0f, 1.0f);
    float detailKeep = clampf(detail, 0.0f, 1.0f);
    float sourceMax = fmaxf(fmaxf(br, bg), bb);
    float lightMask = smoothstepf(0.42f, 1.05f, sourceMax) * protect;
    r = lerpf(r, fmaxf(r, br), lightMask * 0.18f);
    g = lerpf(g, fmaxf(g, bg), lightMask * 0.18f);
    b = lerpf(b, fmaxf(b, bb), lightMask * 0.18f);
    float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
    r = lum + (r - lum) * (1.0f - clampf(defringe, 0.0f, 1.0f) * 0.25f);
    g = lum + (g - lum) * (1.0f - clampf(defringe, 0.0f, 1.0f) * 0.25f);
    b = lum + (b - lum) * (1.0f - clampf(defringe, 0.0f, 1.0f) * 0.25f);
    r = lerpf(lum, r, 0.72f + 0.28f * detailKeep);
    g = lerpf(lum, g, 0.72f + 0.28f * detailKeep);
    b = lerpf(lum, b, 0.72f + 0.28f * detailKeep);
}

static inline void applyVignetteMask(float amount, float radiusParam, float featherParam,
                                     float nx, float ny,
                                     float& r, float& g, float& b) {
    if (amount <= 0.001f) return;
    float dx = (nx - 0.5f) * 2.0f;
    float dy = (ny - 0.5f) * 2.0f;
    float d = sqrtf(dx * dx + dy * dy);
    float radius = lerpf(0.22f, 1.12f, clampf(radiusParam, 0.0f, 1.0f));
    float feather = lerpf(0.10f, 0.95f, clampf(featherParam, 0.0f, 1.0f));
    float edge = smoothstepf(radius, radius + feather, d);
    float vf = 1.0f - clampf(amount * 0.95f, 0.0f, 1.6f) * edge;
    r *= vf; g *= vf; b *= vf;
}

static inline void applyDustAndScratches(float amount, float dustAmount, float scratchAmount,
                                         int x, int y, double renderTime,
                                         float& r, float& g, float& b) {
    if (amount <= 0.001f) return;
    float a = clampf(amount, 0.0f, 2.0f);
    float dustA = clampf(dustAmount, 0.0f, 1.0f);
    float scratchA = clampf(scratchAmount, 0.0f, 1.0f);
    float frame = timeSeed(renderTime, 10.0f, 0.0f);
    float dust = nativeEnhancerNoise((float)x, (float)y + frame * 13.0f, 31.0f);
    if (dust > 1.0f - 0.0120f * a * dustA) {
        float polarity = nativeEnhancerNoise((float)y, frame, 34.0f);
        if (polarity > 0.58f) {
            float spot = lerpf(0.76f, 1.55f, polarity);
            r *= spot; g *= spot; b *= spot;
        } else {
            float spot = lerpf(0.42f, 0.86f, polarity);
            r *= spot; g *= spot; b *= spot;
        }
    }

    float scratchColumn = nativeEnhancerNoise(floorf((float)x * 0.04f), frame, 57.0f);
    float scratchNoise = nativeEnhancerNoise((float)y * 0.35f, frame, 61.0f);
    if (scratchColumn > 1.0f - 0.080f * a * scratchA && scratchNoise > 0.18f) {
        float line = scratchNoise > 0.72f ? (1.0f + 0.80f * a * scratchA)
                                          : (1.0f - 0.35f * a * scratchA);
        r *= line; g *= line; b *= line;
    }
}

static inline void applyFilmDamage(float amount, float dustAmount, float scratchAmount,
                                   int x, int y, double renderTime,
                                   float& r, float& g, float& b) {
    applyDustAndScratches(amount, dustAmount, scratchAmount, x, y, renderTime, r, g, b);
}

static inline void applyLicenseWatermark(int width, int height, int x, int y,
                                         float& r, float& g, float& b) {
    int diagonal = (x + y) % 112;
    bool stripe = diagonal < 6;
    bool lowerBand = y > (height * 7) / 8;
    bool badge = lowerBand && x > width / 24 && x < width / 3;
    if (stripe) {
        r = lerpf(r, 1.0f, 0.20f);
        g = lerpf(g, 0.10f, 0.20f);
        b = lerpf(b, 0.04f, 0.20f);
    }
    if (badge) {
        bool edge = y < (height * 7) / 8 + 4 || y > height - 8 || x < width / 24 + 4 || x > width / 3 - 4;
        float mix = edge ? 0.55f : 0.25f;
        r = lerpf(r, 1.0f, mix);
        g = lerpf(g, 0.78f, mix);
        b = lerpf(b, 0.24f, mix);
    }
}

struct RenderThreadArgs {
    const float* __restrict src;
    float* __restrict dst;
    int sS;
    int dS;
    int width;
    int height;
    int inputTransform;
    int outputTransform;
    float invW;
    float invH;
    bool doDensity;
    bool doSat;
    bool doHalation;
    bool doGrain;
    bool doVignette;
    bool doExpand;
    bool doColorHead;
    bool doBloom;
    bool doDamage;
    bool doBreath;
    bool doSpatial;
    float densityFactor;
    float grainScale;
    float halThreshold;
    float halScale;
    float overscanScale;
    float weaveX;
    float weaveY;
    float weaveRotation;
    int printStock;
    int filmGrainMode;
    int effectMode;
    float fExpand;
    float fExpandBlack;
    float fExpandWhite;
    float fPushPull;
    float fAcutance;
    float fBreath;
    float fFilmBreathExposure;
    float fFilmBreathSaturation;
    float fPrintCustom;
    float fPrintContrast;
    float fColorDensity;
    float fVibrance;
    float fPrinterRed;
    float fPrinterGreen;
    float fPrinterBlue;
    float fSplitToneAmount;
    float fSplitToneWarmth;
    const float* sCurveLUT;
    const double* filmMatrix;
    float fProfile;
    float fColorHead;
    float fColorHeadCyan;
    float fColorHeadMagenta;
    float fColorHeadYellow;
    float fSat;
    const GlowBuffer* halationGlow;
    const GlowBuffer* bloomGlow;
    bool haveHalationGlow;
    bool haveBloomGlow;
    float fH;
    float fHalationWarmth;
    float fBloom;
    float fBloomRadius;
    float fBloomDetail;
    float fBloomSaveLights;
    float fBloomDefringe;
    float fBloomRegion;
    float fBloomSoftness;
    bool showBloomMask;
    float fGr;
    float fFilmGrainSize;
    float fFilmGrainColor;
    float fFilmGrainShadows;
    float fFilmGrainMids;
    float fFilmGrainHighlights;
    float fFilmGrainResolution;
    float fFilmGrainChroma;
    float fFilmGrainClump;
    float fFilmGrainSilverRetention;
    float fFilmGrainTextureSeed;
    float fHalationLocal;
    float fHalationGlobal;
    float fHalationHue;
    float fHalationBlueComp;
    float fHalationImpact;
    float fHalationDefringe;
    float fV;
    float fVignetteRadius;
    float fVignetteFeather;
    float fDamage;
    float fFilmDust;
    float fFilmScratch;
    double renderTime;
    bool applyLicenseWatermark;
};

static void renderRows(const RenderThreadArgs& args, int yBegin, int yEnd) {
    for (int y = yBegin; y < yEnd; y++) {
        float ny = ((float)y + 0.5f) * args.invH;

        for (int x = 0; x < args.width; x++) {
            int si = y * args.sS + x * 4;
            int di = y * args.dS + x * 4;
            float sampleX = (float)x;
            float sampleY = (float)y;
            if (args.doSpatial) {
                computeGateWeaveSample((float)x, (float)y, args.width, args.height,
                                       args.overscanScale, args.weaveX, args.weaveY, args.weaveRotation,
                                       sampleX, sampleY);
            }

            float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
            if (args.doSpatial) {
                sampleSourceBilinear(args.src, args.sS, args.width, args.height, sampleX, sampleY, r, g, b, a);
            } else {
                r = args.src[si]; g = args.src[si+1]; b = args.src[si+2]; a = args.src[si+3];
            }

            applyInputTransform(args.inputTransform, r, g, b);
            if (args.fPushPull != 0.0f) applyPushPullResponse(args.fPushPull, r, g, b);
            if (args.doExpand) applyExpand(args.fExpand, args.fExpandBlack, args.fExpandWhite, r, g, b);
            if (args.doBreath) applyFilmBreath(args.fBreath, args.fFilmBreathExposure, args.fFilmBreathSaturation, args.renderTime, r, g, b);

            r = lutLookup(args.sCurveLUT, r);
            g = lutLookup(args.sCurveLUT, g);
            b = lutLookup(args.sCurveLUT, b);
            float stockR, stockG, stockB, stockContrast;
            printStockChannelBias(args.printStock, args.fPrintCustom, stockR, stockG, stockB, stockContrast);
            if (args.printStock == PRINT_BW_BROMOPORTRAIT && args.fPrintCustom > 0.001f) {
                float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                r = lerpf(r, lum, args.fPrintCustom);
                g = lerpf(g, lum, args.fPrintCustom);
                b = lerpf(b, lum, args.fPrintCustom);
            }
            r *= stockR; g *= stockG; b *= stockB;
            applyPrintResponse(args.fPrintCustom, args.fPrintContrast * stockContrast, r, g, b);
            applyPrinterLights(args.fPrintCustom, args.fPrinterRed, args.fPrinterGreen, args.fPrinterBlue, r, g, b);

            if (args.doDensity) {
                float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                if (lum > 1.0f) lum = 1.0f;
                float sat = 1.0f - args.densityFactor * (1.0f - lum);
                r = lum + (r - lum) * sat;
                g = lum + (g - lum) * sat;
                b = lum + (b - lum) * sat;
            }

            applyFilmMatrix(args.filmMatrix, args.fProfile, r, g, b);
            if (args.doColorHead) applyColorHead(args.fColorHead, args.fColorHeadCyan, args.fColorHeadMagenta, args.fColorHeadYellow, r, g, b);
            if (args.fColorDensity != 0.0f) {
                float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                float densitySat = 1.0f + args.fColorDensity;
                r = lum + (r - lum) * densitySat;
                g = lum + (g - lum) * densitySat;
                b = lum + (b - lum) * densitySat;
            }

            if (args.doSat) {
                float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                r = lum + (r - lum) * args.fSat;
                g = lum + (g - lum) * args.fSat;
                b = lum + (b - lum) * args.fSat;
            }
            applyVibrance(args.fVibrance, r, g, b);
            applySplitTone(args.fSplitToneAmount, args.fSplitToneWarmth, r, g, b);

            if (args.doHalation) {
                float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                if (lum > args.halThreshold) {
                    float ha = (lum - args.halThreshold) * args.halScale;
                    r += ha * (0.95f + 0.35f * args.fHalationWarmth);
                    g += ha * (0.18f + 0.26f * args.fHalationWarmth);
                    b += ha * (0.03f + 0.12f * (1.0f - args.fHalationWarmth));
                }
                if (args.haveHalationGlow) {
                    float br, bg, bb;
                    sampleGlowBuffer(*args.halationGlow, sampleX, sampleY, br, bg, bb);
                    applyCineStillHalation(args.fH, args.fHalationWarmth, br, bg, bb, r, g, b);
                }
                applyHalationControls(args.fH, args.fHalationLocal, args.fHalationGlobal, args.fHalationHue,
                                      args.fHalationBlueComp, args.fHalationImpact, args.fHalationDefringe,
                                      r, g, b);
            }

            if (args.doBloom && args.haveBloomGlow) {
                float br, bg, bb;
                sampleGlowBuffer(*args.bloomGlow, sampleX, sampleY, br, bg, bb);
                if (args.showBloomMask) {
                    float mask = clampf(fmaxf(fmaxf(br, bg), bb) * 2.0f, 0.0f, 1.0f);
                    r = g = b = mask;
                } else {
                    applyBloomDiffusion(args.fBloom, args.fBloomRadius, br, bg, bb, r, g, b);
                    applyBloomControls(args.fBloom, args.fBloomDetail, args.fBloomSaveLights, args.fBloomDefringe,
                                       br, bg, bb, r, g, b);
                }
            }

            if (!args.showBloomMask && args.doGrain) {
                float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                if (lum > 1.0f) lum = 1.0f;
                float lf = 0.05f + 0.9f * lum;
                r += (grainNoise(x, y, 42) - 0.5f) * args.grainScale * lf;
                g += (grainNoise(x, y, 137) - 0.5f) * args.grainScale * lf;
                b += (grainNoise(x, y, 293) - 0.5f) * args.grainScale * lf;
                applyProfiledFilmGrain(args.fGr, args.fFilmGrainSize, args.fFilmGrainColor,
                                       args.fFilmGrainShadows, args.fFilmGrainMids, args.fFilmGrainHighlights,
                                       args.fFilmGrainResolution, args.fFilmGrainChroma,
                                       args.fFilmGrainClump, args.fFilmGrainSilverRetention,
                                       args.fFilmGrainTextureSeed, args.filmGrainMode,
                                       x, y, args.renderTime, r, g, b);
            }

            if (!args.showBloomMask && args.fAcutance != 0.0f)
                applyAcutance(args.fAcutance, args.src, args.sS, args.width, args.height, x, y, r, g, b);

            if (!args.showBloomMask && args.doVignette) {
                float nx = ((float)x + 0.5f) * args.invW;
                applyVignetteMask(args.fV, args.fVignetteRadius, args.fVignetteFeather, nx, ny, r, g, b);
            }

            if (r < 0.0f) r = 0.0f; else if (r > 1.0f) r = 1.0f;
            if (g < 0.0f) g = 0.0f; else if (g > 1.0f) g = 1.0f;
            if (b < 0.0f) b = 0.0f; else if (b > 1.0f) b = 1.0f;
            if (!args.showBloomMask && args.doDamage) applyFilmDamage(args.fDamage, args.fFilmDust, args.fFilmScratch, x, y, args.renderTime, r, g, b);

            applyOutputTransform(args.outputTransform, r, g, b);
            if (args.applyLicenseWatermark) applyLicenseWatermark(args.width, args.height, x, y, r, g, b);

            args.dst[di]   = clampf(r, 0.0f, 1.0f);
            args.dst[di+1] = clampf(g, 0.0f, 1.0f);
            args.dst[di+2] = clampf(b, 0.0f, 1.0f);
            args.dst[di+3] = a;
        }
    }
}

static void renderThreadFunction(unsigned int threadIndex, unsigned int threadMax, void* customArg) {
    RenderThreadArgs* args = (RenderThreadArgs*)customArg;
    int rowsPerThread = (args->height + (int)threadMax - 1) / (int)threadMax;
    int yBegin = (int)threadIndex * rowsPerThread;
    int yEnd = yBegin + rowsPerThread;
    if (yEnd > args->height) yEnd = args->height;
    if (yBegin < yEnd) renderRows(*args, yBegin, yEnd);
}

// ============================================================
// Forward Declarations
// ============================================================
#ifdef __cplusplus
extern "C" {
#endif

static OfxStatus describeAction(OfxImageEffectHandle effect);
static OfxStatus describeInContextAction(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs);
static OfxStatus renderAction(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs);
static OfxStatus instanceChangedAction(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs,
                                        OfxPropertySetHandle outArgs);
static OfxStatus pluginMain(const char* action, const void* handle,
                             OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs);

static OfxPlugin gPlugin;

OfxExport int OfxGetNumberOfPlugins(void) { return 1; }

OfxExport OfxPlugin* OfxGetPlugin(int nth) {
    if (nth != 0) return nullptr;
    gPlugin.pluginApi = kOfxImageEffectPluginApi;
    gPlugin.apiVersion = 1;
    gPlugin.pluginIdentifier = PLUGIN_UID;
    gPlugin.pluginVersionMajor = 5;
    gPlugin.pluginVersionMinor = 0;
    gPlugin.setHost = reinterpret_cast<void(*)(OfxHost*)>(OfxSetHost);
    gPlugin.mainEntry = pluginMain;
    return &gPlugin;
}

OfxExport OfxStatus OfxSetHost(const OfxHost* host) {
    gHost = host;
    if (!gHost) {
        gPropSuite   = nullptr;
        gEffectSuite = nullptr;
        gParamSuite  = nullptr;
        gMemorySuite = nullptr;
        gThreadSuite = nullptr;
        gMessageSuite = nullptr;
        return kOfxStatOK;
    }
    gPropSuite   = (OfxPropertySuiteV1*)gHost->fetchSuite(gHost->host, kOfxPropertySuite, 1);
    gEffectSuite = (OfxImageEffectSuiteV1*)gHost->fetchSuite(gHost->host, kOfxImageEffectSuite, 1);
    gParamSuite  = (OfxParameterSuiteV1*)gHost->fetchSuite(gHost->host, kOfxParameterSuite, 1);
    gMemorySuite = (OfxMemorySuiteV1*)gHost->fetchSuite(gHost->host, kOfxMemorySuite, 1);
    gThreadSuite = (OfxMultiThreadSuiteV1*)gHost->fetchSuite(gHost->host, kOfxMultiThreadSuite, 1);
    gMessageSuite = (OfxMessageSuiteV2*)gHost->fetchSuite(gHost->host, kOfxMessageSuite, 2);
    return kOfxStatOK;
}

#ifdef __cplusplus
}
#endif

// ============================================================
// Helpers
// ============================================================
static void markParamRealtime(OfxPropertySetHandle h) {
    gPropSuite->propSetInt(h, kOfxParamPropEvaluateOnChange, 0, 1);
    gPropSuite->propSetString(h, kOfxParamPropCacheInvalidation, 0, kOfxParamInvalidateValueChange);
}

static void describeGpuCapabilities(OfxPropertySetHandle props) {
    gPropSuite->propSetString(props, kOfxImageEffectPropCPURenderSupported, 0, "true");
#if DUXUN_ENABLE_OPENCL_RENDER
    gPropSuite->propSetString(props, kOfxImageEffectPropOpenCLRenderSupported, 0, "true");
    gPropSuite->propSetString(props, kOfxImageEffectPropOpenCLSupported, 0, "false");
#else
    gPropSuite->propSetString(props, kOfxImageEffectPropOpenCLRenderSupported, 0, "false");
    gPropSuite->propSetString(props, kOfxImageEffectPropOpenCLSupported, 0, "false");
#endif
#if DUXUN_ENABLE_CUDA_RENDER
    gPropSuite->propSetString(props, kOfxImageEffectPropCudaRenderSupported, 0, "true");
    gPropSuite->propSetString(props, kOfxImageEffectPropCudaStreamSupported, 0, "true");
#else
    gPropSuite->propSetString(props, kOfxImageEffectPropCudaRenderSupported, 0, "false");
    gPropSuite->propSetString(props, kOfxImageEffectPropCudaStreamSupported, 0, "false");
#endif
}

struct GpuRenderRequest {
    bool requested;
    bool available;
    bool openCL;
    bool cuda;
    void* openCLQueue;
    void* cudaStream;
};

static GpuRenderRequest readGpuRenderRequest(OfxPropertySetHandle inArgs,
                                             OfxPropertySetHandle srcImg,
                                             OfxPropertySetHandle dstImg) {
    GpuRenderRequest request = { false, false, false, false, nullptr, nullptr };
    int openCLEnabled = 0;
    int cudaEnabled = 0;

    if (gPropSuite->propGetInt(inArgs, kOfxImageEffectPropOpenCLEnabled, 0, &openCLEnabled) == kOfxStatOK && openCLEnabled) {
        request.requested = true;
#if DUXUN_ENABLE_OPENCL_RENDER
        void* queue = nullptr;
        gPropSuite->propGetPointer(inArgs, kOfxImageEffectPropOpenCLCommandQueue, 0, &queue);
        request.openCL = true;
        request.openCLQueue = queue;
        request.available = (queue != nullptr && srcImg != nullptr && dstImg != nullptr);
#endif
    }

    if (gPropSuite->propGetInt(inArgs, kOfxImageEffectPropCudaEnabled, 0, &cudaEnabled) == kOfxStatOK && cudaEnabled) {
        request.requested = true;
#if DUXUN_ENABLE_CUDA_RENDER
        void* stream = nullptr;
        gPropSuite->propGetPointer(inArgs, kOfxImageEffectPropCudaStream, 0, &stream);
        request.cuda = true;
        request.cudaStream = stream;
        request.available = (srcImg != nullptr && dstImg != nullptr);
#endif
    }

    return request;
}

static const int DUXUN_GPU_LOG_MAX_WRITES = 256;

struct GpuRenderTiming {
    double setupMs;
    double kernelMs;
    bool coldStart;
};

struct GpuRenderAttempt {
    const char* phase;
    const char* reason;
    int width;
    int height;
    bool requested;
    bool available;
    bool cuda;
    bool openCL;
    bool doHalation;
    bool doBloom;
    bool doDamage;
    bool doSpatial;
    long long pixels;
    const char* sizeClass;
    double elapsedMs;
    double setupMs;
    double kernelMs;
    bool coldStart;
    int requestIndex;
    int successCount;
    int fallbackCount;
};

static std::mutex gGpuLogMutex;
static int gGpuLogWrites = 0;
static int gGpuRequestCount = 0;
static int gGpuSuccessCount = 0;
static int gGpuFallbackCount = 0;
static std::atomic<int> gCudaSuccessfulRenderCount{0};

static const char* classifyGpuRenderSize(int width, int height) {
    long long pixels = (long long)width * (long long)height;
    if (pixels <= 20000) return "thumbnail";
    if (pixels <= 600000) return "preview";
    if (pixels >= 8000000) return "full_4k";
    return "full";
}

static double gpuElapsedMilliseconds(const std::chrono::steady_clock::time_point& start) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

static void logGpuEvent(const char* phase, const GpuRenderRequest& request,
                        const RenderThreadArgs* args, const char* reason,
                        double elapsedMs = -1.0,
                        const GpuRenderTiming* timing = nullptr) {
#ifdef _WIN32
    std::lock_guard<std::mutex> lock(gGpuLogMutex);
    if (gGpuLogWrites >= DUXUN_GPU_LOG_MAX_WRITES) return;
    if (phase && strcmp(phase, "request") == 0) gGpuRequestCount++;
    if (phase && strcmp(phase, "cuda_success") == 0) gGpuSuccessCount++;
    if (phase && strcmp(phase, "cuda_fallback") == 0) gGpuFallbackCount++;
    char path[MAX_PATH] = {};
    DWORD len = GetTempPathA(MAX_PATH, path);
    if (len == 0 || len >= MAX_PATH) {
        strcpy_s(path, MAX_PATH, ".\\");
    }
    strcat_s(path, MAX_PATH, "DuXunFilmSim-gpu.log");
    FILE* f = nullptr;
    fopen_s(&f, path, "a");
    if (!f) return;
    GpuRenderAttempt attempt = {
        phase ? phase : "unknown",
        reason ? reason : "",
        args ? args->width : 0,
        args ? args->height : 0,
        request.requested,
        request.available,
        request.cuda,
        request.openCL,
        args ? args->doHalation : false,
        args ? args->doBloom : false,
        args ? args->doDamage : false,
        args ? args->doSpatial : false,
        args ? (long long)args->width * (long long)args->height : 0LL,
        classifyGpuRenderSize(args ? args->width : 0, args ? args->height : 0),
        elapsedMs,
        timing ? timing->setupMs : -1.0,
        timing ? timing->kernelMs : -1.0,
        timing ? timing->coldStart : false,
        gGpuRequestCount,
        gGpuSuccessCount,
        gGpuFallbackCount
    };
    fprintf(f, "phase=%s reason=%s width=%d height=%d pixels=%lld sizeClass=%s elapsedMs=%.3f setupMs=%.3f kernelMs=%.3f coldStart=%d requested=%d available=%d cuda=%d opencl=%d doHalation=%d doBloom=%d doDamage=%d doSpatial=%d requestIndex=%d successCount=%d fallbackCount=%d\n",
            attempt.phase, attempt.reason, attempt.width, attempt.height,
            attempt.pixels, attempt.sizeClass, attempt.elapsedMs,
            attempt.setupMs, attempt.kernelMs, attempt.coldStart ? 1 : 0,
            attempt.requested ? 1 : 0, attempt.available ? 1 : 0,
            attempt.cuda ? 1 : 0, attempt.openCL ? 1 : 0,
            attempt.doHalation ? 1 : 0,
            attempt.doBloom ? 1 : 0, attempt.doDamage ? 1 : 0, attempt.doSpatial ? 1 : 0,
            attempt.requestIndex, attempt.successCount, attempt.fallbackCount);
    fclose(f);
    gGpuLogWrites++;
#else
    (void)phase; (void)request; (void)args; (void)reason; (void)elapsedMs; (void)timing;
#endif
}

static const char* kCudaKernelSource = R"CUDA(
extern "C" __device__ float duxunClamp(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

extern "C" __device__ float duxunLerp(float a, float b, float t) {
    return a + (b - a) * t;
}

extern "C" __device__ float duxunFract(float x) {
    return x - floorf(x);
}

extern "C" __device__ unsigned int duxunHash(int x, int y, int seed) {
    unsigned int h = (unsigned int)(x * 374761393 + y * 668265263 + seed);
    h = ((h ^ (h >> 13)) * 1274126177);
    h = (h ^ (h >> 16));
    return h;
}

extern "C" __device__ float duxunHashNoise(int x, int y, int seed) {
    return (float)(duxunHash(x, y, seed) & 0x7fffffff) / 2147483647.0f;
}

extern "C" __device__ void duxunSampleBilinear(const float* src, int strideFloats, int width, int height,
                                               float sx, float sy,
                                               float* r, float* g, float* b, float* a) {
    sx = duxunClamp(sx, 0.0f, (float)(width - 1));
    sy = duxunClamp(sy, 0.0f, (float)(height - 1));
    int x0 = (int)floorf(sx);
    int y0 = (int)floorf(sy);
    int x1 = x0 + 1 < width ? x0 + 1 : x0;
    int y1 = y0 + 1 < height ? y0 + 1 : y0;
    float tx = sx - (float)x0;
    float ty = sy - (float)y0;
    int i00 = y0 * strideFloats + x0 * 4;
    int i10 = y0 * strideFloats + x1 * 4;
    int i01 = y1 * strideFloats + x0 * 4;
    int i11 = y1 * strideFloats + x1 * 4;
    float r0 = duxunLerp(src[i00 + 0], src[i10 + 0], tx);
    float g0 = duxunLerp(src[i00 + 1], src[i10 + 1], tx);
    float b0 = duxunLerp(src[i00 + 2], src[i10 + 2], tx);
    float a0 = duxunLerp(src[i00 + 3], src[i10 + 3], tx);
    float r1 = duxunLerp(src[i01 + 0], src[i11 + 0], tx);
    float g1 = duxunLerp(src[i01 + 1], src[i11 + 1], tx);
    float b1 = duxunLerp(src[i01 + 2], src[i11 + 2], tx);
    float a1 = duxunLerp(src[i01 + 3], src[i11 + 3], tx);
    *r = duxunLerp(r0, r1, ty);
    *g = duxunLerp(g0, g1, ty);
    *b = duxunLerp(b0, b1, ty);
    *a = duxunLerp(a0, a1, ty);
}

extern "C" __device__ float duxunNativeNoise(float x, float y, float seed) {
    return duxunFract(sinf(x * 12.9898f + y * 78.233f + seed * 37.719f) * 43758.5453f);
}

extern "C" __device__ float duxunSCurve(float x, float c, float s, float t) {
    float pivot = 0.18f;
    float x0 = pivot - pivot * c * 0.3f;
    float x1 = pivot + (1.0f - pivot) * c * 0.3f;
    x = duxunClamp(x, 0.0f, 1.0f);
    if (x < x0) {
        float ratio = x / fmaxf(x0, 0.0001f);
        return x0 * powf(duxunClamp(ratio, 0.0f, 1.0f), 1.0f + t);
    }
    if (x <= x1) return x;
    float ratio = (x - x1) / fmaxf(1.0f - x1, 0.0001f);
    return x1 + (1.0f - x1) * (1.0f - powf(duxunClamp(1.0f - ratio, 0.0f, 1.0f), 1.0f + s));
}
)CUDA"
R"CUDA(

extern "C" __device__ void duxunCudaApplyInputTransform(int inputTransform, float* r, float* g, float* b) {
    if (inputTransform == 1) {
        *r = powf(duxunClamp(*r, 0.0f, 1.0f), 2.4f);
        *g = powf(duxunClamp(*g, 0.0f, 1.0f), 2.4f);
        *b = powf(duxunClamp(*b, 0.0f, 1.0f), 2.4f);
    } else if (inputTransform == 2) {
        *r = powf(duxunClamp(*r, 0.0f, 1.0f), 2.0f);
        *g = powf(duxunClamp(*g, 0.0f, 1.0f), 2.0f);
        *b = powf(duxunClamp(*b, 0.0f, 1.0f), 2.0f);
    }
}

extern "C" __device__ void duxunCudaApplyOutputTransform(int outputTransform, float* r, float* g, float* b) {
    if (outputTransform == 1) {
        *r = powf(duxunClamp(*r, 0.0f, 1.0f), 1.0f / 2.4f);
        *g = powf(duxunClamp(*g, 0.0f, 1.0f), 1.0f / 2.4f);
        *b = powf(duxunClamp(*b, 0.0f, 1.0f), 1.0f / 2.4f);
    } else if (outputTransform == 2) {
        *r = powf(duxunClamp(*r, 0.0f, 1.0f), 0.5f);
        *g = powf(duxunClamp(*g, 0.0f, 1.0f), 0.5f);
        *b = powf(duxunClamp(*b, 0.0f, 1.0f), 0.5f);
    }
}

extern "C" __device__ float duxunCudaSmoothstep(float edge0, float edge1, float x) {
    float t = duxunClamp((x - edge0) / fmaxf(edge1 - edge0, 0.0001f), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

extern "C" __device__ float duxunCudaScreen(float base, float glow, float mix) {
    float screened = 1.0f - (1.0f - duxunClamp(base, 0.0f, 1.0f)) * (1.0f - duxunClamp(glow, 0.0f, 1.0f));
    return duxunLerp(base, screened, duxunClamp(mix, 0.0f, 1.0f));
}

extern "C" __global__ void cudaHighlightMaskKernel(
    const float* src, float* mask,
    int srcWidth, int srcHeight, int srcStrideFloats,
    int glowWidth, int glowHeight, int glowKind,
    float threshold, float bloomRegion, float bloomSoftness,
    int inputTransform)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= glowWidth || y >= glowHeight) return;

    float sx = ((float)x + 0.5f) * (float)srcWidth / fmaxf((float)glowWidth, 1.0f) - 0.5f;
    float sy = ((float)y + 0.5f) * (float)srcHeight / fmaxf((float)glowHeight, 1.0f) - 0.5f;
    float pr, pg, pb, pa;
    duxunSampleBilinear(src, srcStrideFloats, srcWidth, srcHeight, sx, sy, &pr, &pg, &pb, &pa);
    duxunCudaApplyInputTransform(inputTransform, &pr, &pg, &pb);

    float lum = 0.2126f * pr + 0.7152f * pg + 0.0722f * pb;
    float key = fmaxf(lum, pr * 0.92f + lum * 0.08f);
    float w = 0.0f;
    if (glowKind == 1) {
        key = lum;
        float floorValue = duxunLerp(0.56f, 0.92f, duxunClamp(bloomRegion, 0.0f, 1.0f));
        float edge0 = fmaxf(threshold, floorValue);
        float edge1 = edge0 + duxunLerp(0.22f, 0.055f, duxunClamp(bloomSoftness, 0.0f, 1.0f));
        w = duxunCudaSmoothstep(edge0, edge1, key);
    } else {
        w = duxunClamp((key - threshold) / fmaxf(0.001f, 1.0f - threshold), 0.0f, 1.0f);
        w = w * w * (3.0f - 2.0f * w);
    }
    int i = (y * glowWidth + x) * 3;
    if (glowKind == 0) {
        float redLeak = fmaxf(pr, lum * 1.12f) * w;
        mask[i + 0] = redLeak;
        mask[i + 1] = redLeak * 0.30f;
        mask[i + 2] = redLeak * 0.025f;
    } else {
        mask[i + 0] = pr * w;
        mask[i + 1] = pg * w;
        mask[i + 2] = pb * w;
    }
}

extern "C" __global__ void cudaDownsampleKernel(const float* src, float* dst, int width, int height) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;
    int i = (y * width + x) * 3;
    dst[i + 0] = src[i + 0];
    dst[i + 1] = src[i + 1];
    dst[i + 2] = src[i + 2];
}

extern "C" __global__ void cudaBlurHorizontalKernel(const float* src, float* dst, int width, int height, int radius) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;
    radius = radius < 1 ? 1 : (radius > 12 ? 12 : radius);
    float ar = 0.0f, ag = 0.0f, ab = 0.0f, total = 0.0f;
    for (int ox = -12; ox <= 12; ox++) {
        if (ox < -radius || ox > radius) continue;
        int xx = x + ox;
        if (xx < 0) xx = 0;
        if (xx >= width) xx = width - 1;
        int aox = ox < 0 ? -ox : ox;
        float wt = (float)(radius + 1 - aox);
        int si = (y * width + xx) * 3;
        ar += src[si + 0] * wt;
        ag += src[si + 1] * wt;
        ab += src[si + 2] * wt;
        total += wt;
    }
    int di = (y * width + x) * 3;
    dst[di + 0] = ar / fmaxf(total, 0.0001f);
    dst[di + 1] = ag / fmaxf(total, 0.0001f);
    dst[di + 2] = ab / fmaxf(total, 0.0001f);
}

extern "C" __global__ void cudaBlurVerticalKernel(const float* src, float* dst, int width, int height, int radius) {
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;
    radius = radius < 1 ? 1 : (radius > 12 ? 12 : radius);
    float ar = 0.0f, ag = 0.0f, ab = 0.0f, total = 0.0f;
    for (int oy = -12; oy <= 12; oy++) {
        if (oy < -radius || oy > radius) continue;
        int yy = y + oy;
        if (yy < 0) yy = 0;
        if (yy >= height) yy = height - 1;
        int aoy = oy < 0 ? -oy : oy;
        float wt = (float)(radius + 1 - aoy);
        int si = (yy * width + x) * 3;
        ar += src[si + 0] * wt;
        ag += src[si + 1] * wt;
        ab += src[si + 2] * wt;
        total += wt;
    }
    int di = (y * width + x) * 3;
    dst[di + 0] = ar / fmaxf(total, 0.0001f);
    dst[di + 1] = ag / fmaxf(total, 0.0001f);
    dst[di + 2] = ab / fmaxf(total, 0.0001f);
}

extern "C" __global__ void cudaCompositeGlowKernel(
    float* dst, const float* bloomGlow, const float* halationGlow,
    int width, int height, int dstStrideFloats, int glowWidth, int glowHeight,
    int enableBloom, int enableHalation, float fH, float fHalationWarmth,
    float fHalationLocal, float fHalationGlobal, float fHalationHue,
    float fHalationBlueComp, float fHalationImpact, float fHalationDefringe,
    float fBloom, float fBloomRadius, float fBloomDetail, float fBloomSaveLights,
    float fBloomDefringe, int showBloomMask)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;
    float gx = duxunClamp(((float)x + 0.5f) * (float)glowWidth / fmaxf((float)width, 1.0f), 0.0f, (float)(glowWidth - 1));
    float gy = duxunClamp(((float)y + 0.5f) * (float)glowHeight / fmaxf((float)height, 1.0f), 0.0f, (float)(glowHeight - 1));
    int ix = (int)gx;
    int iy = (int)gy;
    int gi = (iy * glowWidth + ix) * 3;
    float bloomR = enableBloom ? bloomGlow[gi + 0] : 0.0f;
    float bloomG = enableBloom ? bloomGlow[gi + 1] : 0.0f;
    float bloomB = enableBloom ? bloomGlow[gi + 2] : 0.0f;
    float halR = enableHalation ? halationGlow[gi + 0] : 0.0f;
    float halG = enableHalation ? halationGlow[gi + 1] : 0.0f;
    float halB = enableHalation ? halationGlow[gi + 2] : 0.0f;
    int di = y * dstStrideFloats + x * 4;
    float r = dst[di + 0], g = dst[di + 1], b = dst[di + 2];
    if (showBloomMask) {
        float m = duxunClamp(0.2126f * bloomR + 0.7152f * bloomG + 0.0722f * bloomB, 0.0f, 1.0f);
        dst[di + 0] = m; dst[di + 1] = m; dst[di + 2] = m;
        return;
    }
    if (enableBloom && fBloom > 0.001f) {
        float amount = duxunClamp(fBloom, 0.0f, 1.25f);
        float gain = 1.08f + 1.12f * duxunClamp(fBloomRadius, 0.0f, 1.0f);
        float mix = duxunClamp(amount * (0.18f + 0.12f * (1.0f - duxunClamp(fBloomDetail, 0.0f, 1.0f))), 0.0f, 0.45f);
        float protect = duxunClamp(fBloomSaveLights, 0.0f, 1.0f);
        bloomR = duxunClamp(bloomR * gain, 0.0f, 1.8f);
        bloomG = duxunClamp(bloomG * gain, 0.0f, 1.8f);
        bloomB = duxunClamp(bloomB * gain, 0.0f, 1.8f);
        r = duxunCudaScreen(r, bloomR, mix * (1.0f - protect * duxunClamp(r, 0.0f, 1.0f)));
        g = duxunCudaScreen(g, bloomG, mix * (1.0f - protect * duxunClamp(g, 0.0f, 1.0f)));
        b = duxunCudaScreen(b, bloomB, mix * (1.0f - protect * duxunClamp(b, 0.0f, 1.0f)));
        float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        b = duxunLerp(b, fminf(b, lum * 0.96f), duxunClamp(fBloomDefringe, 0.0f, 1.0f) * mix);
    }
    if (enableHalation && fH > 0.01f) {
        float warm = duxunClamp(fHalationWarmth, 0.0f, 1.0f);
        float lum = duxunClamp(0.2126f * halR + 0.7152f * halG + 0.0722f * halB, 0.0f, 1.4f);
        float redKey = fmaxf(halR * 1.28f, lum * 0.92f);
        float mix = duxunClamp(fH * 0.52f, 0.0f, 0.85f);
        float veil = duxunClamp(redKey * mix, 0.0f, 0.95f);
        float baseLum = duxunClamp(0.2126f * r + 0.7152f * g + 0.0722f * b, 0.0f, 1.4f);
        float ringBias = 1.15f - 0.75f * duxunCudaSmoothstep(0.72f, 1.10f, baseLum);
        ringBias = duxunClamp(ringBias, 0.32f, 1.15f);

        r += veil * ringBias * (0.96f + 0.48f * warm);
        g += veil * ringBias * (0.035f + 0.135f * warm);
        b += veil * ringBias * (0.003f + 0.020f * (1.0f - warm));

        float h = duxunClamp(fHalationHue, 0.0f, 1.0f);
        float impact = duxunClamp(fHalationImpact, 0.0f, 1.0f);
        float blueComp = duxunClamp(fHalationBlueComp, 0.0f, 1.0f);
        float loc = duxunClamp(fHalationLocal, 0.0f, 1.0f);
        float glob = duxunClamp(fHalationGlobal, 0.0f, 1.0f);
        float controlMix = fH * (0.030f + 0.120f * impact) * lum;
        r += controlMix * (0.74f + 0.34f * loc + 0.08f * glob);
        g += controlMix * (0.065f + 0.115f * h);
        b += controlMix * (0.010f + 0.070f * (1.0f - h)) * (1.0f - 0.78f * blueComp);

        float haloTint = duxunClamp((veil * ringBias * (0.48f + 0.38f * warm)) + controlMix * (2.20f + 1.80f * impact), 0.0f, 0.72f);
        float orangeG = r * (0.24f + 0.10f * (1.0f - warm));
        float redB = r * (0.018f + 0.045f * (1.0f - warm));
        g = duxunLerp(g, fminf(g, orangeG), haloTint);
        b = duxunLerp(b, fminf(b, redB), haloTint);
        float fringe = duxunClamp(fHalationDefringe, 0.0f, 1.0f) * lum * fH;
        float outLum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        b = duxunLerp(b, fminf(b, outLum * 0.92f + r * 0.08f), fringe);
    }
    dst[di + 0] = duxunClamp(r, 0.0f, 1.0f);
    dst[di + 1] = duxunClamp(g, 0.0f, 1.0f);
    dst[di + 2] = duxunClamp(b, 0.0f, 1.0f);
}
)CUDA"
R"CUDA(

extern "C" __global__ void duxunFilmSimCudaKernel(
    const float* src, float* dst,
    int width, int height, int srcStrideFloats, int dstStrideFloats,
    int inputTransform, int outputTransform,
    float densityFactor, float grainScale, float halThreshold, float halScale,
    float fH, float fHalationWarmth, float fGr, float fFilmGrainSize, float fFilmGrainColor,
    float fV, float fVignetteRadius, float fVignetteFeather, float fSat,
    float fExpand, float fExpandBlack, float fExpandWhite,
    float fBreath, float fFilmBreathExposure, float fFilmBreathSaturation, float renderTime,
    float fPrintCustom, float fPrintContrast, float fProfile,
    float fColorHead, float fColorHeadCyan, float fColorHeadMagenta, float fColorHeadYellow,
    float fOverscanScale, float fWeaveX, float fWeaveY, float weaveRotation,
    float fDamage, float fFilmDust, float fFilmScratch,
    float m0, float m1, float m2, float m3, float m4, float m5, float m6, float m7, float m8,
    float curveContrast, float curveShoulder, float curveToe)
{
    int x = blockIdx.x * blockDim.x + threadIdx.x;
    int y = blockIdx.y * blockDim.y + threadIdx.y;
    if (x >= width || y >= height) return;

    float sx = (float)x;
    float sy = (float)y;
    if (fabsf(fOverscanScale - 1.0f) > 0.0001f || fabsf(fWeaveX) > 0.001f || fabsf(fWeaveY) > 0.001f || fabsf(weaveRotation) > 0.00001f) {
        float cx = (float)width * 0.5f;
        float cy = (float)height * 0.5f;
        float px = ((float)x - cx) / fmaxf(0.0001f, fOverscanScale);
        float py = ((float)y - cy) / fmaxf(0.0001f, fOverscanScale);
        float cr = cosf(weaveRotation);
        float sr = sinf(weaveRotation);
        sx = px * cr - py * sr + cx + fWeaveX;
        sy = px * sr + py * cr + cy + fWeaveY;
    }

    int si = y * srcStrideFloats + x * 4;
    int di = y * dstStrideFloats + x * 4;
    float r, g, b, a;
    duxunSampleBilinear(src, srcStrideFloats, width, height, sx, sy, &r, &g, &b, &a);

    duxunCudaApplyInputTransform(inputTransform, &r, &g, &b);

    if (fExpand > 0.001f) {
        float black = fExpandBlack * fExpand * 0.12f;
        float white = fExpandWhite * fExpand * 0.10f;
        r = duxunClamp((r - black) / fmaxf(1.0f - black + white, 0.001f), 0.0f, 1.0f);
        g = duxunClamp((g - black) / fmaxf(1.0f - black + white, 0.001f), 0.0f, 1.0f);
        b = duxunClamp((b - black) / fmaxf(1.0f - black + white, 0.001f), 0.0f, 1.0f);
    }

    if (fBreath > 0.001f) {
        float frame = floorf(renderTime * 12.0f);
        float n = duxunNativeNoise(frame, 4.0f, 29.0f) * 2.0f - 1.0f;
        float exposure = 1.0f + n * fBreath * fFilmBreathExposure * 0.045f;
        float satBreath = 1.0f + n * fBreath * fFilmBreathSaturation * 0.035f;
        r *= exposure; g *= exposure; b *= exposure;
        float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        r = lum + (r - lum) * satBreath;
        g = lum + (g - lum) * satBreath;
        b = lum + (b - lum) * satBreath;
    }

    r = duxunSCurve(r, curveContrast, curveShoulder, curveToe);
    g = duxunSCurve(g, curveContrast, curveShoulder, curveToe);
    b = duxunSCurve(b, curveContrast, curveShoulder, curveToe);

    if (fPrintCustom > 0.001f) {
        float printC = 1.0f + fPrintCustom * (0.18f + 0.35f * fPrintContrast);
        r = duxunClamp((r - 0.18f) * printC + 0.18f + fPrintCustom * 0.018f, 0.0f, 1.0f);
        g = duxunClamp((g - 0.18f) * printC + 0.18f + fPrintCustom * 0.010f, 0.0f, 1.0f);
        b = duxunClamp((b - 0.18f) * printC + 0.18f - fPrintCustom * 0.012f, 0.0f, 1.0f);
    }

    if (densityFactor != 0.0f) {
        float lum = fminf(1.0f, 0.2126f * r + 0.7152f * g + 0.0722f * b);
        float sat = 1.0f - densityFactor * (1.0f - lum);
        r = lum + (r - lum) * sat;
        g = lum + (g - lum) * sat;
        b = lum + (b - lum) * sat;
    }

    if (fProfile > 0.001f) {
        float mr = m0 * r + m1 * g + m2 * b;
        float mg = m3 * r + m4 * g + m5 * b;
        float mb = m6 * r + m7 * g + m8 * b;
        float s = duxunClamp(fProfile, 0.0f, 2.0f);
        r = r + (mr - r) * s;
        g = g + (mg - g) * s;
        b = b + (mb - b) * s;
    }

    if (fColorHead > 0.001f) {
        r += fColorHead * (-0.10f * fColorHeadCyan + 0.02f * fColorHeadMagenta + 0.08f * fColorHeadYellow);
        g += fColorHead * (0.07f * fColorHeadCyan - 0.09f * fColorHeadMagenta + 0.02f * fColorHeadYellow);
        b += fColorHead * (0.02f * fColorHeadCyan + 0.08f * fColorHeadMagenta - 0.10f * fColorHeadYellow);
    }

    if (fSat != 1.0f) {
        float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        r = lum + (r - lum) * fSat;
        g = lum + (g - lum) * fSat;
        b = lum + (b - lum) * fSat;
    }

    if (fH > 0.01f) {
        float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
        if (lum > halThreshold) {
            float ha = (lum - halThreshold) * halScale;
            r += ha * (0.95f + 0.35f * fHalationWarmth);
            g += ha * (0.18f + 0.26f * fHalationWarmth);
            b += ha * (0.03f + 0.12f * (1.0f - fHalationWarmth));
        }
    }

    if (fGr > 0.001f) {
        float lum = fminf(1.0f, 0.2126f * r + 0.7152f * g + 0.0722f * b);
        float lf = 0.05f + 0.9f * lum;
        float scale = fmaxf(0.40f, 3.0f - fFilmGrainSize * 2.2f);
        float mono = duxunNativeNoise((float)x / scale, (float)y / scale + renderTime * 19.0f, 42.0f) - 0.5f;
        float nr = duxunHashNoise(x, y, 137) - 0.5f;
        float ng = duxunHashNoise(x, y, 271) - 0.5f;
        float nb = duxunHashNoise(x, y, 409) - 0.5f;
        float colorMix = duxunClamp(fFilmGrainColor, 0.0f, 1.0f);
        float amount = grainScale * lf + fGr * 0.055f;
        r += duxunLerp(mono, nr, colorMix) * amount;
        g += duxunLerp(mono, ng, colorMix) * amount;
        b += duxunLerp(mono, nb, colorMix) * amount;
    }

    if (fV > 0.001f) {
        float nx = ((float)x + 0.5f) / fmaxf((float)width, 1.0f);
        float ny = ((float)y + 0.5f) / fmaxf((float)height, 1.0f);
        float dx = nx - 0.5f;
        float dy = ny - 0.5f;
        float dist = sqrtf(dx * dx + dy * dy) * 1.41421356f;
        float edge = duxunClamp((dist - fVignetteRadius) / fmaxf(0.001f, fVignetteFeather), 0.0f, 1.0f);
        float mask = 1.0f - edge * edge * (3.0f - 2.0f * edge) * fV;
        r *= mask; g *= mask; b *= mask;
    }

    r = duxunClamp(r, 0.0f, 1.0f);
    g = duxunClamp(g, 0.0f, 1.0f);
    b = duxunClamp(b, 0.0f, 1.0f);

    if (fDamage > 0.001f) {
        float frame = floorf(renderTime * 10.0f);
        float dust = duxunNativeNoise((float)x, (float)y + frame * 13.0f, 31.0f);
        if (dust > 1.0f - 0.012f * fDamage * fFilmDust) {
            float spot = duxunNativeNoise((float)y, frame, 34.0f) > 0.58f ? 1.35f : 0.62f;
            r *= spot; g *= spot; b *= spot;
        }
        float scratchColumn = duxunNativeNoise(floorf((float)x * 0.04f), frame, 57.0f);
        float scratchNoise = duxunNativeNoise((float)y * 0.35f, frame, 61.0f);
        if (scratchColumn > 1.0f - 0.080f * fDamage * fFilmScratch && scratchNoise > 0.18f) {
            float line = scratchNoise > 0.72f ? (1.0f + 0.80f * fDamage * fFilmScratch)
                                              : (1.0f - 0.35f * fDamage * fFilmScratch);
            r *= line; g *= line; b *= line;
        }
    }

    duxunCudaApplyOutputTransform(outputTransform, &r, &g, &b);

    dst[di + 0] = duxunClamp(r, 0.0f, 1.0f);
    dst[di + 1] = duxunClamp(g, 0.0f, 1.0f);
    dst[di + 2] = duxunClamp(b, 0.0f, 1.0f);
    dst[di + 3] = a;
}
)CUDA";

#if DUXUN_ENABLE_CUDA_RENDER
#ifdef _WIN32
#define DUXUN_CUDA_API __stdcall
#else
#define DUXUN_CUDA_API
#endif

typedef int DuxunCuResult;
typedef int DuxunNvrtcResult;
typedef void* DuxunCuModule;
typedef void* DuxunCuFunction;
typedef void* DuxunCuContext;
typedef void* DuxunNvrtcProgram;
typedef unsigned long long DuxunCuDevicePtr;

typedef DuxunCuResult (DUXUN_CUDA_API *PFN_cuInit)(unsigned int);
typedef DuxunCuResult (DUXUN_CUDA_API *PFN_cuModuleLoadData)(DuxunCuModule*, const void*);
typedef DuxunCuResult (DUXUN_CUDA_API *PFN_cuModuleGetFunction)(DuxunCuFunction*, DuxunCuModule, const char*);
typedef DuxunCuResult (DUXUN_CUDA_API *PFN_cuLaunchKernel)(DuxunCuFunction, unsigned int, unsigned int, unsigned int,
                                                           unsigned int, unsigned int, unsigned int, unsigned int,
                                                           void*, void**, void**);
typedef DuxunCuResult (DUXUN_CUDA_API *PFN_cuMemAlloc)(DuxunCuDevicePtr*, size_t);
typedef DuxunCuResult (DUXUN_CUDA_API *PFN_cuMemFree)(DuxunCuDevicePtr);
typedef DuxunCuResult (DUXUN_CUDA_API *PFN_cuMemAllocAsync)(DuxunCuDevicePtr*, size_t, void*);
typedef DuxunCuResult (DUXUN_CUDA_API *PFN_cuMemFreeAsync)(DuxunCuDevicePtr, void*);
typedef DuxunCuResult (DUXUN_CUDA_API *PFN_cuGetErrorName)(DuxunCuResult, const char**);
typedef DuxunCuResult (DUXUN_CUDA_API *PFN_cuStreamGetCtx)(void*, DuxunCuContext*);
typedef DuxunCuResult (DUXUN_CUDA_API *PFN_cuCtxGetCurrent)(DuxunCuContext*);
typedef DuxunCuResult (DUXUN_CUDA_API *PFN_cuCtxSetCurrent)(DuxunCuContext);
typedef DuxunCuResult (DUXUN_CUDA_API *PFN_cuPointerGetAttribute)(void*, int, DuxunCuDevicePtr);
typedef DuxunNvrtcResult (DUXUN_CUDA_API *PFN_nvrtcCreateProgram)(DuxunNvrtcProgram*, const char*, const char*, int, const char* const*, const char* const*);
typedef DuxunNvrtcResult (DUXUN_CUDA_API *PFN_nvrtcCompileProgram)(DuxunNvrtcProgram, int, const char* const*);
typedef DuxunNvrtcResult (DUXUN_CUDA_API *PFN_nvrtcGetPTXSize)(DuxunNvrtcProgram, size_t*);
typedef DuxunNvrtcResult (DUXUN_CUDA_API *PFN_nvrtcGetPTX)(DuxunNvrtcProgram, char*);
typedef DuxunNvrtcResult (DUXUN_CUDA_API *PFN_nvrtcGetProgramLogSize)(DuxunNvrtcProgram, size_t*);
typedef DuxunNvrtcResult (DUXUN_CUDA_API *PFN_nvrtcGetProgramLog)(DuxunNvrtcProgram, char*);
typedef DuxunNvrtcResult (DUXUN_CUDA_API *PFN_nvrtcDestroyProgram)(DuxunNvrtcProgram*);

struct CudaRuntimeState {
    bool attempted;
    bool ready;
    HMODULE cudaLib;
    HMODULE nvrtcLib;
    DuxunCuModule module;
    DuxunCuFunction kernel;
    DuxunCuFunction highlightMaskKernel;
    DuxunCuFunction downsampleKernel;
    DuxunCuFunction blurHorizontalKernel;
    DuxunCuFunction blurVerticalKernel;
    DuxunCuFunction compositeGlowKernel;
    std::vector<char> ptx;
    std::string log;
    PFN_cuInit cuInit;
    PFN_cuModuleLoadData cuModuleLoadData;
    PFN_cuModuleGetFunction cuModuleGetFunction;
    PFN_cuLaunchKernel cuLaunchKernel;
    PFN_cuMemAlloc cuMemAlloc;
    PFN_cuMemFree cuMemFree;
    PFN_cuMemAllocAsync cuMemAllocAsync;
    PFN_cuMemFreeAsync cuMemFreeAsync;
    PFN_cuGetErrorName cuGetErrorName;
    PFN_cuStreamGetCtx cuStreamGetCtx;
    PFN_cuCtxGetCurrent cuCtxGetCurrent;
    PFN_cuCtxSetCurrent cuCtxSetCurrent;
    PFN_cuPointerGetAttribute cuPointerGetAttribute;
    PFN_nvrtcCreateProgram nvrtcCreateProgram;
    PFN_nvrtcCompileProgram nvrtcCompileProgram;
    PFN_nvrtcGetPTXSize nvrtcGetPTXSize;
    PFN_nvrtcGetPTX nvrtcGetPTX;
    PFN_nvrtcGetProgramLogSize nvrtcGetProgramLogSize;
    PFN_nvrtcGetProgramLog nvrtcGetProgramLog;
    PFN_nvrtcDestroyProgram nvrtcDestroyProgram;
};

static CudaRuntimeState gCudaRuntime = {};
static std::mutex gCudaRuntimeMutex;

static HMODULE loadFirstCudaLibrary(const char* const* names, int count) {
    for (int i = 0; i < count; i++) {
        HMODULE lib = nullptr;
        if (strchr(names[i], '\\')) {
            lib = LoadLibraryExA(names[i], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        } else {
            lib = LoadLibraryA(names[i]);
        }
        if (lib) return lib;
    }
    return nullptr;
}

template <typename T>
static bool loadCudaProc(HMODULE lib, const char* name, T& out) {
    out = reinterpret_cast<T>(GetProcAddress(lib, name));
    return out != nullptr;
}

static bool loadCudaRuntime(CudaRuntimeState& rt) {
    std::lock_guard<std::mutex> lock(gCudaRuntimeMutex);
    if (rt.attempted) return rt.ready;
    rt.attempted = true;

    const char* cudaNames[] = { "nvcuda.dll" };
    rt.cudaLib = loadFirstCudaLibrary(cudaNames, 1);
    SetDllDirectoryA("C:\\Program Files\\Blackmagic Design\\DaVinci Resolve");
    const char* nvrtcNames[] = {
        "nvrtc64_120_0.dll",
        "C:\\Program Files\\Blackmagic Design\\DaVinci Resolve\\nvrtc64_120_0.dll",
        "nvrtc64_80.dll",
        "C:\\Program Files\\Blackmagic Design\\DaVinci Resolve\\nvrtc64_80.dll"
    };
    rt.nvrtcLib = loadFirstCudaLibrary(nvrtcNames, 4);
    if (!rt.cudaLib || !rt.nvrtcLib) {
        rt.log = "CUDA driver or NVRTC DLL not available";
        return false;
    }

    if (!loadCudaProc(rt.cudaLib, "cuInit", rt.cuInit) ||
        !loadCudaProc(rt.cudaLib, "cuModuleLoadData", rt.cuModuleLoadData) ||
        !loadCudaProc(rt.cudaLib, "cuModuleGetFunction", rt.cuModuleGetFunction) ||
        !loadCudaProc(rt.cudaLib, "cuLaunchKernel", rt.cuLaunchKernel) ||
        !loadCudaProc(rt.cudaLib, "cuMemAlloc", rt.cuMemAlloc) ||
        !loadCudaProc(rt.cudaLib, "cuMemFree", rt.cuMemFree) ||
        !loadCudaProc(rt.cudaLib, "cuGetErrorName", rt.cuGetErrorName) ||
        !loadCudaProc(rt.cudaLib, "cuStreamGetCtx", rt.cuStreamGetCtx) ||
        !loadCudaProc(rt.cudaLib, "cuCtxGetCurrent", rt.cuCtxGetCurrent) ||
        !loadCudaProc(rt.cudaLib, "cuCtxSetCurrent", rt.cuCtxSetCurrent) ||
        !loadCudaProc(rt.cudaLib, "cuPointerGetAttribute", rt.cuPointerGetAttribute) ||
        !loadCudaProc(rt.nvrtcLib, "nvrtcCreateProgram", rt.nvrtcCreateProgram) ||
        !loadCudaProc(rt.nvrtcLib, "nvrtcCompileProgram", rt.nvrtcCompileProgram) ||
        !loadCudaProc(rt.nvrtcLib, "nvrtcGetPTXSize", rt.nvrtcGetPTXSize) ||
        !loadCudaProc(rt.nvrtcLib, "nvrtcGetPTX", rt.nvrtcGetPTX) ||
        !loadCudaProc(rt.nvrtcLib, "nvrtcGetProgramLogSize", rt.nvrtcGetProgramLogSize) ||
        !loadCudaProc(rt.nvrtcLib, "nvrtcGetProgramLog", rt.nvrtcGetProgramLog) ||
        !loadCudaProc(rt.nvrtcLib, "nvrtcDestroyProgram", rt.nvrtcDestroyProgram)) {
        rt.log = "CUDA driver or NVRTC entry point missing";
        return false;
    }
    loadCudaProc(rt.cudaLib, "cuMemAllocAsync", rt.cuMemAllocAsync);
    loadCudaProc(rt.cudaLib, "cuMemFreeAsync", rt.cuMemFreeAsync);

    if (rt.cuInit(0) != 0) {
        rt.log = "cuInit failed";
        return false;
    }

    DuxunNvrtcProgram program = nullptr;
    if (rt.nvrtcCreateProgram(&program, kCudaKernelSource, "DuXunFilmSimCudaKernel.cu", 0, nullptr, nullptr) != 0) {
        rt.log = "nvrtcCreateProgram failed";
        return false;
    }

    const char* options[] = { "--std=c++11", "--gpu-architecture=compute_52" };
    DuxunNvrtcResult compileResult = rt.nvrtcCompileProgram(program, 2, options);
    size_t logSize = 0;
    rt.nvrtcGetProgramLogSize(program, &logSize);
    if (logSize > 1) {
        std::vector<char> log(logSize);
        rt.nvrtcGetProgramLog(program, log.data());
        rt.log.assign(log.data());
    }
    if (compileResult != 0) {
        rt.nvrtcDestroyProgram(&program);
        return false;
    }

    size_t ptxSize = 0;
    if (rt.nvrtcGetPTXSize(program, &ptxSize) != 0 || ptxSize == 0) {
        rt.nvrtcDestroyProgram(&program);
        rt.log = "nvrtcGetPTXSize failed";
        return false;
    }
    rt.ptx.resize(ptxSize);
    if (rt.nvrtcGetPTX(program, rt.ptx.data()) != 0) {
        rt.nvrtcDestroyProgram(&program);
        rt.log = "nvrtcGetPTX failed";
        return false;
    }
    rt.nvrtcDestroyProgram(&program);

    rt.ready = true;
    return true;
}

static bool ensureCudaModuleLoaded(CudaRuntimeState& rt) {
    std::lock_guard<std::mutex> lock(gCudaRuntimeMutex);
    if (rt.module && rt.kernel && rt.highlightMaskKernel && rt.downsampleKernel &&
        rt.blurHorizontalKernel && rt.blurVerticalKernel && rt.compositeGlowKernel) {
        return true;
    }
    if (rt.cuModuleLoadData(&rt.module, rt.ptx.data()) != 0) {
        rt.log = "cuModuleLoadData failed";
        return false;
    }
    if (rt.cuModuleGetFunction(&rt.kernel, rt.module, "duxunFilmSimCudaKernel") != 0) {
        rt.log = "cuModuleGetFunction failed";
        return false;
    }
    if (rt.cuModuleGetFunction(&rt.highlightMaskKernel, rt.module, "cudaHighlightMaskKernel") != 0 ||
        rt.cuModuleGetFunction(&rt.downsampleKernel, rt.module, "cudaDownsampleKernel") != 0 ||
        rt.cuModuleGetFunction(&rt.blurHorizontalKernel, rt.module, "cudaBlurHorizontalKernel") != 0 ||
        rt.cuModuleGetFunction(&rt.blurVerticalKernel, rt.module, "cudaBlurVerticalKernel") != 0 ||
        rt.cuModuleGetFunction(&rt.compositeGlowKernel, rt.module, "cudaCompositeGlowKernel") != 0) {
        rt.log = "cuModuleGetFunction glow failed";
        return false;
    }
    return true;
}

static const char* cudaFailureReason(const char* label, DuxunCuResult result) {
    static thread_local char reason[128];
    const char* errorName = nullptr;
    if (gCudaRuntime.cuGetErrorName) gCudaRuntime.cuGetErrorName(result, &errorName);
    if (!errorName) {
        sprintf_s(reason, sizeof(reason), "%s:%d", label, (int)result);
    } else {
        sprintf_s(reason, sizeof(reason), "%s:%s", label, errorName);
    }
    return reason;
}

static DuxunCuResult cudaAllocDeviceBuffer(DuxunCuDevicePtr* ptr, size_t bytes, void* stream) {
    if (stream && gCudaRuntime.cuMemAllocAsync) {
        return gCudaRuntime.cuMemAllocAsync(ptr, bytes, stream);
    }
    return gCudaRuntime.cuMemAlloc(ptr, bytes);
}

static void cudaFreeDeviceBuffer(DuxunCuDevicePtr& ptr, void* stream) {
    if (!ptr) return;
    if (stream && gCudaRuntime.cuMemFreeAsync) {
        gCudaRuntime.cuMemFreeAsync(ptr, stream);
    } else {
        gCudaRuntime.cuMemFree(ptr);
    }
    ptr = 0;
}

struct ScopedCudaContextBinding {
    DuxunCuContext previous;
    bool restore;
    const char* source;

    ScopedCudaContextBinding() : previous(nullptr), restore(false), source("none") {}
    ~ScopedCudaContextBinding() {
        if (restore && gCudaRuntime.cuCtxSetCurrent) {
            gCudaRuntime.cuCtxSetCurrent(previous);
        }
    }
};

static bool bindCudaContextForRequest(void* stream, const void* src, const void* dst,
                                      ScopedCudaContextBinding& binding,
                                      const char** failureReason) {
    DuxunCuContext streamCtx = nullptr;
    const char* source = "stream";
    DuxunCuResult result = 0;
    if (stream) {
        result = gCudaRuntime.cuStreamGetCtx(stream, &streamCtx);
        if (result != 0) {
            if (failureReason) *failureReason = cudaFailureReason("cuda_stream_get_ctx_failed", result);
            return false;
        }
    }
    if (!streamCtx) {
        source = "pointer";
        DuxunCuDevicePtr devicePtr = (DuxunCuDevicePtr)(uintptr_t)(dst ? dst : src);
        if (!devicePtr) {
            if (failureReason) *failureReason = "cuda_context_unavailable";
            return false;
        }
        result = gCudaRuntime.cuPointerGetAttribute(&streamCtx, 1, devicePtr);
        if (result != 0 || !streamCtx) {
            if (failureReason) *failureReason = cudaFailureReason("cuda_pointer_get_ctx_failed", result);
            return false;
        }
    }
    result = gCudaRuntime.cuCtxGetCurrent(&binding.previous);
    if (result != 0) {
        if (failureReason) *failureReason = cudaFailureReason("cuda_ctx_get_current_failed", result);
        return false;
    }
    if (binding.previous == streamCtx) {
        binding.source = strcmp(source, "stream") == 0 ? "stream_same" : "pointer_same";
        return true;
    }
    result = gCudaRuntime.cuCtxSetCurrent(streamCtx);
    if (result != 0) {
        if (failureReason) *failureReason = cudaFailureReason("cuda_ctx_set_current_failed", result);
        return false;
    }
    binding.restore = true;
    binding.source = strcmp(source, "stream") == 0 ? "stream_set" : "pointer_set";
    return true;
}

static bool launchCuda2D(DuxunCuFunction kernel, int width, int height, void** params, void* stream) {
    unsigned int blockX = 16;
    unsigned int blockY = 16;
    unsigned int gridX = (unsigned int)((width + (int)blockX - 1) / (int)blockX);
    unsigned int gridY = (unsigned int)((height + (int)blockY - 1) / (int)blockY);
    return gCudaRuntime.cuLaunchKernel(kernel, gridX, gridY, 1,
                                       blockX, blockY, 1, 0,
                                       stream, params, nullptr) == 0;
}

static bool launchCudaGlowPass(const GpuRenderRequest& gpuRequest,
                               const RenderThreadArgs& args,
                               int glowKind,
                               float threshold,
                               float radius,
                               int glowWidth,
                               int glowHeight,
                               DuxunCuDevicePtr glowMaskDeviceBuffer,
                               DuxunCuDevicePtr glowTempDeviceBuffer,
                               DuxunCuDevicePtr glowDeviceBuffer,
                               const char** failureReason) {
    const float* srcParam = args.src;
    int srcWidth = args.width;
    int srcHeight = args.height;
    int srcStrideFloats = args.sS;
    float bloomRegion = glowKind == GLOW_BLOOM ? args.fBloomRegion : 0.0f;
    float bloomSoftness = glowKind == GLOW_BLOOM ? args.fBloomSoftness : 0.0f;
    int inputTransform = args.inputTransform;
    void* maskParams[] = {
        (void*)&srcParam, (void*)&glowMaskDeviceBuffer,
        (void*)&srcWidth, (void*)&srcHeight, (void*)&srcStrideFloats,
        (void*)&glowWidth, (void*)&glowHeight, (void*)&glowKind,
        (void*)&threshold, (void*)&bloomRegion, (void*)&bloomSoftness,
        (void*)&inputTransform
    };
    if (!launchCuda2D(gCudaRuntime.highlightMaskKernel, glowWidth, glowHeight, maskParams, gpuRequest.cudaStream)) {
        if (failureReason) *failureReason = glowKind == GLOW_BLOOM ? "bloom_mask_launch_failed" : "halation_mask_launch_failed";
        return false;
    }

    void* downsampleParams[] = {
        (void*)&glowMaskDeviceBuffer, (void*)&glowTempDeviceBuffer,
        (void*)&glowWidth, (void*)&glowHeight
    };
    if (!launchCuda2D(gCudaRuntime.downsampleKernel, glowWidth, glowHeight, downsampleParams, gpuRequest.cudaStream)) {
        if (failureReason) *failureReason = glowKind == GLOW_BLOOM ? "bloom_downsample_launch_failed" : "halation_downsample_launch_failed";
        return false;
    }

    int blurRadius = (int)radius;
    if (blurRadius < 1) blurRadius = 1;
    if (blurRadius > 12) blurRadius = 12;
    void* blurHParams[] = {
        (void*)&glowTempDeviceBuffer, (void*)&glowDeviceBuffer,
        (void*)&glowWidth, (void*)&glowHeight, (void*)&blurRadius
    };
    if (!launchCuda2D(gCudaRuntime.blurHorizontalKernel, glowWidth, glowHeight, blurHParams, gpuRequest.cudaStream)) {
        if (failureReason) *failureReason = glowKind == GLOW_BLOOM ? "bloom_blur_h_launch_failed" : "halation_blur_h_launch_failed";
        return false;
    }
    void* blurVParams[] = {
        (void*)&glowDeviceBuffer, (void*)&glowTempDeviceBuffer,
        (void*)&glowWidth, (void*)&glowHeight, (void*)&blurRadius
    };
    if (!launchCuda2D(gCudaRuntime.blurVerticalKernel, glowWidth, glowHeight, blurVParams, gpuRequest.cudaStream)) {
        if (failureReason) *failureReason = glowKind == GLOW_BLOOM ? "bloom_blur_v_launch_failed" : "halation_blur_v_launch_failed";
        return false;
    }
    void* copyBackParams[] = {
        (void*)&glowTempDeviceBuffer, (void*)&glowDeviceBuffer,
        (void*)&glowWidth, (void*)&glowHeight
    };
    if (!launchCuda2D(gCudaRuntime.downsampleKernel, glowWidth, glowHeight, copyBackParams, gpuRequest.cudaStream)) {
        if (failureReason) *failureReason = glowKind == GLOW_BLOOM ? "bloom_copy_launch_failed" : "halation_copy_launch_failed";
        return false;
    }
    return true;
}

static bool tryCudaGlowRender(const GpuRenderRequest& gpuRequest,
                              const RenderThreadArgs& args,
                              const char** failureReason) {
    if (!args.doBloom && !args.doHalation) return true;

    const int glowScale = 4;
    int glowWidth = (args.width + glowScale - 1) / glowScale;
    int glowHeight = (args.height + glowScale - 1) / glowScale;
    if (glowWidth <= 0 || glowHeight <= 0) {
        if (failureReason) *failureReason = "glow_bad_size";
        return false;
    }

    size_t glowDeviceBufferBytes = (size_t)glowWidth * (size_t)glowHeight * 3u * sizeof(float);
    DuxunCuDevicePtr glowMaskDeviceBuffer = 0;
    DuxunCuDevicePtr glowTempDeviceBuffer = 0;
    DuxunCuDevicePtr halationGlowDeviceBuffer = 0;
    DuxunCuDevicePtr bloomGlowDeviceBuffer = 0;
    auto freeGlowBuffers = [&]() {
        cudaFreeDeviceBuffer(glowMaskDeviceBuffer, gpuRequest.cudaStream);
        cudaFreeDeviceBuffer(glowTempDeviceBuffer, gpuRequest.cudaStream);
        cudaFreeDeviceBuffer(halationGlowDeviceBuffer, gpuRequest.cudaStream);
        cudaFreeDeviceBuffer(bloomGlowDeviceBuffer, gpuRequest.cudaStream);
    };
    DuxunCuResult allocResult = cudaAllocDeviceBuffer(&glowMaskDeviceBuffer, glowDeviceBufferBytes, gpuRequest.cudaStream);
    if (allocResult != 0) {
        freeGlowBuffers();
        if (failureReason) *failureReason = cudaFailureReason("glow_mask_alloc_failed", allocResult);
        return false;
    }
    allocResult = cudaAllocDeviceBuffer(&glowTempDeviceBuffer, glowDeviceBufferBytes, gpuRequest.cudaStream);
    if (allocResult != 0) {
        freeGlowBuffers();
        if (failureReason) *failureReason = cudaFailureReason("glow_temp_alloc_failed", allocResult);
        return false;
    }
    allocResult = args.doHalation ? cudaAllocDeviceBuffer(&halationGlowDeviceBuffer, glowDeviceBufferBytes, gpuRequest.cudaStream) : 0;
    if (allocResult != 0) {
        freeGlowBuffers();
        if (failureReason) *failureReason = cudaFailureReason("halation_glow_alloc_failed", allocResult);
        return false;
    }
    allocResult = args.doBloom ? cudaAllocDeviceBuffer(&bloomGlowDeviceBuffer, glowDeviceBufferBytes, gpuRequest.cudaStream) : 0;
    if (allocResult != 0) {
        freeGlowBuffers();
        if (failureReason) *failureReason = cudaFailureReason("bloom_glow_alloc_failed", allocResult);
        return false;
    }

    int srcWidth = args.width;
    int srcHeight = args.height;
    if (args.doHalation) {
        if (!launchCudaGlowPass(gpuRequest, args, GLOW_HALATION, args.halThreshold, 2.0f + args.fH * 10.0f,
                                glowWidth, glowHeight, glowMaskDeviceBuffer, glowTempDeviceBuffer,
                                halationGlowDeviceBuffer, failureReason)) {
            freeGlowBuffers();
            return false;
        }
    }
    if (args.doBloom) {
        if (!launchCudaGlowPass(gpuRequest, args, GLOW_BLOOM, 0.72f, 2.0f + args.fBloomRadius * 10.0f,
                                glowWidth, glowHeight, glowMaskDeviceBuffer, glowTempDeviceBuffer,
                                bloomGlowDeviceBuffer, failureReason)) {
            freeGlowBuffers();
            return false;
        }
    }

    float* dstParam = args.dst;
    int dstStrideFloats = args.dS;
    int enableBloom = args.doBloom ? 1 : 0;
    int enableHalation = args.doHalation ? 1 : 0;
    int showBloomMask = args.showBloomMask ? 1 : 0;
    void* compositeParams[] = {
        (void*)&dstParam, (void*)&bloomGlowDeviceBuffer, (void*)&halationGlowDeviceBuffer,
        (void*)&srcWidth, (void*)&srcHeight, (void*)&dstStrideFloats,
        (void*)&glowWidth, (void*)&glowHeight,
        (void*)&enableBloom, (void*)&enableHalation, (void*)&args.fH, (void*)&args.fHalationWarmth,
        (void*)&args.fHalationLocal, (void*)&args.fHalationGlobal, (void*)&args.fHalationHue,
        (void*)&args.fHalationBlueComp, (void*)&args.fHalationImpact, (void*)&args.fHalationDefringe,
        (void*)&args.fBloom, (void*)&args.fBloomRadius, (void*)&args.fBloomDetail,
        (void*)&args.fBloomSaveLights, (void*)&args.fBloomDefringe,
        (void*)&showBloomMask
    };
    bool ok = launchCuda2D(gCudaRuntime.compositeGlowKernel, args.width, args.height, compositeParams, gpuRequest.cudaStream);
    freeGlowBuffers();
    if (!ok) {
        if (failureReason) *failureReason = "glow_composite_launch_failed";
        return false;
    }
    return true;
}
#endif

static bool isCudaGpuPathSupported(const RenderThreadArgs& args) {
    (void)args;
    return true;
}

static bool tryGpuRender(const GpuRenderRequest& gpuRequest,
                         const RenderThreadArgs& args,
                         float curveContrast,
                         float curveShoulder,
                         float curveToe,
                         const char** failureReason,
                         GpuRenderTiming* timing) {
    if (failureReason) *failureReason = "unknown";
    if (timing) {
        timing->setupMs = -1.0;
        timing->kernelMs = -1.0;
        timing->coldStart = false;
    }
#if DUXUN_ENABLE_CUDA_RENDER
    if (timing) {
        timing->coldStart = (gCudaSuccessfulRenderCount.load(std::memory_order_relaxed) == 0);
    }
    const auto setupStart = std::chrono::steady_clock::now();
    auto finishSetupTiming = [&]() {
        if (timing && timing->setupMs < 0.0) {
            timing->setupMs = gpuElapsedMilliseconds(setupStart);
        }
    };

    if (!gpuRequest.cuda) { if (failureReason) *failureReason = gpuRequest.openCL ? "opencl_not_implemented" : "not_cuda"; finishSetupTiming(); return false; }
    if (!gpuRequest.available) { if (failureReason) *failureReason = "requested_unavailable"; finishSetupTiming(); return false; }
    if (!isCudaGpuPathSupported(args)) { if (failureReason) *failureReason = "unsupported_effects"; finishSetupTiming(); return false; }
    if (!loadCudaRuntime(gCudaRuntime)) { if (failureReason) *failureReason = "runtime_not_loaded"; finishSetupTiming(); return false; }
    ScopedCudaContextBinding contextBinding;
    if (!bindCudaContextForRequest(gpuRequest.cudaStream, args.src, args.dst, contextBinding, failureReason)) { finishSetupTiming(); return false; }
    if (!ensureCudaModuleLoaded(gCudaRuntime)) { if (failureReason) *failureReason = "module_not_loaded"; finishSetupTiming(); return false; }
    finishSetupTiming();

    const float* srcParam = args.src;
    float* dstParam = args.dst;
    int width = args.width;
    int height = args.height;
    int srcStrideFloats = args.sS;
    int dstStrideFloats = args.dS;
    int inputTransform = args.inputTransform;
    int outputTransform = args.outputTransform;
    float m0 = (float)args.filmMatrix[0], m1 = (float)args.filmMatrix[1], m2 = (float)args.filmMatrix[2];
    float m3 = (float)args.filmMatrix[3], m4 = (float)args.filmMatrix[4], m5 = (float)args.filmMatrix[5];
    float m6 = (float)args.filmMatrix[6], m7 = (float)args.filmMatrix[7], m8 = (float)args.filmMatrix[8];

    void* params[] = {
        (void*)&srcParam, (void*)&dstParam,
        (void*)&width, (void*)&height, (void*)&srcStrideFloats, (void*)&dstStrideFloats,
        (void*)&inputTransform, (void*)&outputTransform,
        (void*)&args.densityFactor, (void*)&args.grainScale, (void*)&args.halThreshold, (void*)&args.halScale,
        (void*)&args.fH, (void*)&args.fHalationWarmth, (void*)&args.fGr, (void*)&args.fFilmGrainSize, (void*)&args.fFilmGrainColor,
        (void*)&args.fV, (void*)&args.fVignetteRadius, (void*)&args.fVignetteFeather, (void*)&args.fSat,
        (void*)&args.fExpand, (void*)&args.fExpandBlack, (void*)&args.fExpandWhite,
        (void*)&args.fBreath, (void*)&args.fFilmBreathExposure, (void*)&args.fFilmBreathSaturation, (void*)&args.renderTime,
        (void*)&args.fPrintCustom, (void*)&args.fPrintContrast, (void*)&args.fProfile,
        (void*)&args.fColorHead, (void*)&args.fColorHeadCyan, (void*)&args.fColorHeadMagenta, (void*)&args.fColorHeadYellow,
        (void*)&args.overscanScale, (void*)&args.weaveX, (void*)&args.weaveY, (void*)&args.weaveRotation,
        (void*)&args.fDamage, (void*)&args.fFilmDust, (void*)&args.fFilmScratch,
        (void*)&m0, (void*)&m1, (void*)&m2, (void*)&m3, (void*)&m4, (void*)&m5, (void*)&m6, (void*)&m7, (void*)&m8,
        (void*)&curveContrast, (void*)&curveShoulder, (void*)&curveToe
    };

    unsigned int blockX = 16;
    unsigned int blockY = 16;
    unsigned int gridX = (unsigned int)((width + (int)blockX - 1) / (int)blockX);
    unsigned int gridY = (unsigned int)((height + (int)blockY - 1) / (int)blockY);
    const auto kernelStart = std::chrono::steady_clock::now();
    DuxunCuResult launchResult = gCudaRuntime.cuLaunchKernel(gCudaRuntime.kernel, gridX, gridY, 1,
                                                             blockX, blockY, 1, 0,
                                                             gpuRequest.cudaStream, params, nullptr);
    if (launchResult != 0) {
        if (timing) timing->kernelMs = gpuElapsedMilliseconds(kernelStart);
        if (failureReason) *failureReason = "launch_failed";
        return false;
    }
    if (!tryCudaGlowRender(gpuRequest, args, failureReason)) {
        if (timing) timing->kernelMs = gpuElapsedMilliseconds(kernelStart);
        return false;
    }
    if (timing) timing->kernelMs = gpuElapsedMilliseconds(kernelStart);
    gCudaSuccessfulRenderCount.fetch_add(1, std::memory_order_relaxed);
    if (failureReason) *failureReason = "ok";
    return true;
#else
    (void)gpuRequest;
    (void)args;
    (void)curveContrast;
    (void)curveShoulder;
    (void)curveToe;
    (void)failureReason;
    (void)timing;
    if (failureReason) *failureReason = "cuda_not_compiled";
    return false;
#endif
}

static OfxStatus defineDoubleParam(OfxParamSetHandle ps, const char* name, const char* label,
                                    const char* hint, double def, double lo, double hi, double step,
                                    const char* parent) {
    OfxPropertySetHandle h;
    if (gParamSuite->paramDefine(ps, kOfxParamTypeDouble, name, &h) != kOfxStatOK)
        return kOfxStatErrBadHandle;
    gPropSuite->propSetString(h, kOfxPropLabel, 0, label);
    if (hint) gPropSuite->propSetString(h, kOfxParamPropHint, 0, hint);
    gPropSuite->propSetDouble(h, kOfxParamPropDefault, 0, def);
    gPropSuite->propSetDouble(h, kOfxParamPropMin, 0, lo);
    gPropSuite->propSetDouble(h, kOfxParamPropMax, 0, hi);
    gPropSuite->propSetDouble(h, kOfxParamPropDisplayMin, 0, lo);
    gPropSuite->propSetDouble(h, kOfxParamPropDisplayMax, 0, hi);
    gPropSuite->propSetDouble(h, kOfxParamPropIncrement, 0, step);
    gPropSuite->propSetString(h, kOfxParamPropDoubleType, 0, kOfxParamDoubleTypePlain);
    gPropSuite->propSetInt(h, kOfxParamPropDigits, 0, 2);
    gPropSuite->propSetInt(h, kOfxParamPropAnimates, 0, 0);
    markParamRealtime(h);
    if (parent) gPropSuite->propSetString(h, kOfxParamPropParent, 0, parent);
    return kOfxStatOK;
}

static OfxStatus defineChoiceParam(OfxParamSetHandle ps, const char* name, const char* label,
                                    const char* hint, int def, const char* parent,
                                    const char* const* options, int optionCount) {
    OfxPropertySetHandle h;
    if (gParamSuite->paramDefine(ps, kOfxParamTypeChoice, name, &h) != kOfxStatOK)
        return kOfxStatErrBadHandle;
    gPropSuite->propSetString(h, kOfxPropLabel, 0, label);
    if (hint) gPropSuite->propSetString(h, kOfxParamPropHint, 0, hint);
    gPropSuite->propSetInt(h, kOfxParamPropDefault, 0, def);
    gPropSuite->propSetInt(h, kOfxParamPropAnimates, 0, 0);
    markParamRealtime(h);
    if (parent) gPropSuite->propSetString(h, kOfxParamPropParent, 0, parent);
    for (int i = 0; i < optionCount; i++)
        gPropSuite->propSetString(h, kOfxParamPropChoiceOption, i, options[i]);
    return kOfxStatOK;
}

static OfxStatus defineStringLabelParam(OfxParamSetHandle ps, const char* name, const char* label,
                                        const char* value, const char* parent) {
    OfxPropertySetHandle h;
    if (gParamSuite->paramDefine(ps, kOfxParamTypeString, name, &h) != kOfxStatOK)
        return kOfxStatErrBadHandle;
    gPropSuite->propSetString(h, kOfxPropLabel, 0, label);
    gPropSuite->propSetString(h, kOfxParamPropDefault, 0, value);
    gPropSuite->propSetString(h, kOfxParamPropStringMode, 0, kOfxParamStringIsLabel);
    gPropSuite->propSetInt(h, kOfxParamPropAnimates, 0, 0);
    if (parent) gPropSuite->propSetString(h, kOfxParamPropParent, 0, parent);
    return kOfxStatOK;
}

static OfxStatus definePushButtonParam(OfxParamSetHandle ps, const char* name, const char* label,
                                       const char* hint, const char* parent) {
    OfxPropertySetHandle h;
    if (gParamSuite->paramDefine(ps, kOfxParamTypePushButton, name, &h) != kOfxStatOK)
        return kOfxStatErrBadHandle;
    gPropSuite->propSetString(h, kOfxPropLabel, 0, label);
    if (hint) gPropSuite->propSetString(h, kOfxParamPropHint, 0, hint);
    if (parent) gPropSuite->propSetString(h, kOfxParamPropParent, 0, parent);
    return kOfxStatOK;
}

static void defineEnabledParam(OfxParamSetHandle ps, const char* name, const char* parent) {
    const char* options[] = { "跳过", "启用" };
    defineChoiceParam(ps, name, "开关", "选择“跳过”后该模块完全不参与渲染。", ENABLE_SKIP, parent, options, 2);
}

static void setStringParamIfPresent(OfxParamSetHandle pSet, const char* name, const std::string& value) {
    OfxParamHandle ph = nullptr;
    if (gParamSuite->paramGetHandle(pSet, name, &ph, nullptr) == kOfxStatOK)
        gParamSuite->paramSetValue(ph, value.c_str());
}

static void updateLicenseStatusParam(OfxParamSetHandle pSet, const duxun_license::LocalLicenseState& state) {
    setStringParamIfPresent(pSet, "licenseStatus", state.statusText);
}

static void postLicenseMessage(OfxImageEffectHandle effect, const duxun_license::LocalLicenseState& state) {
    if (!gMessageSuite) return;
    if (!state.watermarkRequired) {
        gMessageSuite->clearPersistentMessage(effect);
        return;
    }
    const char* messageType = state.trialActive && !state.expired ? kOfxMessageMessage : kOfxMessageWarning;
    gMessageSuite->setPersistentMessage(effect, messageType, "DuXunLicense", "%s", state.statusText.c_str());
}

static OfxStatus defineGroupParam(OfxParamSetHandle ps, const char* name, const char* label,
                                   int open, const char* parent);

static void defineLicenseParams(OfxParamSetHandle ps) {
    defineGroupParam(ps, "grpLicense", "License", 0, nullptr);
    defineStringLabelParam(ps, "licenseStatus", "Status",
                           "License: checked on render or reload.", "grpLicense");
    definePushButtonParam(ps, "generateActivationRequest", "Generate Activation Request",
                          "Write %PROGRAMDATA%\\DuXun\\FilmSim\\activation-request.json for offline activation.",
                          "grpLicense");
    definePushButtonParam(ps, "reloadLicense", "Reload License",
                          "Reload %PROGRAMDATA%\\DuXun\\FilmSim\\license.json after installing a signed license.",
                          "grpLicense");
}

static void defineGrainProfileParam(OfxParamSetHandle ps, const char* parent) {
    const char* options[] = {
        "跟随胶片",
        "65mm 低感细腻",
        "35mm 标准",
        "35mm 高感粗颗粒",
        "16mm",
        "Super8 / 8mm",
        "自定义"
    };
    defineChoiceParam(ps, "filmGrainProfile", "颗粒规格", "按胶片规格选择颗粒大小、彩色颗粒和高光权重。", PROFILE_AUTO, parent, options, 7);
}

static void defineHalationProfileParam(OfxParamSetHandle ps, const char* parent) {
    const char* options[] = {
        "跟随胶片",
        "65mm 标准",
        "35mm 标准",
        "16mm 标准",
        "Super8 / 8mm 标准",
        "65mm No Remjet",
        "35mm CineStill / No Remjet",
        "16mm No Remjet",
        "Super8 / 8mm No Remjet",
        "自定义"
    };
    defineChoiceParam(ps, "halationProfile", "光晕规格", "选择 remjet 保留或去背胶片带来的红橙高光扩散。", PROFILE_AUTO, parent, options, 10);
}

static void defineBloomProfileParam(OfxParamSetHandle ps, const char* parent) {
    const char* options[] = {
        "跟随胶片",
        "65mm 柔和",
        "35mm 标准",
        "16mm 明显",
        "Super8 / 8mm 强烈",
        "自定义"
    };
    defineChoiceParam(ps, "bloomProfile", "辉光规格", "按画幅选择辉光扩散和强度。", PROFILE_AUTO, parent, options, 6);
}

static void defineDamageProfileParam(OfxParamSetHandle ps, const char* parent) {
    const char* options[] = {
        "跟随胶片",
        "65mm 干净",
        "35mm 放映拷贝",
        "16mm 档案片",
        "Super8 / 8mm 家用",
        "自定义"
    };
    defineChoiceParam(ps, "filmDamageProfile", "损伤规格", "按介质和放映拷贝状态选择灰尘、划痕和污点强度。", PROFILE_AUTO, parent, options, 6);
}

static void defineBreathProfileParam(OfxParamSetHandle ps, const char* parent) {
    const char* options[] = {
        "跟随胶片",
        "65mm 极轻",
        "35mm 轻微",
        "16mm 明显",
        "Super8 / 8mm 强烈",
        "自定义"
    };
    defineChoiceParam(ps, "filmBreathProfile", "呼吸规格", "按画幅和设备稳定性选择帧间曝光/饱和呼吸。", PROFILE_AUTO, parent, options, 6);
}

static void defineWeaveProfileParam(OfxParamSetHandle ps, const char* parent) {
    const char* options[] = {
        "跟随胶片",
        "65mm 稳定",
        "35mm 摄影机",
        "16mm 摄影机",
        "Super8 / 8mm 放映机",
        "自定义"
    };
    defineChoiceParam(ps, "gateWeaveProfile", "片门规格", "按摄影机或放映机稳定性选择片门抖动。", PROFILE_AUTO, parent, options, 6);
}

static void defineOverscanPresetParam(OfxParamSetHandle ps, const char* parent) {
    const char* options[] = {
        "无",
        "1.33 Academy",
        "1.66 European",
        "1.85 Widescreen",
        "2.39 Scope",
        "16mm / 8mm Gate"
    };
    defineChoiceParam(ps, "overscanPreset", "画幅比例", "类似 Dehancer Overscan 的快速画幅预设。", OVERSCAN_NONE, parent, options, 6);
}

static void definePreviewQualityParam(OfxParamSetHandle ps, const char* parent) {
    const char* options[] = { "快速预览", "平衡", "高质量" };
    defineChoiceParam(ps, "previewQuality", "预览质量", "降低光晕/辉光缓存分辨率以改善交互帧率。", PREVIEW_FAST, parent, options, 3);
}

static OfxStatus defineGroupParam(OfxParamSetHandle ps, const char* name, const char* label,
                                   int open, const char* parent) {
    OfxPropertySetHandle h;
    if (gParamSuite->paramDefine(ps, kOfxParamTypeGroup, name, &h) != kOfxStatOK)
        return kOfxStatErrBadHandle;
    gPropSuite->propSetString(h, kOfxPropLabel, 0, label);
    gPropSuite->propSetInt(h, kOfxParamPropGroupOpen, 0, open);
    if (parent) gPropSuite->propSetString(h, kOfxParamPropParent, 0, parent);
    return kOfxStatOK;
}

static void definePlaceholderSection(OfxParamSetHandle ps, const char* groupName, const char* groupLabel,
                                      const char* paramName, const char* hint) {
    defineGroupParam(ps, groupName, groupLabel, 0, "grpCustom");
    defineDoubleParam(ps, paramName, "占位强度", hint, 0.0, 0.0, 1.0, 0.01, groupName);
}

static void defineCustomPlaceholderParams(OfxParamSetHandle ps) {
    defineGroupParam(ps, "grpCustom", "自定义", 1, nullptr);
    definePreviewQualityParam(ps, "grpCustom");
    const char* effectModeOptions[] = { "完整模拟", "仅色彩", "仅纹理", "颗粒 + 光晕" };
    defineChoiceParam(ps, "effectMode", "作用范围", "快速限定插件参与渲染的模块。", EFFECT_FULL, "grpCustom", effectModeOptions, 4);

    defineGroupParam(ps, "grpExpand", "动态范围", 0, "grpCustom");
    defineEnabledParam(ps, "expandEnabled", "grpExpand");
    defineDoubleParam(ps, "expandAmount", "强度", "压低黑位、拉开高光，让画面对比范围更大。", 0.0, 0.0, 2.0, 0.01, "grpExpand");
    defineDoubleParam(ps, "expandBlack", "黑位", "向下压黑位，增加底片密度。", 0.35, 0.0, 1.0, 0.01, "grpExpand");
    defineDoubleParam(ps, "expandWhite", "白位", "向上拉开高光余量。", 0.45, 0.0, 1.0, 0.01, "grpExpand");
    defineDoubleParam(ps, "pushPull", "迫冲/拉冲", "负值拉冲降低反差，正值迫冲提高密度、反差和颗粒。", 0.0, -2.0, 2.0, 0.01, "grpExpand");
    defineDoubleParam(ps, "acutance", "边缘锐感", "增强或减弱胶片边缘微反差。", 0.0, -1.0, 1.0, 0.01, "grpExpand");

    defineGroupParam(ps, "grpPrint", "成片反差", 0, "grpCustom");
    defineEnabledParam(ps, "printEnabled", "grpPrint");
    const char* printStockOptions[] = { "Linear", "Cineon Film Log", "Kodak 2383", "Fuji 3513", "Kodak Endura", "Bromportrait BW" };
    defineChoiceParam(ps, "printStock", "印片介质", "选择印片介质或线性旁路。", PRINT_KODAK_2383, "grpPrint", printStockOptions, 6);
    defineDoubleParam(ps, "printAmount", "强度", "加强最终成片曲线、密度和高光暖色响应。", 0.0, 0.0, 2.0, 0.01, "grpPrint");
    defineDoubleParam(ps, "printContrast", "对比", "控制成片阶段的整体反差。", 0.35, 0.0, 1.0, 0.01, "grpPrint");
    defineDoubleParam(ps, "colorDensity", "色彩密度", "提高或降低印片阶段的色彩密度。", 0.35, 0.0, 1.0, 0.01, "grpPrint");
    defineDoubleParam(ps, "vibrance", "自然饱和", "优先提升低饱和颜色，避免过度推高高饱和区域。", 0.0, 0.0, 1.0, 0.01, "grpPrint");
    defineDoubleParam(ps, "printerRed", "红光", "模拟印片机红通道 printer light。", 0.0, -1.0, 1.0, 0.01, "grpPrint");
    defineDoubleParam(ps, "printerGreen", "绿光", "模拟印片机绿通道 printer light。", 0.0, -1.0, 1.0, 0.01, "grpPrint");
    defineDoubleParam(ps, "printerBlue", "蓝光", "模拟印片机蓝通道 printer light。", 0.0, -1.0, 1.0, 0.01, "grpPrint");
    defineDoubleParam(ps, "splitToneAmount", "分离调色", "让暗部和高光产生相反的冷暖偏移。", 0.0, 0.0, 1.0, 0.01, "grpPrint");
    defineDoubleParam(ps, "splitToneWarmth", "分离冷暖", "控制分离调色的暖色倾向。", 0.55, 0.0, 1.0, 0.01, "grpPrint");

    defineGroupParam(ps, "grpColorHead", "色彩校正", 0, "grpCustom");
    defineEnabledParam(ps, "colorHeadEnabled", "grpColorHead");
    defineDoubleParam(ps, "colorHeadAmount", "强度", "启用青、品红、黄三向色彩偏移。", 0.0, 0.0, 2.0, 0.01, "grpColorHead");
    defineDoubleParam(ps, "colorHeadCyan", "青", "负值偏红，正值偏青。", 0.0, -1.0, 1.0, 0.01, "grpColorHead");
    defineDoubleParam(ps, "colorHeadMagenta", "品红", "负值偏绿，正值偏品红。", 0.0, -1.0, 1.0, 0.01, "grpColorHead");
    defineDoubleParam(ps, "colorHeadYellow", "黄", "负值偏蓝，正值偏黄。", 0.0, -1.0, 1.0, 0.01, "grpColorHead");

    defineGroupParam(ps, "grpFilmGrain", "胶片颗粒", 0, "grpCustom");
    defineEnabledParam(ps, "filmGrainEnabled", "grpFilmGrain");
    defineGrainProfileParam(ps, "grpFilmGrain");
    const char* filmGrainModeOptions[] = { "真实颗粒", "细节保留", "染料云" };
    defineChoiceParam(ps, "filmGrainMode", "颗粒模式", "选择颗粒生成方式。", GRAIN_MODE_ANALOG, "grpFilmGrain", filmGrainModeOptions, 3);
    defineDoubleParam(ps, "filmGrainAmount", "强度", "移植 NativeEnhancer-FE 风格的时间化颗粒。", 0.0, 0.0, 2.0, 0.01, "grpFilmGrain");
    defineDoubleParam(ps, "filmGrainSize", "尺寸", "颗粒的空间尺度。", 0.35, 0.0, 1.0, 0.01, "grpFilmGrain");
    defineDoubleParam(ps, "filmGrainColor", "彩色颗粒", "0 为单色颗粒，1 为彩色颗粒。", 0.35, 0.0, 1.0, 0.01, "grpFilmGrain");
    defineDoubleParam(ps, "filmGrainShadows", "暗部颗粒", "暗部颗粒权重。", 0.55, 0.0, 1.0, 0.01, "grpFilmGrain");
    defineDoubleParam(ps, "filmGrainMids", "中灰颗粒", "中间调颗粒权重。", 0.70, 0.0, 1.0, 0.01, "grpFilmGrain");
    defineDoubleParam(ps, "filmGrainHighlights", "高光颗粒", "高光颗粒权重。", 0.35, 0.0, 1.0, 0.01, "grpFilmGrain");
    defineDoubleParam(ps, "filmGrainResolution", "颗粒解析度", "数值越高颗粒越细。", 0.50, 0.0, 1.0, 0.01, "grpFilmGrain");
    defineDoubleParam(ps, "filmGrainChroma", "色噪分量", "额外彩色颗粒分量。", 0.25, 0.0, 1.0, 0.01, "grpFilmGrain");

    defineGroupParam(ps, "grpHalationCustom", "光晕", 0, "grpCustom");
    defineEnabledParam(ps, "halationEnabled", "grpHalationCustom");
    defineHalationProfileParam(ps, "grpHalationCustom");
    defineDoubleParam(ps, "halationAmount", "强度", "参考 CineStill 800T 的红橙高光光晕。", 0.0, 0.0, 2.0, 0.01, "grpHalationCustom");
    defineDoubleParam(ps, "halationThreshold", "阈值", "只有超过阈值的高光参与光晕。", 0.58, 0.0, 1.0, 0.01, "grpHalationCustom");
    defineDoubleParam(ps, "halationRadius", "半径", "光晕扩散半径。", 0.55, 0.0, 1.0, 0.01, "grpHalationCustom");
    defineDoubleParam(ps, "halationWarmth", "暖色", "控制红、橙、品红倾向。", 0.80, 0.0, 1.0, 0.01, "grpHalationCustom");
    defineDoubleParam(ps, "halationLocal", "局部边缘", "靠近高光边缘的局部光晕强度。", 0.60, 0.0, 1.0, 0.01, "grpHalationCustom");
    defineDoubleParam(ps, "halationGlobal", "全局雾化", "更大范围的红橙雾化。", 0.30, 0.0, 1.0, 0.01, "grpHalationCustom");
    defineDoubleParam(ps, "halationHue", "色相偏移", "控制光晕从红橙到品红的偏移。", 0.35, 0.0, 1.0, 0.01, "grpHalationCustom");
    defineDoubleParam(ps, "halationBlueComp", "蓝光抑制", "减少蓝色通道对光晕的污染。", 0.50, 0.0, 1.0, 0.01, "grpHalationCustom");
    defineDoubleParam(ps, "halationImpact", "影响", "额外增强光晕对最终画面的混合比例。", 0.50, 0.0, 1.0, 0.01, "grpHalationCustom");
    defineDoubleParam(ps, "halationDefringe", "去紫边", "抑制高光边缘紫/蓝色溢出。", 0.20, 0.0, 1.0, 0.01, "grpHalationCustom");

    defineGroupParam(ps, "grpBloom", "辉光", 0, "grpCustom");
    defineEnabledParam(ps, "bloomEnabled", "grpBloom");
    defineBloomProfileParam(ps, "grpBloom");
    defineDoubleParam(ps, "bloomAmount", "强度", "柔化并扩散高光。", 0.0, 0.0, 2.0, 0.01, "grpBloom");
    defineDoubleParam(ps, "bloomThreshold", "阈值", "控制进入辉光的亮度门槛。", 0.66, 0.0, 1.0, 0.01, "grpBloom");
    defineDoubleParam(ps, "bloomRegion", "高光区域", "类似 ACR 锐化遮罩的区域选择，越高只影响越亮的高光。", 0.42, 0.0, 1.0, 0.01, "grpBloom");
    defineDoubleParam(ps, "bloomSoftness", "区域过渡", "控制辉光区域边缘的过渡宽度。", 0.38, 0.0, 1.0, 0.01, "grpBloom");
    const char* bloomMaskOptions[] = { "关闭", "显示辉光区域" };
    defineChoiceParam(ps, "bloomMaskPreview", "区域预览", "显示进入辉光的高光遮罩，方便调整阈值和区域。", BLOOM_MASK_OFF, "grpBloom", bloomMaskOptions, 2);
    defineDoubleParam(ps, "bloomRadius", "半径", "辉光扩散半径。", 0.45, 0.0, 1.0, 0.01, "grpBloom");
    defineDoubleParam(ps, "bloomDetail", "细节保留", "提高数值可保留更多高光细节。", 0.35, 0.0, 1.0, 0.01, "grpBloom");
    defineDoubleParam(ps, "bloomSaveLights", "保留光源", "减少辉光对光源主体的冲淡。", 0.45, 0.0, 1.0, 0.01, "grpBloom");
    defineDoubleParam(ps, "bloomDefringe", "去色边", "降低辉光边缘彩色污染。", 0.20, 0.0, 1.0, 0.01, "grpBloom");

    defineGroupParam(ps, "grpFilmDamage", "胶片损伤", 0, "grpCustom");
    defineEnabledParam(ps, "filmDamageEnabled", "grpFilmDamage");
    defineDamageProfileParam(ps, "grpFilmDamage");
    defineDoubleParam(ps, "filmDamageAmount", "强度", "总损伤强度。", 0.0, 0.0, 2.0, 0.01, "grpFilmDamage");
    defineDoubleParam(ps, "filmDust", "灰尘", "随机灰尘和污点数量。", 0.45, 0.0, 1.0, 0.01, "grpFilmDamage");
    defineDoubleParam(ps, "filmScratch", "划痕", "竖向划痕强度。", 0.35, 0.0, 1.0, 0.01, "grpFilmDamage");

    defineGroupParam(ps, "grpFilmBreath", "胶片呼吸", 0, "grpCustom");
    defineEnabledParam(ps, "filmBreathEnabled", "grpFilmBreath");
    defineBreathProfileParam(ps, "grpFilmBreath");
    defineDoubleParam(ps, "filmBreathAmount", "强度", "帧间曝光与饱和度呼吸。", 0.0, 0.0, 2.0, 0.01, "grpFilmBreath");
    defineDoubleParam(ps, "filmBreathExposure", "曝光呼吸", "控制亮度漂移。", 0.45, 0.0, 1.0, 0.01, "grpFilmBreath");
    defineDoubleParam(ps, "filmBreathSaturation", "饱和呼吸", "控制饱和度漂移。", 0.35, 0.0, 1.0, 0.01, "grpFilmBreath");

    defineGroupParam(ps, "grpGateWeave", "片门抖动", 0, "grpCustom");
    defineEnabledParam(ps, "gateWeaveEnabled", "grpGateWeave");
    defineWeaveProfileParam(ps, "grpGateWeave");
    defineDoubleParam(ps, "gateWeaveAmount", "强度", "模拟片门不稳定造成的画面漂移。", 0.0, 0.0, 2.0, 0.01, "grpGateWeave");
    defineDoubleParam(ps, "gateWeaveX", "水平", "横向漂移比例。", 0.50, 0.0, 1.0, 0.01, "grpGateWeave");
    defineDoubleParam(ps, "gateWeaveY", "垂直", "纵向漂移比例。", 0.35, 0.0, 1.0, 0.01, "grpGateWeave");

    defineGroupParam(ps, "grpOverscan", "边缘放大", 0, "grpCustom");
    defineEnabledParam(ps, "overscanEnabled", "grpOverscan");
    defineOverscanPresetParam(ps, "grpOverscan");
    defineDoubleParam(ps, "overscanAmount", "强度", "轻微放大画面，用来藏住抖动后的边缘。", 0.0, 0.0, 2.0, 0.01, "grpOverscan");

    defineGroupParam(ps, "grpVignetteCustom", "暗角", 0, "grpCustom");
    defineEnabledParam(ps, "vignetteEnabled", "grpVignetteCustom");
    defineDoubleParam(ps, "vignetteAmount", "强度", "镜头暗角强度。", 0.0, 0.0, 2.0, 0.01, "grpVignetteCustom");
    defineDoubleParam(ps, "vignetteRadius", "半径", "暗角开始位置。", 0.70, 0.0, 1.0, 0.01, "grpVignetteCustom");
    defineDoubleParam(ps, "vignetteFeather", "羽化", "暗角过渡宽度。", 0.55, 0.0, 1.0, 0.01, "grpVignetteCustom");
}

static void setDoubleParamIfPresent(OfxParamSetHandle pSet, const char* name, double value) {
    OfxParamHandle ph = nullptr;
    if (gParamSuite->paramGetHandle(pSet, name, &ph, nullptr) == kOfxStatOK)
        gParamSuite->paramSetValue(ph, value);
}

static void setIntParamIfPresent(OfxParamSetHandle pSet, const char* name, int value) {
    OfxParamHandle ph = nullptr;
    if (gParamSuite->paramGetHandle(pSet, name, &ph, nullptr) == kOfxStatOK)
        gParamSuite->paramSetValue(ph, value);
}

static int getIntParamIfPresent(OfxParamSetHandle pSet, const char* name, int fallback) {
    OfxParamHandle ph = nullptr;
    int value = fallback;
    if (gParamSuite->paramGetHandle(pSet, name, &ph, nullptr) == kOfxStatOK)
        gParamSuite->paramGetValue(ph, &value);
    return value;
}

static void setSecretIfPresent(OfxParamSetHandle pSet, const char* name) {
    OfxParamHandle ph = nullptr;
    OfxPropertySetHandle props = nullptr;
    if (gParamSuite->paramGetHandle(pSet, name, &ph, &props) == kOfxStatOK && props)
        gPropSuite->propSetInt(props, kOfxParamPropSecret, 0, 1);
}

struct PresetCustomDefaults {
    int expandEnabled;
    int printEnabled;
    int colorHeadEnabled;
    int filmGrainEnabled;
    int halationEnabled;
    int bloomEnabled;
    int filmDamageEnabled;
    int filmBreathEnabled;
    int gateWeaveEnabled;
    int overscanEnabled;
    int vignetteEnabled;
    double expandAmount;
    double printAmount;
    double colorDensity;
    double vibrance;
    double filmGrainAmount;
    double filmGrainShadows;
    double filmGrainMids;
    double filmGrainHighlights;
    double filmGrainResolution;
    double filmGrainChroma;
    double halationAmount;
    double halationThreshold;
    double halationRadius;
    double halationWarmth;
    double halationLocal;
    double halationGlobal;
    double halationHue;
    double halationBlueComp;
    double halationImpact;
    double halationDefringe;
    double bloomAmount;
    double bloomThreshold;
    double bloomRegion;
    double bloomSoftness;
    double bloomRadius;
    double bloomDetail;
    double bloomSaveLights;
    double bloomDefringe;
    double vignetteAmount;
    int printStock;
};

static void applyBlackAndWhiteStockDefaults(int idx, PresetCustomDefaults& d, float speed) {
    d.printStock = PRINT_BW_BROMOPORTRAIT;
    d.colorDensity = 0.16;
    d.vibrance = 0.0;
    d.filmGrainChroma = 0.0;
    d.halationEnabled = ENABLE_SKIP;
    d.halationAmount = 0.0;
    d.bloomEnabled = ENABLE_SKIP;
    d.bloomAmount = 0.0;

    if (presetNameHas(idx, "T-Max 3200") || presetNameHas(idx, "Delta 3200") || presetNameHas(idx, "Neopan 1600")) {
        d.printAmount = 0.34;
        d.colorDensity = 0.24;
        d.filmGrainAmount = 0.50 + (double)speed * 0.10;
        d.filmGrainShadows = 0.72;
        d.filmGrainMids = 0.82;
        d.filmGrainHighlights = 0.44;
        d.filmGrainResolution = 0.36;
        d.vignetteAmount = fmaxf((float)d.vignetteAmount, 0.08f);
    } else if (presetNameHas(idx, "Tri-X") || presetNameHas(idx, "HP5")) {
        d.printAmount = 0.31;
        d.colorDensity = 0.22;
        d.filmGrainAmount = 0.36;
        d.filmGrainShadows = 0.66;
        d.filmGrainMids = 0.76;
        d.filmGrainHighlights = 0.36;
        d.filmGrainResolution = 0.48;
        d.vignetteAmount = fmaxf((float)d.vignetteAmount, 0.065f);
    } else if (presetNameHas(idx, "Acros") || presetNameHas(idx, "APX 25") || presetNameHas(idx, "APX 100") ||
               presetNameHas(idx, "Delta 100") || presetNameHas(idx, "FP4")) {
        d.printAmount = 0.20;
        d.colorDensity = 0.14;
        d.filmGrainAmount = 0.12 + (double)speed * 0.04;
        d.filmGrainShadows = 0.44;
        d.filmGrainMids = 0.60;
        d.filmGrainHighlights = 0.24;
        d.filmGrainResolution = 0.90;
        d.vignetteAmount = 0.035;
    } else if (presetNameHas(idx, "XP2")) {
        d.printAmount = 0.24;
        d.colorDensity = 0.18;
        d.filmGrainAmount = 0.18;
        d.filmGrainShadows = 0.52;
        d.filmGrainMids = 0.66;
        d.filmGrainHighlights = 0.30;
        d.filmGrainResolution = 0.78;
        d.vignetteAmount = 0.045;
    } else {
        d.printAmount = 0.25 + (double)speed * 0.04;
        d.filmGrainAmount = 0.18 + (double)speed * 0.22;
        d.filmGrainResolution = clampf(0.72f - speed * 0.20f, 0.36f, 0.90f);
    }
}

static void applyFujiAgfaCineStillDefaults(int idx, PresetCustomDefaults& d, float speed) {
    if (presetNameHas(idx, "CineStill 800T")) {
        d.printAmount = 0.24;
        d.colorDensity = 0.30;
        d.vibrance = 0.0;
        d.filmGrainAmount = 0.30;
        d.filmGrainResolution = 0.55;
        d.filmGrainChroma = 0.32;
        d.halationEnabled = ENABLE_ON;
        d.halationAmount = 0.34;
        d.halationThreshold = 0.74;
        d.halationRadius = 0.60;
        d.halationWarmth = 0.96;
        d.halationLocal = 0.72;
        d.halationGlobal = 0.025;
        d.halationHue = 0.72;
        d.halationBlueComp = 0.86;
        d.halationImpact = 0.58;
        d.halationDefringe = 0.42;
        d.bloomEnabled = ENABLE_SKIP;
        d.bloomAmount = 0.0;
        d.bloomThreshold = 0.90;
        d.bloomRegion = 0.80;
        d.bloomRadius = 0.30;
    } else if (presetNameHas(idx, "CineStill 50D")) {
        d.printAmount = 0.22;
        d.colorDensity = 0.32;
        d.vibrance = 0.02;
        d.filmGrainAmount = 0.08;
        d.filmGrainResolution = 0.88;
        d.filmGrainChroma = 0.16;
        d.halationEnabled = ENABLE_ON;
        d.halationAmount = 0.08;
        d.halationThreshold = 0.86;
        d.halationRadius = 0.32;
        d.halationWarmth = 0.76;
        d.halationLocal = 0.30;
        d.halationGlobal = 0.04;
        d.halationHue = 0.34;
        d.halationBlueComp = 0.50;
        d.halationImpact = 0.20;
        d.halationDefringe = 0.28;
        d.bloomEnabled = ENABLE_SKIP;
        d.bloomAmount = 0.0;
    } else if (presetNameHas(idx, "Superia 1600") || presetNameHas(idx, "Superia HG 1600")) {
        d.printAmount = 0.34;
        d.colorDensity = 0.44;
        d.vibrance = 0.18;
        d.filmGrainAmount = 0.34;
        d.filmGrainResolution = 0.54;
        d.filmGrainChroma = 0.30;
        d.halationEnabled = ENABLE_ON;
        d.halationAmount = 0.018;
        d.halationThreshold = 0.86;
        d.halationRadius = 0.24;
        d.halationWarmth = 0.50;
        d.bloomEnabled = ENABLE_SKIP;
        d.bloomAmount = 0.0;
    } else if (presetNameHas(idx, "Superia")) {
        d.printAmount = 0.30;
        d.colorDensity = 0.40;
        d.vibrance = 0.14;
        d.filmGrainAmount = 0.13 + (double)speed * 0.10;
        d.filmGrainResolution = clampf(0.82f - speed * 0.22f, 0.58f, 0.84f);
        d.filmGrainChroma = 0.22;
        d.bloomEnabled = ENABLE_SKIP;
        d.bloomAmount = 0.0;
    } else if (presetNameHas(idx, "Agfa Vista")) {
        d.printAmount = 0.18;
        d.colorDensity = 0.37;
        d.vibrance = 0.08;
        d.filmGrainAmount = 0.11 + (double)speed * 0.08;
        d.filmGrainResolution = clampf(0.84f - speed * 0.18f, 0.64f, 0.86f);
        d.filmGrainChroma = 0.18;
        d.halationEnabled = ENABLE_SKIP;
        d.halationAmount = 0.0;
        d.bloomEnabled = ENABLE_SKIP;
        d.bloomAmount = 0.0;
    }
}

static void applyKodakColorNegativeDefaults(int idx, PresetCustomDefaults& d, float speed) {
    if (presetNameHas(idx, "Ektar 100")) {
        d.printAmount = 0.34;
        d.colorDensity = 0.46;
        d.vibrance = 0.22;
        d.filmGrainAmount = 0.07;
        d.filmGrainResolution = 0.92;
        d.filmGrainChroma = 0.14;
        d.halationEnabled = ENABLE_SKIP;
        d.halationAmount = 0.0;
        d.bloomEnabled = ENABLE_SKIP;
        d.bloomAmount = 0.0;
    } else if (presetNameHas(idx, "Gold 200") || presetNameHas(idx, "ColorPlus 200")) {
        d.printAmount = 0.31;
        d.colorDensity = 0.42;
        d.vibrance = 0.14;
        d.filmGrainAmount = 0.18 + (double)speed * 0.03;
        d.filmGrainResolution = 0.70;
        d.filmGrainChroma = 0.20;
        d.bloomEnabled = ENABLE_SKIP;
        d.bloomAmount = 0.0;
    } else if (presetNameHas(idx, "Ultra Max 400")) {
        d.printAmount = 0.33;
        d.colorDensity = 0.44;
        d.vibrance = 0.16;
        d.filmGrainAmount = 0.26;
        d.filmGrainResolution = 0.62;
        d.filmGrainChroma = 0.24;
        d.halationEnabled = ENABLE_ON;
        d.halationAmount = 0.018;
        d.halationThreshold = 0.86;
        d.halationRadius = 0.24;
        d.halationWarmth = 0.58;
        d.bloomEnabled = ENABLE_SKIP;
        d.bloomAmount = 0.0;
    } else if (presetNameHas(idx, "Portra 800")) {
        d.printAmount = 0.25;
        d.colorDensity = 0.32;
        d.vibrance = 0.02;
        d.filmGrainAmount = 0.28;
        d.filmGrainResolution = 0.58;
        d.filmGrainChroma = 0.22;
        d.halationEnabled = ENABLE_ON;
        d.halationAmount = 0.018;
        d.halationThreshold = 0.88;
        d.halationRadius = 0.22;
        d.halationWarmth = 0.54;
        d.bloomEnabled = ENABLE_SKIP;
        d.bloomAmount = 0.0;
    } else if (presetNameHas(idx, "Portra 400")) {
        d.printAmount = 0.22;
        d.colorDensity = 0.30;
        d.vibrance = 0.0;
        d.filmGrainAmount = 0.16;
        d.filmGrainResolution = 0.74;
        d.filmGrainChroma = 0.16;
        d.halationEnabled = ENABLE_SKIP;
        d.halationAmount = 0.0;
        d.bloomEnabled = ENABLE_SKIP;
        d.bloomAmount = 0.0;
    } else if (presetNameHas(idx, "Portra 160")) {
        d.printAmount = 0.20;
        d.colorDensity = 0.28;
        d.vibrance = 0.0;
        d.filmGrainAmount = 0.10;
        d.filmGrainResolution = 0.86;
        d.filmGrainChroma = 0.12;
        d.halationEnabled = ENABLE_SKIP;
        d.halationAmount = 0.0;
        d.bloomEnabled = ENABLE_SKIP;
        d.bloomAmount = 0.0;
    }
}

static void applyHighValueColorStockDefaults(int idx, PresetCustomDefaults& d, float speed) {
    if (presetNameHas(idx, "Pro 800Z")) {
        d.printAmount = 0.22;
        d.colorDensity = 0.32;
        d.vibrance = 0.0;
        d.filmGrainAmount = 0.34 + (double)speed * 0.04;
        d.filmGrainResolution = 0.54;
        d.filmGrainChroma = 0.28;
        d.halationEnabled = ENABLE_ON;
        d.halationAmount = 0.025;
        d.halationThreshold = 0.86;
        d.halationRadius = 0.26;
        d.halationWarmth = 0.50;
        d.halationLocal = 0.26;
        d.halationGlobal = 0.03;
    } else if (presetNameHas(idx, "Astia")) {
        d.printAmount = 0.24;
        d.colorDensity = 0.30;
        d.vibrance = 0.05;
        d.filmGrainAmount = 0.08;
        d.filmGrainResolution = 0.90;
        d.filmGrainChroma = 0.14;
        d.bloomEnabled = ENABLE_SKIP;
        d.bloomAmount = 0.0;
    } else if (presetNameHas(idx, "Fortia")) {
        d.printAmount = 0.36;
        d.colorDensity = 0.50;
        d.vibrance = 0.22;
        d.filmGrainAmount = 0.06;
        d.filmGrainResolution = 0.94;
        d.filmGrainChroma = 0.12;
        d.bloomEnabled = ENABLE_SKIP;
        d.bloomAmount = 0.0;
    } else if (presetNameHas(idx, "Elite Chrome")) {
        d.printAmount = 0.30;
        d.colorDensity = 0.40;
        d.vibrance = 0.08;
        d.filmGrainAmount = 0.20;
        d.filmGrainResolution = 0.68;
        d.filmGrainChroma = 0.22;
        d.bloomEnabled = ENABLE_SKIP;
        d.bloomAmount = 0.0;
    }
}

static PresetCustomDefaults presetCustomDefaultsForStock(int globalIdx) {
    int idx = globalIdx < 0 || globalIdx >= gNumPresets ? 32 : globalIdx;
    bool bw = isBlackAndWhitePreset(idx);
    int iso = stockIsoForPreset(idx);
    StockHalationClass halationClass = stockHalationClassForPreset(idx);
    float speed = clampf(((float)iso - 50.0f) / 1550.0f, 0.0f, 1.0f);
    GrainProfile gp = stockGrainDefaultsForPreset(idx);
    PresetCustomDefaults d = {
        ENABLE_ON,
        ENABLE_ON,
        ENABLE_SKIP,
        ENABLE_ON,
        ENABLE_SKIP,
        ENABLE_SKIP,
        ENABLE_SKIP,
        ENABLE_SKIP,
        ENABLE_SKIP,
        ENABLE_SKIP,
        ENABLE_SKIP,
        0.04 + speed * 0.05,
        bw ? 0.22 : 0.18 + speed * 0.06,
        bw ? 0.20 : 0.36 + (gPresets[idx].saturation - 1.0) * 0.18,
        bw ? 0.00 : fmaxf(0.0f, (float)gPresets[idx].saturation - 1.0f) * 0.45f,
        0.10 + (double)gPresets[idx].grain * 0.72 + speed * 0.08,
        gp.shadows,
        gp.mids,
        gp.highlights,
        gp.resolution,
        gp.chroma,
        0.0,
        0.74,
        0.42,
        0.70,
        0.42,
        0.14,
        0.32,
        0.42,
        0.30,
        0.18,
        0.0,
        0.82,
        0.55,
        0.34,
        0.34,
        0.48,
        0.24,
        0.24,
        0.06 + (double)gPresets[idx].vignette * 0.40,
        bw ? PRINT_BW_BROMOPORTRAIT : PRINT_KODAK_2383
    };
    if (bw) {
        applyBlackAndWhiteStockDefaults(idx, d, speed);
    }
    if (halationClass == HALATION_STOCK_NONE) {
        d.halationAmount = 0.0;
        d.halationEnabled = ENABLE_SKIP;
        d.bloomAmount = 0.0;
        d.bloomEnabled = ENABLE_SKIP;
    } else if (halationClass == HALATION_STOCK_SUBTLE) {
        d.halationEnabled = ENABLE_ON;
        d.halationAmount = 0.012 + speed * 0.010;
        d.halationThreshold = 0.86;
        d.halationRadius = 0.22;
        d.halationWarmth = 0.52;
        d.halationLocal = 0.22;
        d.halationGlobal = 0.03;
        d.halationHue = 0.28;
        d.halationBlueComp = 0.42;
        d.halationImpact = 0.12;
        d.halationDefringe = 0.18;
        d.bloomAmount = 0.0;
        d.bloomEnabled = ENABLE_SKIP;
    }
    if (presetNameHas(idx, "Velvia") || presetNameHas(idx, "Ektachrome") || presetNameHas(idx, "Provia")) {
        d.printAmount += 0.06;
        d.colorDensity += 0.05;
        d.bloomAmount = 0.0;
        d.bloomEnabled = ENABLE_SKIP;
    }
    if (presetNameHas(idx, "Portra")) {
        d.printAmount *= 0.92;
        d.vibrance *= 0.72;
        if (!presetNameHas(idx, "Portra 800")) {
            d.halationAmount = 0.0;
            d.halationEnabled = ENABLE_SKIP;
        }
        d.bloomAmount = 0.0;
        d.bloomEnabled = ENABLE_SKIP;
    }
    if (!bw) {
        applyFujiAgfaCineStillDefaults(idx, d, speed);
        applyKodakColorNegativeDefaults(idx, d, speed);
        applyHighValueColorStockDefaults(idx, d, speed);
    }
    if (d.vignetteAmount > 0.005) {
        d.vignetteEnabled = ENABLE_ON;
    }
    return d;
}

static int currentFilmIndex(OfxParamSetHandle pSet) {
    int filmIdx = getIntParamIfPresent(pSet, "filmPreset", 32);
    if (filmIdx < 0) return 32;
    if (filmIdx >= gNumPresets) return gNumPresets - 1;
    return filmIdx;
}

static void applyFilmGrainProfileDefaults(OfxParamSetHandle pSet, int filmIdx, int choice) {
    if (choice == GRAIN_CUSTOM) return;
    GrainProfile gp = stockGrainProfileForPreset(filmIdx, choice);
    PresetCustomDefaults d = presetCustomDefaultsForStock(filmIdx);
    setIntParamIfPresent(pSet, "filmGrainEnabled", ENABLE_ON);
    setDoubleParamIfPresent(pSet, "filmGrainAmount", clampf((float)(d.filmGrainAmount * (0.70 + 0.36 * gp.gain)), 0.0f, 2.0f));
    setDoubleParamIfPresent(pSet, "filmGrainSize", gp.size);
    setDoubleParamIfPresent(pSet, "filmGrainColor", gp.color);
    setDoubleParamIfPresent(pSet, "filmGrainShadows", gp.shadows);
    setDoubleParamIfPresent(pSet, "filmGrainMids", gp.mids);
    setDoubleParamIfPresent(pSet, "filmGrainHighlights", gp.highlights);
    setDoubleParamIfPresent(pSet, "filmGrainResolution", gp.resolution);
    setDoubleParamIfPresent(pSet, "filmGrainChroma", gp.chroma);
}

static void applyHalationProfileDefaults(OfxParamSetHandle pSet, int filmIdx, int choice) {
    if (choice == HALATION_CUSTOM) return;
    float radiusGain = 1.0f, warmthGain = 1.0f;
    float gain = halationGainForProfile(filmIdx, choice, radiusGain, warmthGain);
    PresetCustomDefaults d = presetCustomDefaultsForStock(filmIdx);
    bool noRemjet = (choice >= HALATION_65MM_NO_REMJET && choice <= HALATION_8MM_NO_REMJET) || presetNameHas(filmIdx, "CineStill");
    if (choice == PROFILE_AUTO) {
        setIntParamIfPresent(pSet, "halationEnabled", d.halationEnabled);
        setDoubleParamIfPresent(pSet, "halationAmount", d.halationAmount);
        setDoubleParamIfPresent(pSet, "halationThreshold", d.halationThreshold);
        setDoubleParamIfPresent(pSet, "halationRadius", d.halationRadius);
        setDoubleParamIfPresent(pSet, "halationWarmth", d.halationWarmth);
        setDoubleParamIfPresent(pSet, "halationLocal", d.halationLocal);
        setDoubleParamIfPresent(pSet, "halationGlobal", d.halationGlobal);
        setDoubleParamIfPresent(pSet, "halationHue", d.halationHue);
        setDoubleParamIfPresent(pSet, "halationBlueComp", d.halationBlueComp);
        setDoubleParamIfPresent(pSet, "halationImpact", d.halationImpact);
        setDoubleParamIfPresent(pSet, "halationDefringe", d.halationDefringe);
        return;
    }
    double manualHalationBase = d.halationAmount;
    if (choice != PROFILE_AUTO && d.halationAmount <= 0.0001) {
        manualHalationBase = noRemjet ? 0.090 : 0.045;
    }
    setIntParamIfPresent(pSet, "halationEnabled", ENABLE_ON);
    setDoubleParamIfPresent(pSet, "halationAmount", clampf((float)(manualHalationBase * gain), 0.0f, 2.0f));
    setDoubleParamIfPresent(pSet, "halationThreshold", noRemjet ? 0.78 : 0.84);
    setDoubleParamIfPresent(pSet, "halationRadius", clampf(0.50f * radiusGain, 0.0f, 1.0f));
    setDoubleParamIfPresent(pSet, "halationWarmth", clampf(0.66f * warmthGain, 0.0f, 1.0f));
    setDoubleParamIfPresent(pSet, "halationLocal", noRemjet ? 0.44 : 0.30);
    setDoubleParamIfPresent(pSet, "halationGlobal", noRemjet ? 0.10 : 0.05);
    setDoubleParamIfPresent(pSet, "halationHue", noRemjet ? 0.42 : 0.32);
    setDoubleParamIfPresent(pSet, "halationBlueComp", noRemjet ? 0.66 : 0.46);
    setDoubleParamIfPresent(pSet, "halationImpact", clampf(0.18f + 0.10f * gain, 0.0f, 1.0f));
    setDoubleParamIfPresent(pSet, "halationDefringe", noRemjet ? 0.34 : 0.22);
}

static void applyBloomProfileDefaults(OfxParamSetHandle pSet, int filmIdx, int choice) {
    if (choice == BLOOM_CUSTOM) return;
    float radiusGain = 1.0f;
    float gain = bloomGainForProfile(filmIdx, choice, radiusGain);
    PresetCustomDefaults d = presetCustomDefaultsForStock(filmIdx);
    if (choice == PROFILE_AUTO) {
        setIntParamIfPresent(pSet, "bloomEnabled", d.bloomEnabled);
        setDoubleParamIfPresent(pSet, "bloomAmount", d.bloomAmount);
        setDoubleParamIfPresent(pSet, "bloomThreshold", d.bloomThreshold);
        setDoubleParamIfPresent(pSet, "bloomRegion", d.bloomRegion);
        setDoubleParamIfPresent(pSet, "bloomSoftness", d.bloomSoftness);
        setDoubleParamIfPresent(pSet, "bloomRadius", d.bloomRadius);
        setDoubleParamIfPresent(pSet, "bloomDetail", d.bloomDetail);
        setDoubleParamIfPresent(pSet, "bloomSaveLights", d.bloomSaveLights);
        setDoubleParamIfPresent(pSet, "bloomDefringe", d.bloomDefringe);
        setIntParamIfPresent(pSet, "bloomMaskPreview", BLOOM_MASK_OFF);
        return;
    }
    double manualBloomBase = d.bloomAmount;
    if (choice != PROFILE_AUTO && d.bloomAmount <= 0.0001) {
        manualBloomBase = 0.045;
    }
    setIntParamIfPresent(pSet, "bloomEnabled", ENABLE_ON);
    setDoubleParamIfPresent(pSet, "bloomAmount", clampf((float)(manualBloomBase * gain), 0.0f, 2.0f));
    setDoubleParamIfPresent(pSet, "bloomThreshold", d.bloomThreshold);
    setDoubleParamIfPresent(pSet, "bloomRegion", d.bloomRegion);
    setDoubleParamIfPresent(pSet, "bloomSoftness", d.bloomSoftness);
    setDoubleParamIfPresent(pSet, "bloomRadius", clampf(0.42f * radiusGain, 0.0f, 1.0f));
    setDoubleParamIfPresent(pSet, "bloomDetail", clampf(0.45f - 0.10f * (gain - 1.0f), 0.18f, 0.68f));
    setDoubleParamIfPresent(pSet, "bloomSaveLights", clampf(0.54f - 0.08f * (gain - 1.0f), 0.25f, 0.75f));
    setDoubleParamIfPresent(pSet, "bloomDefringe", 0.24);
    setIntParamIfPresent(pSet, "bloomMaskPreview", BLOOM_MASK_OFF);
}

static void applyDamageProfileDefaults(OfxParamSetHandle pSet, int filmIdx, int choice) {
    if (choice == DAMAGE_CUSTOM) return;
    float dustGain = 1.0f, scratchGain = 1.0f;
    float gain = damageGainForProfile(filmIdx, choice, dustGain, scratchGain);
    setIntParamIfPresent(pSet, "filmDamageEnabled", ENABLE_ON);
    setDoubleParamIfPresent(pSet, "filmDamageAmount", clampf(0.08f * gain, 0.0f, 2.0f));
    setDoubleParamIfPresent(pSet, "filmDust", clampf(0.42f * dustGain, 0.0f, 1.0f));
    setDoubleParamIfPresent(pSet, "filmScratch", clampf(0.30f * scratchGain, 0.0f, 1.0f));
}

static void applyBreathProfileDefaults(OfxParamSetHandle pSet, int filmIdx, int choice) {
    if (choice == BREATH_CUSTOM) return;
    float exposureGain = 1.0f, saturationGain = 1.0f;
    float gain = breathGainForProfile(filmIdx, choice, exposureGain, saturationGain);
    setIntParamIfPresent(pSet, "filmBreathEnabled", ENABLE_ON);
    setDoubleParamIfPresent(pSet, "filmBreathAmount", clampf(0.09f * gain, 0.0f, 2.0f));
    setDoubleParamIfPresent(pSet, "filmBreathExposure", clampf(0.44f * exposureGain, 0.0f, 1.0f));
    setDoubleParamIfPresent(pSet, "filmBreathSaturation", clampf(0.34f * saturationGain, 0.0f, 1.0f));
}

static void applyWeaveProfileDefaults(OfxParamSetHandle pSet, int filmIdx, int choice) {
    if (choice == WEAVE_CUSTOM) return;
    float xGain = 1.0f, yGain = 1.0f;
    float gain = weaveGainForProfile(filmIdx, choice, xGain, yGain);
    setIntParamIfPresent(pSet, "gateWeaveEnabled", ENABLE_ON);
    setDoubleParamIfPresent(pSet, "gateWeaveAmount", clampf(0.07f * gain, 0.0f, 2.0f));
    setDoubleParamIfPresent(pSet, "gateWeaveX", clampf(0.50f * xGain, 0.0f, 1.0f));
    setDoubleParamIfPresent(pSet, "gateWeaveY", clampf(0.36f * yGain, 0.0f, 1.0f));
}

static void applyOverscanPresetDefaults(OfxParamSetHandle pSet, int choice) {
    setIntParamIfPresent(pSet, "overscanEnabled", choice == OVERSCAN_NONE ? ENABLE_SKIP : ENABLE_ON);
    setDoubleParamIfPresent(pSet, "overscanAmount", clampf(overscanScaleForPreset(choice) * 12.0f, 0.0f, 2.0f));
}

static void applyPrintStockDefaults(OfxParamSetHandle pSet, int choice) {
    setIntParamIfPresent(pSet, "printEnabled", choice == PRINT_LINEAR ? ENABLE_SKIP : ENABLE_ON);
    switch (choice) {
        case PRINT_LINEAR:
            setDoubleParamIfPresent(pSet, "printAmount", 0.0);
            setDoubleParamIfPresent(pSet, "printContrast", 0.0);
            setDoubleParamIfPresent(pSet, "colorDensity", 0.35);
            setDoubleParamIfPresent(pSet, "vibrance", 0.0);
            break;
        case PRINT_FUJI_3513:
            setDoubleParamIfPresent(pSet, "printAmount", 0.24);
            setDoubleParamIfPresent(pSet, "printContrast", 0.42);
            setDoubleParamIfPresent(pSet, "colorDensity", 0.42);
            setDoubleParamIfPresent(pSet, "vibrance", 0.10);
            break;
        case PRINT_KODAK_ENDURA:
            setDoubleParamIfPresent(pSet, "printAmount", 0.20);
            setDoubleParamIfPresent(pSet, "printContrast", 0.34);
            setDoubleParamIfPresent(pSet, "colorDensity", 0.46);
            setDoubleParamIfPresent(pSet, "vibrance", 0.06);
            break;
        case PRINT_BW_BROMOPORTRAIT:
            setDoubleParamIfPresent(pSet, "printAmount", 0.26);
            setDoubleParamIfPresent(pSet, "printContrast", 0.48);
            setDoubleParamIfPresent(pSet, "colorDensity", 0.22);
            setDoubleParamIfPresent(pSet, "vibrance", 0.0);
            break;
        case PRINT_CINEON_LOG:
            setDoubleParamIfPresent(pSet, "printAmount", 0.16);
            setDoubleParamIfPresent(pSet, "printContrast", 0.26);
            setDoubleParamIfPresent(pSet, "colorDensity", 0.35);
            setDoubleParamIfPresent(pSet, "vibrance", 0.04);
            break;
        default:
            setDoubleParamIfPresent(pSet, "printAmount", 0.22);
            setDoubleParamIfPresent(pSet, "printContrast", 0.38);
            setDoubleParamIfPresent(pSet, "colorDensity", 0.40);
            setDoubleParamIfPresent(pSet, "vibrance", 0.06);
            break;
    }
}

static void applyFilmGrainModeDefaults(OfxParamSetHandle pSet, int filmIdx, int mode) {
    int profile = getIntParamIfPresent(pSet, "filmGrainProfile", PROFILE_AUTO);
    GrainProfile gp = stockGrainProfileForPreset(filmIdx, profile);
    setIntParamIfPresent(pSet, "filmGrainEnabled", ENABLE_ON);
    if (mode == GRAIN_MODE_DETAIL) {
        setDoubleParamIfPresent(pSet, "filmGrainAmount", 0.12 + gp.gain * 0.06);
        setDoubleParamIfPresent(pSet, "filmGrainSize", clampf(gp.size * 0.76f, 0.0f, 1.0f));
        setDoubleParamIfPresent(pSet, "filmGrainResolution", clampf(gp.resolution + 0.16f, 0.0f, 1.0f));
        setDoubleParamIfPresent(pSet, "filmGrainChroma", clampf(gp.chroma * 0.70f, 0.0f, 1.0f));
    } else if (mode == GRAIN_MODE_DYE_CLOUD) {
        setDoubleParamIfPresent(pSet, "filmGrainAmount", 0.14 + gp.gain * 0.08);
        setDoubleParamIfPresent(pSet, "filmGrainSize", clampf(gp.size * 1.12f, 0.0f, 1.0f));
        setDoubleParamIfPresent(pSet, "filmGrainColor", clampf(gp.color + 0.22f, 0.0f, 1.0f));
        setDoubleParamIfPresent(pSet, "filmGrainChroma", clampf(gp.chroma + 0.18f, 0.0f, 1.0f));
    } else {
        applyFilmGrainProfileDefaults(pSet, filmIdx, profile);
    }
}

static bool syncCustomProfileSliders(OfxParamSetHandle pSet, const char* paramName) {
    int filmIdx = currentFilmIndex(pSet);
    gParamSuite->paramEditBegin(pSet, "custom-profile-sync");
    bool handled = true;
    if (strcmp(paramName, "filmGrainProfile") == 0) {
        applyFilmGrainProfileDefaults(pSet, filmIdx, getIntParamIfPresent(pSet, "filmGrainProfile", PROFILE_AUTO));
    } else if (strcmp(paramName, "halationProfile") == 0) {
        applyHalationProfileDefaults(pSet, filmIdx, getIntParamIfPresent(pSet, "halationProfile", PROFILE_AUTO));
    } else if (strcmp(paramName, "bloomProfile") == 0) {
        applyBloomProfileDefaults(pSet, filmIdx, getIntParamIfPresent(pSet, "bloomProfile", PROFILE_AUTO));
    } else if (strcmp(paramName, "filmDamageProfile") == 0) {
        applyDamageProfileDefaults(pSet, filmIdx, getIntParamIfPresent(pSet, "filmDamageProfile", PROFILE_AUTO));
    } else if (strcmp(paramName, "filmBreathProfile") == 0) {
        applyBreathProfileDefaults(pSet, filmIdx, getIntParamIfPresent(pSet, "filmBreathProfile", PROFILE_AUTO));
    } else if (strcmp(paramName, "gateWeaveProfile") == 0) {
        applyWeaveProfileDefaults(pSet, filmIdx, getIntParamIfPresent(pSet, "gateWeaveProfile", PROFILE_AUTO));
    } else if (strcmp(paramName, "overscanPreset") == 0) {
        applyOverscanPresetDefaults(pSet, getIntParamIfPresent(pSet, "overscanPreset", OVERSCAN_NONE));
    } else if (strcmp(paramName, "printStock") == 0) {
        applyPrintStockDefaults(pSet, getIntParamIfPresent(pSet, "printStock", PRINT_KODAK_2383));
    } else if (strcmp(paramName, "filmGrainMode") == 0) {
        applyFilmGrainModeDefaults(pSet, filmIdx, getIntParamIfPresent(pSet, "filmGrainMode", GRAIN_MODE_ANALOG));
    } else {
        handled = false;
    }
    gParamSuite->paramEditEnd(pSet);
    return handled;
}

static void applyPresetCustomDefaults(OfxParamSetHandle pSet, int globalIdx) {
    PresetCustomDefaults d = presetCustomDefaultsForStock(globalIdx);
    setIntParamIfPresent(pSet, "expandEnabled", d.expandEnabled);
    setIntParamIfPresent(pSet, "printEnabled", d.printEnabled);
    setIntParamIfPresent(pSet, "colorHeadEnabled", d.colorHeadEnabled);
    setIntParamIfPresent(pSet, "filmGrainEnabled", d.filmGrainEnabled);
    setIntParamIfPresent(pSet, "halationEnabled", d.halationEnabled);
    setIntParamIfPresent(pSet, "bloomEnabled", d.bloomEnabled);
    setIntParamIfPresent(pSet, "filmDamageEnabled", d.filmDamageEnabled);
    setIntParamIfPresent(pSet, "filmBreathEnabled", d.filmBreathEnabled);
    setIntParamIfPresent(pSet, "gateWeaveEnabled", d.gateWeaveEnabled);
    setIntParamIfPresent(pSet, "overscanEnabled", d.overscanEnabled);
    setIntParamIfPresent(pSet, "vignetteEnabled", d.vignetteEnabled);

    const char* profileParams[] = {
        "filmGrainProfile", "halationProfile", "bloomProfile", "filmDamageProfile",
        "filmBreathProfile", "gateWeaveProfile"
    };
    for (int i = 0; i < 6; i++) setIntParamIfPresent(pSet, profileParams[i], PROFILE_AUTO);
    setIntParamIfPresent(pSet, "effectMode", EFFECT_FULL);
    setIntParamIfPresent(pSet, "printStock", d.printStock);
    setIntParamIfPresent(pSet, "filmGrainMode", GRAIN_MODE_ANALOG);
    setIntParamIfPresent(pSet, "overscanPreset", OVERSCAN_NONE);
    setIntParamIfPresent(pSet, "previewQuality", PREVIEW_FAST);

    setDoubleParamIfPresent(pSet, "expandAmount", d.expandAmount);
    setDoubleParamIfPresent(pSet, "pushPull", 0.0);
    setDoubleParamIfPresent(pSet, "acutance", 0.0);
    setDoubleParamIfPresent(pSet, "printAmount", d.printAmount);
    setDoubleParamIfPresent(pSet, "colorDensity", d.colorDensity);
    setDoubleParamIfPresent(pSet, "vibrance", d.vibrance);
    setDoubleParamIfPresent(pSet, "printerRed", 0.0);
    setDoubleParamIfPresent(pSet, "printerGreen", 0.0);
    setDoubleParamIfPresent(pSet, "printerBlue", 0.0);
    setDoubleParamIfPresent(pSet, "splitToneAmount", 0.0);
    setDoubleParamIfPresent(pSet, "splitToneWarmth", 0.55);
    setDoubleParamIfPresent(pSet, "colorHeadAmount", 0.0);
    setDoubleParamIfPresent(pSet, "filmGrainAmount", d.filmGrainAmount);
    setDoubleParamIfPresent(pSet, "filmGrainShadows", d.filmGrainShadows);
    setDoubleParamIfPresent(pSet, "filmGrainMids", d.filmGrainMids);
    setDoubleParamIfPresent(pSet, "filmGrainHighlights", d.filmGrainHighlights);
    setDoubleParamIfPresent(pSet, "filmGrainResolution", d.filmGrainResolution);
    setDoubleParamIfPresent(pSet, "filmGrainChroma", d.filmGrainChroma);
    setDoubleParamIfPresent(pSet, "halationAmount", d.halationAmount);
    setDoubleParamIfPresent(pSet, "halationThreshold", d.halationThreshold);
    setDoubleParamIfPresent(pSet, "halationRadius", d.halationRadius);
    setDoubleParamIfPresent(pSet, "halationWarmth", d.halationWarmth);
    setDoubleParamIfPresent(pSet, "halationLocal", d.halationLocal);
    setDoubleParamIfPresent(pSet, "halationGlobal", d.halationGlobal);
    setDoubleParamIfPresent(pSet, "halationHue", d.halationHue);
    setDoubleParamIfPresent(pSet, "halationBlueComp", d.halationBlueComp);
    setDoubleParamIfPresent(pSet, "halationImpact", d.halationImpact);
    setDoubleParamIfPresent(pSet, "halationDefringe", d.halationDefringe);
    setDoubleParamIfPresent(pSet, "bloomAmount", d.bloomAmount);
    setDoubleParamIfPresent(pSet, "bloomThreshold", d.bloomThreshold);
    setDoubleParamIfPresent(pSet, "bloomRegion", d.bloomRegion);
    setDoubleParamIfPresent(pSet, "bloomSoftness", d.bloomSoftness);
    setIntParamIfPresent(pSet, "bloomMaskPreview", BLOOM_MASK_OFF);
    setDoubleParamIfPresent(pSet, "bloomRadius", d.bloomRadius);
    setDoubleParamIfPresent(pSet, "bloomDetail", d.bloomDetail);
    setDoubleParamIfPresent(pSet, "bloomSaveLights", d.bloomSaveLights);
    setDoubleParamIfPresent(pSet, "bloomDefringe", d.bloomDefringe);
    setDoubleParamIfPresent(pSet, "filmDamageAmount", 0.0);
    setDoubleParamIfPresent(pSet, "filmBreathAmount", 0.0);
    setDoubleParamIfPresent(pSet, "gateWeaveAmount", 0.0);
    setDoubleParamIfPresent(pSet, "overscanAmount", 0.0);
    setDoubleParamIfPresent(pSet, "vignetteAmount", d.vignetteAmount);
}

static void applyPreset(OfxParamSetHandle pSet, int globalIdx) {
    if (globalIdx < 0) globalIdx = 0;
    if (globalIdx >= gNumPresets) globalIdx = gNumPresets - 1;
    const FilmPreset& p = gPresets[globalIdx];
    gParamSuite->paramEditBegin(pSet, "preset");
    const char* names[] = {"contrast","shoulder","toe","density","halation","grain","saturation","vignette","profileStrength","printStrength"};
    double vals[] = {p.contrast, p.shoulder, p.toe, p.density, p.halation, p.grain, p.saturation, p.vignette, 1.0, 0.65};
    for (int i = 0; i < 10; i++) {
        OfxParamHandle ph = nullptr;
        if (gParamSuite->paramGetHandle(pSet, names[i], &ph, nullptr) == kOfxStatOK)
            gParamSuite->paramSetValue(ph, vals[i]);
    }
    applyPresetCustomDefaults(pSet, globalIdx);
    gParamSuite->paramEditEnd(pSet);
}

// ============================================================
// PluginMain
// ============================================================
static OfxStatus pluginMain(const char* action, const void* handle,
                             OfxPropertySetHandle inArgs, OfxPropertySetHandle outArgs) {
    OfxImageEffectHandle effect = (OfxImageEffectHandle)handle;
    if (strcmp(action, kOfxActionDescribe) == 0) return describeAction(effect);
    if (strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) return describeInContextAction(effect, inArgs);
    if (strcmp(action, kOfxActionCreateInstance) == 0) return kOfxStatOK;
    if (strcmp(action, kOfxActionDestroyInstance) == 0) return kOfxStatOK;
    if (strcmp(action, kOfxImageEffectActionRender) == 0) return renderAction(effect, inArgs);
    if (strcmp(action, kOfxActionInstanceChanged) == 0) return instanceChangedAction(effect, inArgs, outArgs);
    if (strcmp(action, kOfxImageEffectActionIsIdentity) == 0) return kOfxStatReplyDefault;
    if (strcmp(action, kOfxImageEffectActionGetClipPreferences) == 0) return kOfxStatReplyDefault;
    if (strcmp(action, kOfxImageEffectActionBeginSequenceRender) == 0) return kOfxStatOK;
    if (strcmp(action, kOfxImageEffectActionEndSequenceRender) == 0) return kOfxStatOK;
    if (strcmp(action, kOfxActionPurgeCaches) == 0) return kOfxStatOK;
    if (strcmp(action, kOfxActionSyncPrivateData) == 0) return kOfxStatOK;
    return kOfxStatReplyDefault;
}

// ============================================================
// Describe
// ============================================================
static OfxStatus describeAction(OfxImageEffectHandle effect) {
    OfxPropertySetHandle props;
    gEffectSuite->getPropertySet(effect, &props);
    gPropSuite->propSetString(props, kOfxPropLabel, 0, PLUGIN_LABEL);
    gPropSuite->propSetString(props, kOfxImageEffectPluginPropGrouping, 0, "DuXun");
    gPropSuite->propSetString(props, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
    gPropSuite->propSetString(props, kOfxImageEffectPropSupportedContexts, 1, kOfxImageEffectContextGeneral);
    gPropSuite->propSetString(props, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat);
    gPropSuite->propSetString(props, kOfxImageEffectPropSupportedPixelDepths, 1, kOfxBitDepthShort);
    gPropSuite->propSetString(props, kOfxImageEffectPropSupportedPixelDepths, 2, kOfxBitDepthByte);
    gPropSuite->propSetString(props, kOfxImageEffectPluginRenderThreadSafety, 0, kOfxImageEffectRenderFullySafe);
    gPropSuite->propSetInt(props, kOfxImageEffectPluginPropHostFrameThreading, 0, 1);
    gPropSuite->propSetInt(props, kOfxImageEffectPropSupportsTiles, 0, 0);
    gPropSuite->propSetInt(props, kOfxImageEffectPropSupportsMultiResolution, 0, 1);
    describeGpuCapabilities(props);
    return kOfxStatOK;
}

// ============================================================
// DescribeInContext
// ============================================================
static OfxStatus describeInContextAction(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs) {
    OfxPropertySetHandle clipProps;

    // Output clip
    gEffectSuite->clipDefine(effect, kOfxImageEffectOutputClipName, &clipProps);
    gPropSuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gPropSuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentRGB);

    // Source clip
    gEffectSuite->clipDefine(effect, kOfxImageEffectSimpleSourceClipName, &clipProps);
    gPropSuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 0, kOfxImageComponentRGBA);
    gPropSuite->propSetString(clipProps, kOfxImageEffectPropSupportedComponents, 1, kOfxImageComponentRGB);
    gPropSuite->propSetInt(clipProps, kOfxImageEffectPropSupportsTiles, 0, 0);

    OfxParamSetHandle paramSet = nullptr;
    gEffectSuite->getParamSet(effect, &paramSet);
    if (!paramSet) return kOfxStatErrBadHandle;

    OfxPropertySetHandle h;

    // === GROUP: 胶片预设 ===
    gParamSuite->paramDefine(paramSet, kOfxParamTypeGroup, "grpPreset", &h);
    gPropSuite->propSetString(h, kOfxPropLabel, 0, "\xe8\x83\xb6\xe7\x89\x87\xe9\xa2\x84\xe8\xae\xbe");
    gPropSuite->propSetInt(h, kOfxParamPropGroupOpen, 0, 1);

    // Brand choice
    gParamSuite->paramDefine(paramSet, kOfxParamTypeChoice, "filmBrand", &h);
    gPropSuite->propSetString(h, kOfxPropLabel, 0, "\xe5\x93\x81\xe7\x89\x8c");
    gPropSuite->propSetString(h, kOfxParamPropHint, 0, "\xe9\x80\x89\xe6\x8b\xa9\xe5\x93\x81\xe7\x89\x8c\xe7\xad\x9b\xe9\x80\x89\xe8\x83\xb6\xe7\x89\x87");
    gPropSuite->propSetInt(h, kOfxParamPropDefault, 0, 4); // Kodak
    gPropSuite->propSetInt(h, kOfxParamPropAnimates, 0, 0);
    gPropSuite->propSetString(h, kOfxParamPropParent, 0, "grpPreset");
    markParamRealtime(h);
    gPropSuite->propSetString(h, kOfxParamPropChoiceOption, 0, "Agfa");
    gPropSuite->propSetString(h, kOfxParamPropChoiceOption, 1, "CineStill");
    gPropSuite->propSetString(h, kOfxParamPropChoiceOption, 2, "Fuji");
    gPropSuite->propSetString(h, kOfxParamPropChoiceOption, 3, "Ilford");
    gPropSuite->propSetString(h, kOfxParamPropChoiceOption, 4, "Kodak");
    gPropSuite->propSetString(h, kOfxParamPropChoiceOption, 5, "\xe8\x87\xaa\xe5\xae\x9a\xe4\xb9\x89");

    // Film choice - all 54 films
    gParamSuite->paramDefine(paramSet, kOfxParamTypeChoice, "filmPreset", &h);
    gPropSuite->propSetString(h, kOfxPropLabel, 0, "\xe8\x83\xb6\xe7\x89\x87\xe5\x9e\x8b\xe5\x8f\xb7");
    gPropSuite->propSetString(h, kOfxParamPropHint, 0, "\xe9\x80\x89\xe6\x8b\xa9\xe9\xa2\x84\xe8\xae\xbe\xe8\x83\xb6\xe7\x89\x87\xef\xbc\x8c\xe5\x8f\x82\xe6\x95\xb0\xe8\x87\xaa\xe5\x8a\xa8\xe8\xb0\x83\xe6\x95\xb4");
    gPropSuite->propSetInt(h, kOfxParamPropDefault, 0, 32); // first Kodak
    gPropSuite->propSetInt(h, kOfxParamPropAnimates, 0, 0);
    gPropSuite->propSetString(h, kOfxParamPropParent, 0, "grpPreset");
    markParamRealtime(h);
    for (int i = 0; i < gNumPresets; i++)
        gPropSuite->propSetString(h, kOfxParamPropChoiceOption, i, gPresets[i].name);

    // === GROUP: Color management ===
    gParamSuite->paramDefine(paramSet, kOfxParamTypeGroup, "grpColorMgmt", &h);
    gPropSuite->propSetString(h, kOfxPropLabel, 0, "Color Management");
    gPropSuite->propSetInt(h, kOfxParamPropGroupOpen, 0, 1);

    const char* transformChoices[] = {
        "Timeline / no conversion",
        "Rec.709 Gamma 2.4",
        "DaVinci Wide Gamut / Intermediate"
    };
    defineChoiceParam(paramSet, "inputTransform", "Input Transform",
        "Convert the node input into the film working space before the preset is applied.",
        TRANSFORM_TIMELINE, "grpColorMgmt", transformChoices, 3);
    defineChoiceParam(paramSet, "outputTransform", "Output Transform",
        "Keep this on Timeline to return the current Resolve timeline colour space and gamma.",
        TRANSFORM_TIMELINE, "grpColorMgmt", transformChoices, 3);

    // === GROUP: 曲线与对比度 ===
    gParamSuite->paramDefine(paramSet, kOfxParamTypeGroup, "grpCurve", &h);
    gPropSuite->propSetString(h, kOfxPropLabel, 0, "\xe6\x9b\xb2\xe7\xba\xbf\xe4\xb8\x8e\xe5\xaf\xb9\xe6\xaf\x94\xe5\xba\xa6");
    gPropSuite->propSetInt(h, kOfxParamPropGroupOpen, 0, 1);

    defineDoubleParam(paramSet, "contrast", "\xe5\xaf\xb9\xe6\xaf\x94\xe5\xba\xa6",
        "S-Curve\xe5\xaf\xb9\xe6\xaf\x94\xe5\xba\xa6\xe5\xbc\xba\xe5\xba\xa6", 1.15, 0.5, 2.0, 0.01, "grpCurve");
    defineDoubleParam(paramSet, "shoulder", "\xe9\xab\x98\xe5\x85\x89\xe5\x8e\x8b\xe7\xbc\xa9",
        "\xe9\xab\x98\xe5\x85\x89\xe5\x8c\xba\xe5\x9f\x9f\xe8\x82\xa9\xe9\x83\xa8\xe6\x9f\x94\xe5\x8c\x96", 0.35, 0.0, 1.0, 0.01, "grpCurve");
    defineDoubleParam(paramSet, "toe", "\xe6\x9a\x97\xe9\x83\xa8\xe5\x8e\x8b\xe7\xbc\xa9",
        "\xe6\x9a\x97\xe9\x83\xa8\xe5\x8c\xba\xe5\x9f\x9f\xe8\xb6\xbe\xe9\x83\xa8\xe6\x9f\x94\xe5\x8c\x96", 0.25, 0.0, 1.0, 0.01, "grpCurve");

    // === GROUP: 色彩与密度 ===
    gParamSuite->paramDefine(paramSet, kOfxParamTypeGroup, "grpColor", &h);
    gPropSuite->propSetString(h, kOfxPropLabel, 0, "\xe8\x89\xb2\xe5\xbd\xa9\xe4\xb8\x8e\xe5\xaf\x86\xe5\xba\xa6");
    gPropSuite->propSetInt(h, kOfxParamPropGroupOpen, 0, 1);

    defineDoubleParam(paramSet, "density", "\xe5\xaf\x86\xe5\xba\xa6",
        "\xe8\x83\xb6\xe7\x89\x87\xe5\xaf\x86\xe5\xba\xa6\xe8\xb0\x83\xe8\x8a\x82", 0.85, 0.0, 1.5, 0.01, "grpColor");
    defineDoubleParam(paramSet, "saturation", "\xe9\xa5\xb1\xe5\x92\x8c\xe5\xba\xa6",
        "\xe6\x95\xb4\xe4\xbd\x93\xe9\xa5\xb1\xe5\x92\x8c\xe5\xba\xa6\xef\xbc\x8c""1.0""\xe4\xb8\xba\xe5\x8e\x9f\xe5\xa7\x8b", 1.00, 0.0, 2.0, 0.01, "grpColor");
    defineDoubleParam(paramSet, "profileStrength", "胶片特性强度",
        "Film stock colour matrix strength.", 1.00, 0.0, 2.0, 0.01, "grpColor");
    defineDoubleParam(paramSet, "printStrength", "印片强度",
        "Blend amount for the film print curve and density response.", 0.65, 0.0, 1.5, 0.01, "grpColor");

    // === GROUP: 胶片效果 ===
    gParamSuite->paramDefine(paramSet, kOfxParamTypeGroup, "grpEffect", &h);
    gPropSuite->propSetString(h, kOfxPropLabel, 0, "\xe8\x83\xb6\xe7\x89\x87\xe6\x95\x88\xe6\x9e\x9c");
    gPropSuite->propSetInt(h, kOfxParamPropGroupOpen, 0, 1);
    gPropSuite->propSetInt(h, kOfxParamPropSecret, 0, 1);

    defineDoubleParam(paramSet, "halation", "\xe5\x85\x89\xe6\x99\x95",
        "\xe9\xab\x98\xe5\x85\x89\xe7\xba\xa2\xe8\x89\xb2\xe5\x85\x89\xe6\x99\x95\xe6\x95\xa3\xe5\xb0\x84", 0.30, 0.0, 1.0, 0.01, "grpEffect");
    defineDoubleParam(paramSet, "grain", "\xe9\xa2\x97\xe7\xb2\x92",
        "\xe8\x83\xb6\xe7\x89\x87\xe9\xa2\x97\xe7\xb2\x92\xe6\x84\x9f\xe5\xbc\xba\xe5\xba\xa6", 0.15, 0.0, 0.5, 0.01, "grpEffect");
    defineDoubleParam(paramSet, "vignette", "\xe6\xb8\x90\xe6\x99\x95",
        "\xe7\x94\xbb\xe9\x9d\xa2\xe8\xbe\xb9\xe7\xbc\x98\xe6\x9a\x97\xe8\xa7\x92", 0.00, 0.0, 1.0, 0.01, "grpEffect");
    setSecretIfPresent(paramSet, "halation");
    setSecretIfPresent(paramSet, "grain");
    setSecretIfPresent(paramSet, "vignette");

    defineLicenseParams(paramSet);
    defineCustomPlaceholderParams(paramSet);

    // === PAGE ===
    gParamSuite->paramDefine(paramSet, kOfxParamTypePage, "pgMain", &h);
    gPropSuite->propSetString(h, kOfxPropLabel, 0, "\xe7\x8b\xac\xe5\xaf\xbb\xe8\x83\xb6\xe7\x89\x87\xe6\xa8\xa1\xe6\x8b\x9f");
    gPropSuite->propSetString(h, kOfxParamPropPageChild, 0, "grpPreset");
    gPropSuite->propSetString(h, kOfxParamPropPageChild, 1, "grpColorMgmt");
    gPropSuite->propSetString(h, kOfxParamPropPageChild, 2, "grpCurve");
    gPropSuite->propSetString(h, kOfxParamPropPageChild, 3, "grpColor");
    gPropSuite->propSetString(h, kOfxParamPropPageChild, 4, "grpCustom");
    gPropSuite->propSetString(h, kOfxParamPropPageChild, 5, "grpLicense");

    return kOfxStatOK;
}

// ============================================================
// InstanceChanged - brand/film cascading + slider sync
// ============================================================
static OfxStatus instanceChangedAction(OfxImageEffectHandle effect,
                                        OfxPropertySetHandle inArgs,
                                        OfxPropertySetHandle outArgs) {
    char* reason = nullptr;
    gPropSuite->propGetString(inArgs, kOfxPropChangeReason, 0, &reason);
    if (!reason || strcmp(reason, kOfxChangeUserEdited) != 0) return kOfxStatReplyDefault;

    char* paramName = nullptr;
    gPropSuite->propGetString(inArgs, kOfxPropName, 0, &paramName);
    if (!paramName) return kOfxStatReplyDefault;

    OfxParamSetHandle pSet = nullptr;
    gEffectSuite->getParamSet(effect, &pSet);
    if (!pSet) return kOfxStatErrBadHandle;

    if (strcmp(paramName, "generateActivationRequest") == 0) {
        std::string requestPath = duxun_license::writeActivationRequest();
        setStringParamIfPresent(pSet, "licenseStatus", std::string("Activation request written: ") + requestPath);
        if (gMessageSuite) {
            gMessageSuite->setPersistentMessage(effect, kOfxMessageMessage, "DuXunLicense",
                                                "Activation request written: %s", requestPath.c_str());
        }
        return kOfxStatOK;
    }

    if (strcmp(paramName, "reloadLicense") == 0) {
        duxun_license::LocalLicenseState licenseState = duxun_license::reloadLocalLicense();
        updateLicenseStatusParam(pSet, licenseState);
        postLicenseMessage(effect, licenseState);
        return kOfxStatOK;
    }

    // --- Brand changed ---
    if (strcmp(paramName, "filmBrand") == 0) {
        OfxParamHandle brandH = nullptr;
        gParamSuite->paramGetHandle(pSet, "filmBrand", &brandH, nullptr);
        int brand = 0;
        gParamSuite->paramGetValue(brandH, &brand);

        if (brand == BRAND_CUSTOM) {
            // Set film to Custom
            OfxParamHandle filmH = nullptr;
            gParamSuite->paramGetHandle(pSet, "filmPreset", &filmH, nullptr);
            gParamSuite->paramSetValue(filmH, gNumPresets - 1);
            // Don't touch sliders - user controls
        } else {
            // Jump to first film of this brand
            OfxParamHandle filmH = nullptr;
            gParamSuite->paramGetHandle(pSet, "filmPreset", &filmH, nullptr);
            gParamSuite->paramSetValue(filmH, gBrandStart[brand]);
            // Apply preset
            applyPreset(pSet, gBrandStart[brand]);
        }
        return kOfxStatOK;
    }

    // --- Film changed ---
    if (strcmp(paramName, "filmPreset") == 0) {
        OfxParamHandle filmH = nullptr;
        gParamSuite->paramGetHandle(pSet, "filmPreset", &filmH, nullptr);
        int filmIdx = 0;
        gParamSuite->paramGetValue(filmH, &filmIdx);

        // Sync brand dropdown to match
        int brand = getBrandForFilm(filmIdx);
        OfxParamHandle brandH = nullptr;
        gParamSuite->paramGetHandle(pSet, "filmBrand", &brandH, nullptr);
        gParamSuite->paramSetValue(brandH, brand);

        // Apply preset (unless Custom)
        if (brand != BRAND_CUSTOM)
            applyPreset(pSet, filmIdx);

        return kOfxStatOK;
    }

    if (strcmp(paramName, "filmGrainProfile") == 0 ||
        strcmp(paramName, "halationProfile") == 0 ||
        strcmp(paramName, "bloomProfile") == 0 ||
        strcmp(paramName, "filmDamageProfile") == 0 ||
        strcmp(paramName, "filmBreathProfile") == 0 ||
        strcmp(paramName, "gateWeaveProfile") == 0 ||
        strcmp(paramName, "overscanPreset") == 0 ||
        strcmp(paramName, "printStock") == 0 ||
        strcmp(paramName, "filmGrainMode") == 0) {
        return syncCustomProfileSliders(pSet, paramName) ? kOfxStatOK : kOfxStatReplyDefault;
    }

    return kOfxStatReplyDefault;
}

// ============================================================
// Render - LUT-optimized
// ============================================================
static OfxStatus renderAction(OfxImageEffectHandle effect, OfxPropertySetHandle inArgs) {
    double renderTime = 0.0;
    gPropSuite->propGetDouble(inArgs, kOfxPropTime, 0, &renderTime);

    OfxRectI renderWin;
    gPropSuite->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, (int*)&renderWin);
    int width  = renderWin.x2 - renderWin.x1;
    int height = renderWin.y2 - renderWin.y1;
    if (width <= 0 || height <= 0) return kOfxStatOK;

    OfxParamSetHandle paramSet = nullptr;
    gEffectSuite->getParamSet(effect, &paramSet);
    if (!paramSet) return kOfxStatErrBadHandle;

    duxun_license::LocalLicenseState licenseState = duxun_license::evaluateLocalLicense();
    updateLicenseStatusParam(paramSet, licenseState);
    postLicenseMessage(effect, licenseState);

    // Read params
    double contrast = 1.15, shoulder = 0.35, toe = 0.25;
    double density = 0.85, halation = 0.3, grain = 0.15;
    double saturation = 1.0, vignette = 0.0;
    double profileStrength = 1.0, printStrength = 0.65;
    double expandAmount = 0.0, expandBlack = 0.35, expandWhite = 0.45;
    double printAmount = 0.0, printContrast = 0.35;
    double pushPull = 0.0, acutance = 0.0, colorDensity = 0.35, vibrance = 0.0;
    double printerRed = 0.0, printerGreen = 0.0, printerBlue = 0.0;
    double splitToneAmount = 0.0, splitToneWarmth = 0.55;
    double colorHeadAmount = 0.0, colorHeadCyan = 0.0, colorHeadMagenta = 0.0, colorHeadYellow = 0.0;
    double filmGrainAmount = 0.0, filmGrainSize = 0.35, filmGrainColor = 0.35;
    double filmGrainShadows = 0.55, filmGrainMids = 0.70, filmGrainHighlights = 0.35;
    double filmGrainResolution = 0.50, filmGrainChroma = 0.25;
    double halationAmount = 0.0, halationThreshold = 0.58, halationRadius = 0.55, halationWarmth = 0.80;
    double halationLocal = 0.60, halationGlobal = 0.30, halationHue = 0.35;
    double halationBlueComp = 0.50, halationImpact = 0.50, halationDefringe = 0.20;
    double bloomAmount = 0.0, bloomThreshold = 0.66, bloomRadius = 0.45;
    double bloomRegion = 0.42, bloomSoftness = 0.38;
    double bloomDetail = 0.35, bloomSaveLights = 0.45, bloomDefringe = 0.20;
    double filmDamageAmount = 0.0, filmDust = 0.45, filmScratch = 0.35;
    double filmBreathAmount = 0.0, filmBreathExposure = 0.45, filmBreathSaturation = 0.35;
    double gateWeaveAmount = 0.0, gateWeaveX = 0.50, gateWeaveY = 0.35;
    double overscanAmount = 0.0, vignetteAmount = 0.0, vignetteRadius = 0.70, vignetteFeather = 0.55;
    int inputTransform = TRANSFORM_TIMELINE;
    int outputTransform = TRANSFORM_TIMELINE;
    int filmIdx = 32;
    int expandEnabled = ENABLE_SKIP, printEnabled = ENABLE_SKIP, colorHeadEnabled = ENABLE_SKIP;
    int filmGrainEnabled = ENABLE_SKIP, halationEnabled = ENABLE_SKIP, bloomEnabled = ENABLE_SKIP;
    int filmDamageEnabled = ENABLE_SKIP, filmBreathEnabled = ENABLE_SKIP, gateWeaveEnabled = ENABLE_SKIP;
    int overscanEnabled = ENABLE_SKIP, vignetteEnabled = ENABLE_SKIP;
    int filmGrainProfile = PROFILE_AUTO, halationProfile = PROFILE_AUTO, bloomProfile = PROFILE_AUTO;
    int filmDamageProfile = PROFILE_AUTO, filmBreathProfile = PROFILE_AUTO, gateWeaveProfile = PROFILE_AUTO;
    int overscanPreset = OVERSCAN_NONE, previewQuality = PREVIEW_FAST;
    int printStock = PRINT_KODAK_2383, filmGrainMode = GRAIN_MODE_ANALOG, effectMode = EFFECT_FULL;
    int bloomMaskPreview = BLOOM_MASK_OFF;

    OfxParamHandle ph;
    OfxPropertySetHandle phProps;
    if (gParamSuite->paramGetHandle(paramSet, "filmPreset", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValue(ph, &filmIdx);
    if (gParamSuite->paramGetHandle(paramSet, "inputTransform", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValue(ph, &inputTransform);
    if (gParamSuite->paramGetHandle(paramSet, "outputTransform", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValue(ph, &outputTransform);
    if (gParamSuite->paramGetHandle(paramSet, "contrast", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &contrast);
    if (gParamSuite->paramGetHandle(paramSet, "shoulder", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &shoulder);
    if (gParamSuite->paramGetHandle(paramSet, "toe", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &toe);
    if (gParamSuite->paramGetHandle(paramSet, "density", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &density);
    if (gParamSuite->paramGetHandle(paramSet, "halation", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &halation);
    if (gParamSuite->paramGetHandle(paramSet, "grain", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &grain);
    if (gParamSuite->paramGetHandle(paramSet, "saturation", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &saturation);
    if (gParamSuite->paramGetHandle(paramSet, "vignette", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &vignette);
    if (gParamSuite->paramGetHandle(paramSet, "profileStrength", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &profileStrength);
    if (gParamSuite->paramGetHandle(paramSet, "printStrength", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &printStrength);
    if (gParamSuite->paramGetHandle(paramSet, "expandAmount", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &expandAmount);
    if (gParamSuite->paramGetHandle(paramSet, "expandBlack", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &expandBlack);
    if (gParamSuite->paramGetHandle(paramSet, "expandWhite", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &expandWhite);
    if (gParamSuite->paramGetHandle(paramSet, "printAmount", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &printAmount);
    if (gParamSuite->paramGetHandle(paramSet, "printContrast", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &printContrast);
    if (gParamSuite->paramGetHandle(paramSet, "pushPull", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &pushPull);
    if (gParamSuite->paramGetHandle(paramSet, "acutance", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &acutance);
    if (gParamSuite->paramGetHandle(paramSet, "colorDensity", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &colorDensity);
    if (gParamSuite->paramGetHandle(paramSet, "vibrance", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &vibrance);
    if (gParamSuite->paramGetHandle(paramSet, "printerRed", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &printerRed);
    if (gParamSuite->paramGetHandle(paramSet, "printerGreen", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &printerGreen);
    if (gParamSuite->paramGetHandle(paramSet, "printerBlue", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &printerBlue);
    if (gParamSuite->paramGetHandle(paramSet, "splitToneAmount", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &splitToneAmount);
    if (gParamSuite->paramGetHandle(paramSet, "splitToneWarmth", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &splitToneWarmth);
    if (gParamSuite->paramGetHandle(paramSet, "colorHeadAmount", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &colorHeadAmount);
    if (gParamSuite->paramGetHandle(paramSet, "colorHeadCyan", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &colorHeadCyan);
    if (gParamSuite->paramGetHandle(paramSet, "colorHeadMagenta", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &colorHeadMagenta);
    if (gParamSuite->paramGetHandle(paramSet, "colorHeadYellow", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &colorHeadYellow);
    if (gParamSuite->paramGetHandle(paramSet, "filmGrainAmount", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &filmGrainAmount);
    if (gParamSuite->paramGetHandle(paramSet, "filmGrainSize", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &filmGrainSize);
    if (gParamSuite->paramGetHandle(paramSet, "filmGrainColor", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &filmGrainColor);
    if (gParamSuite->paramGetHandle(paramSet, "filmGrainShadows", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &filmGrainShadows);
    if (gParamSuite->paramGetHandle(paramSet, "filmGrainMids", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &filmGrainMids);
    if (gParamSuite->paramGetHandle(paramSet, "filmGrainHighlights", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &filmGrainHighlights);
    if (gParamSuite->paramGetHandle(paramSet, "filmGrainResolution", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &filmGrainResolution);
    if (gParamSuite->paramGetHandle(paramSet, "filmGrainChroma", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &filmGrainChroma);
    if (gParamSuite->paramGetHandle(paramSet, "halationAmount", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &halationAmount);
    if (gParamSuite->paramGetHandle(paramSet, "halationThreshold", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &halationThreshold);
    if (gParamSuite->paramGetHandle(paramSet, "halationRadius", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &halationRadius);
    if (gParamSuite->paramGetHandle(paramSet, "halationWarmth", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &halationWarmth);
    if (gParamSuite->paramGetHandle(paramSet, "halationLocal", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &halationLocal);
    if (gParamSuite->paramGetHandle(paramSet, "halationGlobal", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &halationGlobal);
    if (gParamSuite->paramGetHandle(paramSet, "halationHue", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &halationHue);
    if (gParamSuite->paramGetHandle(paramSet, "halationBlueComp", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &halationBlueComp);
    if (gParamSuite->paramGetHandle(paramSet, "halationImpact", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &halationImpact);
    if (gParamSuite->paramGetHandle(paramSet, "halationDefringe", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &halationDefringe);
    if (gParamSuite->paramGetHandle(paramSet, "bloomAmount", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &bloomAmount);
    if (gParamSuite->paramGetHandle(paramSet, "bloomThreshold", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &bloomThreshold);
    if (gParamSuite->paramGetHandle(paramSet, "bloomRegion", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &bloomRegion);
    if (gParamSuite->paramGetHandle(paramSet, "bloomSoftness", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &bloomSoftness);
    if (gParamSuite->paramGetHandle(paramSet, "bloomRadius", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &bloomRadius);
    if (gParamSuite->paramGetHandle(paramSet, "bloomDetail", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &bloomDetail);
    if (gParamSuite->paramGetHandle(paramSet, "bloomSaveLights", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &bloomSaveLights);
    if (gParamSuite->paramGetHandle(paramSet, "bloomDefringe", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &bloomDefringe);
    if (gParamSuite->paramGetHandle(paramSet, "filmDamageAmount", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &filmDamageAmount);
    if (gParamSuite->paramGetHandle(paramSet, "filmDust", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &filmDust);
    if (gParamSuite->paramGetHandle(paramSet, "filmScratch", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &filmScratch);
    if (gParamSuite->paramGetHandle(paramSet, "filmBreathAmount", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &filmBreathAmount);
    if (gParamSuite->paramGetHandle(paramSet, "filmBreathExposure", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &filmBreathExposure);
    if (gParamSuite->paramGetHandle(paramSet, "filmBreathSaturation", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &filmBreathSaturation);
    if (gParamSuite->paramGetHandle(paramSet, "gateWeaveAmount", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &gateWeaveAmount);
    if (gParamSuite->paramGetHandle(paramSet, "gateWeaveX", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &gateWeaveX);
    if (gParamSuite->paramGetHandle(paramSet, "gateWeaveY", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &gateWeaveY);
    if (gParamSuite->paramGetHandle(paramSet, "overscanAmount", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &overscanAmount);
    if (gParamSuite->paramGetHandle(paramSet, "vignetteAmount", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &vignetteAmount);
    if (gParamSuite->paramGetHandle(paramSet, "vignetteRadius", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &vignetteRadius);
    if (gParamSuite->paramGetHandle(paramSet, "vignetteFeather", &ph, &phProps) == kOfxStatOK)
        gParamSuite->paramGetValueAtTime(ph, renderTime, &vignetteFeather);

    expandEnabled = getIntParamIfPresent(paramSet, "expandEnabled", ENABLE_SKIP);
    printEnabled = getIntParamIfPresent(paramSet, "printEnabled", ENABLE_SKIP);
    colorHeadEnabled = getIntParamIfPresent(paramSet, "colorHeadEnabled", ENABLE_SKIP);
    filmGrainEnabled = getIntParamIfPresent(paramSet, "filmGrainEnabled", ENABLE_SKIP);
    halationEnabled = getIntParamIfPresent(paramSet, "halationEnabled", ENABLE_SKIP);
    bloomEnabled = getIntParamIfPresent(paramSet, "bloomEnabled", ENABLE_SKIP);
    filmDamageEnabled = getIntParamIfPresent(paramSet, "filmDamageEnabled", ENABLE_SKIP);
    filmBreathEnabled = getIntParamIfPresent(paramSet, "filmBreathEnabled", ENABLE_SKIP);
    gateWeaveEnabled = getIntParamIfPresent(paramSet, "gateWeaveEnabled", ENABLE_SKIP);
    overscanEnabled = getIntParamIfPresent(paramSet, "overscanEnabled", ENABLE_SKIP);
    vignetteEnabled = getIntParamIfPresent(paramSet, "vignetteEnabled", ENABLE_SKIP);
    effectMode = getIntParamIfPresent(paramSet, "effectMode", EFFECT_FULL);
    printStock = getIntParamIfPresent(paramSet, "printStock", PRINT_KODAK_2383);
    filmGrainMode = getIntParamIfPresent(paramSet, "filmGrainMode", GRAIN_MODE_ANALOG);
    filmGrainProfile = getIntParamIfPresent(paramSet, "filmGrainProfile", PROFILE_AUTO);
    halationProfile = getIntParamIfPresent(paramSet, "halationProfile", PROFILE_AUTO);
    bloomProfile = getIntParamIfPresent(paramSet, "bloomProfile", PROFILE_AUTO);
    bloomMaskPreview = getIntParamIfPresent(paramSet, "bloomMaskPreview", BLOOM_MASK_OFF);
    filmDamageProfile = getIntParamIfPresent(paramSet, "filmDamageProfile", PROFILE_AUTO);
    filmBreathProfile = getIntParamIfPresent(paramSet, "filmBreathProfile", PROFILE_AUTO);
    gateWeaveProfile = getIntParamIfPresent(paramSet, "gateWeaveProfile", PROFILE_AUTO);
    overscanPreset = getIntParamIfPresent(paramSet, "overscanPreset", OVERSCAN_NONE);
    previewQuality = getIntParamIfPresent(paramSet, "previewQuality", PREVIEW_FAST);

    const bool colorModeEnabled = (effectMode != EFFECT_TEXTURE_ONLY && effectMode != EFFECT_GRAIN_HALATION);
    const bool textureModeEnabled = (effectMode != EFFECT_COLOR_ONLY);
    const bool grainHalationOnly = (effectMode == EFFECT_GRAIN_HALATION);
    const bool useExpand = (expandEnabled == 1) && colorModeEnabled;
    const bool usePrint = (printEnabled == 1) && colorModeEnabled;
    const bool useColorHead = (colorHeadEnabled == 1) && colorModeEnabled;
    const bool useFilmGrain = (filmGrainEnabled == 1) && textureModeEnabled;
    const bool useHalation = (halationEnabled == 1) && textureModeEnabled;
    const bool useBloom = (bloomEnabled == 1) && textureModeEnabled && !grainHalationOnly;
    const bool useFilmDamage = (filmDamageEnabled == 1) && textureModeEnabled && !grainHalationOnly;
    const bool useFilmBreath = (filmBreathEnabled == 1) && textureModeEnabled && !grainHalationOnly;
    const bool useGateWeave = (gateWeaveEnabled == 1) && textureModeEnabled && !grainHalationOnly;
    const bool useOverscan = (overscanEnabled == 1) && textureModeEnabled && !grainHalationOnly;
    const bool useVignette = (vignetteEnabled == 1) && !grainHalationOnly;
    const int effectiveInputTransform = inputTransform;
    const int effectiveOutputTransform = outputTransform;

    GrainProfile grainProfile = stockGrainProfileForPreset(filmIdx, filmGrainProfile);
    float halationRadiusGain = 1.0f, halationWarmthGain = 1.0f;
    float halationGain = halationGainForProfile(filmIdx, halationProfile, halationRadiusGain, halationWarmthGain);
    float bloomRadiusGain = 1.0f;
    float bloomGain = bloomGainForProfile(filmIdx, bloomProfile, bloomRadiusGain);
    float damageDustGain = 1.0f, damageScratchGain = 1.0f;
    float damageGain = damageGainForProfile(filmIdx, filmDamageProfile, damageDustGain, damageScratchGain);
    float breathExposureGain = 1.0f, breathSaturationGain = 1.0f;
    float breathGain = breathGainForProfile(filmIdx, filmBreathProfile, breathExposureGain, breathSaturationGain);
    float weaveXGain = 1.0f, weaveYGain = 1.0f;
    float weaveGain = weaveGainForProfile(filmIdx, gateWeaveProfile, weaveXGain, weaveYGain);

    float fD  = colorModeEnabled ? (float)density : 0.0f;
    float halationRenderBase = stockHalationRenderAmount(filmIdx, halationProfile, (float)halationAmount);
    float fH  = useHalation ? clampf(halationRenderBase * halationGain, 0.0f, 1.0f) : 0.0f;
    float fGr = useFilmGrain ? clampf((((float)grain * grainProfile.gain) + (float)filmGrainAmount * 0.62f * grainProfile.gain) * (1.0f + (float)pushPull * 0.10f), 0.0f, 2.2f) : 0.0f;
    float fSat = colorModeEnabled ? (float)saturation : 1.0f;
    float fV  = useVignette ? clampf((float)vignette + (float)vignetteAmount * 0.85f, 0.0f, 1.5f) : 0.0f;
    float fProfile = colorModeEnabled ? (float)profileStrength : 0.0f;
    float fPrintContrast = (float)printContrast;
    float fPrintCustom = usePrint ? clampf((float)printAmount, 0.0f, 2.0f) : 0.0f;
    float fPrint = colorModeEnabled ? ((float)printStrength + fPrintCustom * (0.90f + 0.75f * fPrintContrast)) : 0.0f;
    float fExpand = useExpand ? (float)expandAmount : 0.0f;
    float fExpandBlack = (float)expandBlack;
    float fExpandWhite = (float)expandWhite;
    float fPushPull = colorModeEnabled ? (float)pushPull : 0.0f;
    float fAcutance = textureModeEnabled && !grainHalationOnly ? (float)acutance : 0.0f;
    float fColorHead = useColorHead ? (float)colorHeadAmount : 0.0f;
    float fColorHeadCyan = (float)colorHeadCyan;
    float fColorHeadMagenta = (float)colorHeadMagenta;
    float fColorHeadYellow = (float)colorHeadYellow;
    float fFilmGrainSize = clampf(grainProfile.size + ((float)filmGrainSize - 0.35f) * 0.80f, 0.0f, 1.0f);
    float fFilmGrainColor = clampf(lerpf(grainProfile.color, (float)filmGrainColor, 0.55f), 0.0f, 1.0f);
    float grainUserMix = filmGrainProfile == GRAIN_CUSTOM ? 1.0f : 0.45f;
    float fFilmGrainShadows = clampf(lerpf(grainProfile.shadows, (float)filmGrainShadows, grainUserMix), 0.0f, 1.0f);
    float fFilmGrainMids = clampf(lerpf(grainProfile.mids, (float)filmGrainMids, grainUserMix), 0.0f, 1.0f);
    float fFilmGrainHighlights = clampf(lerpf(grainProfile.highlights, (float)filmGrainHighlights, grainUserMix), 0.0f, 1.0f);
    float fFilmGrainResolution = clampf(lerpf(grainProfile.resolution, (float)filmGrainResolution, grainUserMix), 0.0f, 1.0f);
    float fFilmGrainChroma = clampf(lerpf(grainProfile.chroma, (float)filmGrainChroma, grainUserMix), 0.0f, 1.0f);
    float fFilmGrainClump = grainProfile.grainClump;
    float fFilmGrainSilverRetention = grainProfile.grainSilverRetention;
    float fFilmGrainTextureSeed = grainProfile.grainTextureSeed;
    float fHalationThreshold = (float)halationThreshold;
    float fHalationRadius = clampf((float)halationRadius * halationRadiusGain, 0.0f, 1.0f);
    float fHalationWarmth = clampf((float)halationWarmth * halationWarmthGain, 0.0f, 1.0f);
    float fHalationLocal = (float)halationLocal;
    float fHalationGlobal = (float)halationGlobal;
    float fHalationHue = (float)halationHue;
    float fHalationBlueComp = (float)halationBlueComp;
    float fHalationImpact = (float)halationImpact;
    float fHalationDefringe = (float)halationDefringe;
    if (presetNameHas(filmIdx, "CineStill 800T") && useHalation) {
        fH = fmaxf(fH, clampf(0.34f * halationGain, 0.0f, 1.0f));
        fHalationThreshold = fminf(fHalationThreshold, 0.74f);
        fHalationRadius = fmaxf(fHalationRadius, clampf(0.60f * halationRadiusGain, 0.0f, 1.0f));
        fHalationWarmth = fmaxf(fHalationWarmth, clampf(0.96f * halationWarmthGain, 0.0f, 1.0f));
        fHalationLocal = fmaxf(fHalationLocal, 0.72f);
        fHalationGlobal = fminf(fHalationGlobal, 0.025f);
        fHalationHue = fmaxf(fHalationHue, 0.72f);
        fHalationBlueComp = fmaxf(fHalationBlueComp, 0.86f);
        fHalationImpact = fmaxf(fHalationImpact, 0.58f);
        fHalationDefringe = fmaxf(fHalationDefringe, 0.42f);
    }
    float fBloom = useBloom ? clampf((float)bloomAmount * 0.42f * bloomGain, 0.0f, 1.0f) : 0.0f;
    float fBloomThreshold = (float)bloomThreshold;
    float fBloomRadius = clampf((float)bloomRadius * bloomRadiusGain, 0.0f, 1.0f);
    float fBloomDetail = (float)bloomDetail;
    float fBloomSaveLights = (float)bloomSaveLights;
    float fBloomDefringe = (float)bloomDefringe;
    float fBloomRegion = (float)bloomRegion;
    float fBloomSoftness = (float)bloomSoftness;
    bool showBloomMask = bloomMaskPreview == BLOOM_MASK_SHOW;
    float fDamage = useFilmDamage ? (float)filmDamageAmount * damageGain : 0.0f;
    float fFilmDust = clampf((float)filmDust * damageDustGain, 0.0f, 1.0f);
    float fFilmScratch = clampf((float)filmScratch * damageScratchGain, 0.0f, 1.0f);
    float fBreath = useFilmBreath ? (float)filmBreathAmount * breathGain : 0.0f;
    float fFilmBreathExposure = clampf((float)filmBreathExposure * breathExposureGain, 0.0f, 1.0f);
    float fFilmBreathSaturation = clampf((float)filmBreathSaturation * breathSaturationGain, 0.0f, 1.0f);
    float fGateWeave = useGateWeave ? (float)gateWeaveAmount * weaveGain : 0.0f;
    float fGateWeaveX = clampf((float)gateWeaveX * weaveXGain, 0.0f, 1.0f);
    float fGateWeaveY = clampf((float)gateWeaveY * weaveYGain, 0.0f, 1.0f);
    float fOverscan = useOverscan ? (float)overscanAmount : 0.0f;
    float fVignetteRadius = (float)vignetteRadius;
    float fVignetteFeather = (float)vignetteFeather;
    float fColorDensity = usePrint ? ((float)colorDensity - 0.35f) * (0.45f + 0.35f * fPrintCustom) : 0.0f;
    float fVibrance = colorModeEnabled ? (float)vibrance : 0.0f;
    float fPrinterRed = usePrint ? (float)printerRed : 0.0f;
    float fPrinterGreen = usePrint ? (float)printerGreen : 0.0f;
    float fPrinterBlue = usePrint ? (float)printerBlue : 0.0f;
    float fSplitToneAmount = usePrint ? (float)splitToneAmount : 0.0f;
    float fSplitToneWarmth = (float)splitToneWarmth;
    const double* filmMatrix = matrixForPreset(filmIdx);

    // Build the print curve once per render. Output gamma is explicit now.
    float sCurveLUT[LUT_SIZE + 1];
    float curveStrength = clampf(fPrint, 0.0f, 1.5f);
    float curveContrast = 1.0f + (((float)contrast - 1.0f) + 0.32f * fPrintContrast * (float)printAmount) * curveStrength;
    float curveShoulder = (float)shoulder * curveStrength;
    float curveToe = (float)toe * curveStrength;
    buildSCurveLUT(sCurveLUT, curveContrast, curveShoulder, curveToe);

    // Get image buffers
    OfxImageClipHandle srcClip = nullptr, dstClip = nullptr;
    gEffectSuite->clipGetHandle(effect, kOfxImageEffectSimpleSourceClipName, &srcClip, nullptr);
    gEffectSuite->clipGetHandle(effect, kOfxImageEffectOutputClipName, &dstClip, nullptr);
    if (!srcClip || !dstClip) return kOfxStatErrBadHandle;

    OfxRectD rr;
    rr.x1 = (double)renderWin.x1; rr.y1 = (double)renderWin.y1;
    rr.x2 = (double)renderWin.x2; rr.y2 = (double)renderWin.y2;

    OfxPropertySetHandle srcImg = nullptr, dstImg = nullptr;
    OfxStatus stat;
    stat = gEffectSuite->clipGetImage(srcClip, renderTime, &rr, &srcImg);
    if (stat != kOfxStatOK || !srcImg) return stat;
    stat = gEffectSuite->clipGetImage(dstClip, renderTime, &rr, &dstImg);
    if (stat != kOfxStatOK || !dstImg) { gEffectSuite->clipReleaseImage(srcImg); return stat; }

    void* srcRaw = nullptr, *dstRaw = nullptr;
    int srcRB = 0, dstRB = 0;
    gPropSuite->propGetPointer(srcImg, kOfxImagePropData, 0, &srcRaw);
    gPropSuite->propGetPointer(dstImg, kOfxImagePropData, 0, &dstRaw);
    gPropSuite->propGetInt(srcImg, kOfxImagePropRowBytes, 0, &srcRB);
    gPropSuite->propGetInt(dstImg, kOfxImagePropRowBytes, 0, &dstRB);
    if (!srcRaw || !dstRaw) { gEffectSuite->clipReleaseImage(srcImg); gEffectSuite->clipReleaseImage(dstImg); return kOfxStatErrMemory; }

    float* __restrict src = (float*)srcRaw;
    float* __restrict dst = (float*)dstRaw;
    int sS = srcRB / 4, dS = dstRB / 4;
    float invW = 1.0f / (float)width, invH = 1.0f / (float)height;

    // Pre-compute flags
    const bool doDensity = (fD != 0.0f);
    const bool doSat     = (fSat != 1.0f);
    const bool doHalation = (fH > 0.01f);
    const bool doGrain   = (fGr > 0.001f);
    const bool doVignette = (fV > 0.001f);
    const bool doExpand = (fExpand > 0.001f);
    const bool doColorHead = (fColorHead > 0.001f);
    const bool doBloom = (fBloom > 0.001f || showBloomMask);
    const bool doDamage = (fDamage > 0.001f);
    const bool doBreath = (fBreath > 0.001f);
    const bool doSpatial = (fGateWeave > 0.001f || fOverscan > 0.001f);

    GlowBuffer halationGlow = { 0, 0, 0, nullptr };
    GlowBuffer bloomGlow = { 0, 0, 0, nullptr };
    bool haveHalationGlow = false;
    bool haveBloomGlow = false;

    // Pre-compute density factor
    float densityFactor = fD * 0.15f * (0.35f + 0.65f * curveStrength);

    // Pre-compute grain scale
    float grainScale = fGr * 0.4f * grainProfile.highlightWeight;

    // Pre-compute temporal offsets.
    float halThreshold = clampf(fHalationThreshold, 0.0f, 0.98f);
    float halScale = fH * 0.20f;
    float frameSeed = timeSeed(renderTime, 12.0f, 0.0f);
    float weaveX = (nativeEnhancerNoise(frameSeed, 8.0f, 71.0f) * 2.0f - 1.0f) * fGateWeave * fGateWeaveX * fmaxf(2.0f, (float)width * 0.006f);
    float weaveY = (nativeEnhancerNoise(frameSeed, 9.0f, 73.0f) * 2.0f - 1.0f) * fGateWeave * fGateWeaveY * fmaxf(1.0f, (float)height * 0.004f);
    float weaveRotation = (nativeEnhancerNoise(frameSeed, 10.0f, 79.0f) * 2.0f - 1.0f) * fGateWeave * 0.006f;
    float overscanScale = 1.0f + 0.08f * fOverscan;

    RenderThreadArgs args = {
        src, dst, sS, dS, width, height, effectiveInputTransform, effectiveOutputTransform,
        invW, invH,
        doDensity, doSat, doHalation, doGrain, doVignette, doExpand, doColorHead,
        doBloom, doDamage, doBreath, doSpatial, densityFactor, grainScale,
        halThreshold, halScale, overscanScale, weaveX, weaveY, weaveRotation,
        printStock, filmGrainMode, effectMode, fExpand, fExpandBlack, fExpandWhite,
        fPushPull, fAcutance, fBreath, fFilmBreathExposure,
        fFilmBreathSaturation, fPrintCustom, fPrintContrast, fColorDensity,
        fVibrance, fPrinterRed, fPrinterGreen, fPrinterBlue, fSplitToneAmount,
        fSplitToneWarmth, sCurveLUT,
        filmMatrix, fProfile, fColorHead, fColorHeadCyan, fColorHeadMagenta,
        fColorHeadYellow, fSat, &halationGlow, &bloomGlow, haveHalationGlow,
        haveBloomGlow, fH, fHalationWarmth, fBloom, fBloomRadius, fBloomDetail,
        fBloomSaveLights, fBloomDefringe, fBloomRegion, fBloomSoftness, showBloomMask,
        fGr, fFilmGrainSize, fFilmGrainColor,
        fFilmGrainShadows, fFilmGrainMids, fFilmGrainHighlights, fFilmGrainResolution,
        fFilmGrainChroma, fFilmGrainClump, fFilmGrainSilverRetention, fFilmGrainTextureSeed,
        fHalationLocal, fHalationGlobal, fHalationHue,
        fHalationBlueComp, fHalationImpact, fHalationDefringe,
        fV, fVignetteRadius, fVignetteFeather,
        fDamage, fFilmDust, fFilmScratch, renderTime, licenseState.watermarkRequired
    };

    GpuRenderRequest gpuRequest = readGpuRenderRequest(inArgs, srcImg, dstImg);
    logGpuEvent("request", gpuRequest, &args, "received");
    if (gpuRequest.requested && licenseState.watermarkRequired) {
        logGpuEvent("cuda_fallback", gpuRequest, &args, "license_watermark_requires_cpu", 0.0);
        gEffectSuite->clipReleaseImage(srcImg);
        gEffectSuite->clipReleaseImage(dstImg);
        return kOfxStatGPURenderFailed;
    }
    if (gpuRequest.requested && !gpuRequest.available) {
        logGpuEvent("cuda_fallback", gpuRequest, &args, "requested_unavailable", 0.0);
        gEffectSuite->clipReleaseImage(srcImg);
        gEffectSuite->clipReleaseImage(dstImg);
        return kOfxStatGPURenderFailed;
    }
    if (gpuRequest.requested) {
        const char* gpuFailureReason = "unknown";
        GpuRenderTiming gpuTiming = { -1.0, -1.0, false };
        const auto gpuStart = std::chrono::steady_clock::now();
        if (tryGpuRender(gpuRequest, args, curveContrast, curveShoulder, curveToe, &gpuFailureReason, &gpuTiming)) {
            double gpuElapsedMs = gpuElapsedMilliseconds(gpuStart);
            logGpuEvent("cuda_success", gpuRequest, &args, gpuFailureReason, gpuElapsedMs, &gpuTiming);
            freeGlowBuffer(halationGlow);
            freeGlowBuffer(bloomGlow);
            gEffectSuite->clipReleaseImage(srcImg);
            gEffectSuite->clipReleaseImage(dstImg);
            return kOfxStatOK;
        }
        double gpuElapsedMs = gpuElapsedMilliseconds(gpuStart);
        logGpuEvent("cuda_fallback", gpuRequest, &args, gpuFailureReason, gpuElapsedMs, &gpuTiming);
        gEffectSuite->clipReleaseImage(srcImg);
        gEffectSuite->clipReleaseImage(dstImg);
        return kOfxStatGPURenderFailed;
    }

    int glowScale = qualityGlowScale(previewQuality);
    if (doHalation) {
        haveHalationGlow = buildGlowBuffer(halationGlow, src, sS, width, height,
                                           fHalationThreshold, 5.0f + 38.0f * fHalationRadius,
                                           GLOW_HALATION, 0.0f, 0.0f,
                                           effectiveInputTransform,
                                           glowScale);
    }
    if (doBloom) {
        haveBloomGlow = buildGlowBuffer(bloomGlow, src, sS, width, height,
                                        fBloomThreshold, 5.0f + 46.0f * fBloomRadius,
                                        GLOW_BLOOM, fBloomRegion, fBloomSoftness,
                                        effectiveInputTransform,
                                        glowScale);
    }
    args.haveHalationGlow = haveHalationGlow;
    args.haveBloomGlow = haveBloomGlow;

    bool threaded = false;
    if (gThreadSuite) {
        unsigned int nThreads = 1;
        if (gThreadSuite->multiThreadNumCPUs(&nThreads) == kOfxStatOK && nThreads > 1) {
            unsigned int workThreads = (unsigned int)((width * height) / 32768);
            if (workThreads < 1) workThreads = 1;
            if (workThreads > nThreads) workThreads = nThreads;
            if (workThreads > (unsigned int)height) workThreads = (unsigned int)height;
            if (workThreads > 1 && gThreadSuite->multiThread(renderThreadFunction, workThreads, &args) == kOfxStatOK) {
                threaded = true;
            }
        }
    }
    if (!threaded) renderRows(args, 0, height);

    freeGlowBuffer(halationGlow);
    freeGlowBuffer(bloomGlow);
    gEffectSuite->clipReleaseImage(srcImg);
    gEffectSuite->clipReleaseImage(dstImg);
    return kOfxStatOK;
}
