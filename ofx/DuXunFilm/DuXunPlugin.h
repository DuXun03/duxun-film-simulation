/**
 * DuXunPlugin.h
 * ==============
 * 独寻胶片模拟 OFX Plugin for DaVinci Resolve
 *
 * OpenFX-compliant C++ plugin that appears in the Effects Library
 * as "独寻胶片模拟" under the "Color" category.
 *
 * Features:
 *   - 65 film stock presets (spectral data from Kodak/Fuji/Agfa/Ilford datasheets)
 *   - Full color pipeline: S-Curve → Matrix → Density → CMY
 *   - Texture effects: Halation, Bloom, Film Grain
 *   - Display transform: DWG/DI → Rec.709 / sRGB
 *
 * Build Requirements:
 *   - OpenFX SDK (https://github.com/AcademySoftwareFoundation/openfx)
 *   - CMake 3.20+
 *   - Visual Studio 2022 (Windows) or Xcode 15+ (macOS)
 *
 * License: MIT
 * Author: 独寻 (Lin Xinyu)
 */

#ifndef DUXUN_FILM_PLUGIN_H
#define DUXUN_FILM_PLUGIN_H

#include "ofxCore.h"
#include "ofxImageEffect.h"
#include "ofxMultiThread.h"
#include "ofxProperty.h"
#include "ofxsLog.h"
#include "ofxsParam.h"
#include "ofxsProperty.h"
#include "ofxsImageEffect.h"
#include "ofxsCore.h"
#include "ofxsThreadSuite.h"

#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

// Plugin identification
#define kPluginIdentifier    "com.duxun.filmsim"
#define kPluginVersionMajor  3
#define kPluginVersionMinor  0
#define kPluginName          "独寻胶片模拟"
#define kPluginGrouping      "独寻胶片模拟"
#define kPluginDescription   "专业电影胶片色彩模拟插件，支持 65 种经典胶片 Stock 预设，基于官方光谱数据手册"

// Parameter groups
#define kParamGroup_Curve    "对比度曲线"
#define kParamGroup_Matrix   "色彩矩阵"
#define kParamGroup_Density  "染料密度"
#define kParamGroup_CMY      "减色色彩头"
#define kParamGroup_Texture  "纹理效果"
#define kParamGroup_Display  "显示变换"

// Parameter names
#define kParam_Preset        "filmStock"
#define kParam_PresetLabel   "胶片预设"
#define kParam_PresetHint    "选择胶片 Stock 预设，自动配置所有参数"

#define kParam_Contrast      "contrast"
#define kParam_ContrastLabel "对比度"
#define kParam_Pivot         "pivot"
#define kParam_PivotLabel    "中灰基准"
#define kParam_Shoulder      "shoulder"
#define kParam_ShoulderLabel "高光压缩"
#define kParam_Toe           "toe"
#define kParam_ToeLabel      "暗部提升"

#define kParam_MatrixMix     "matrixMix"
#define kParam_MatrixMixLabel "矩阵强度"
#define kParam_LumKeep       "lumKeep"
#define kParam_LumKeepLabel  "亮度保持"

#define kParam_Density       "density"
#define kParam_DensityLabel  "染料浓度"
#define kParam_ChromaRoll    "chromaRoll"
#define kParam_ChromaRollLabel "高光饱和压缩"
#define kParam_ShadowSat     "shadowSat"
#define kParam_ShadowSatLabel "暗部饱和保持"

#define kParam_Warmth        "warmth"
#define kParam_WarmthLabel   "色温偏移"

#define kParam_Halation      "halation"
#define kParam_HalationLabel "光晕强度"
#define kParam_Grain         "grain"
#define kParam_GrainLabel    "颗粒强度"

#define kParam_DisplayGamma  "displayGamma"
#define kParam_DisplayGammaLabel "显示 Gamma"

// Number of film stock presets (auto-generated)
#ifndef DUXUN_FILM_STOCK_COUNT
#define DUXUN_FILM_STOCK_COUNT 65
#endif

/**
 * Film stock preset data.
 * Each preset contains: contrast, shoulder, toe, 3x3 matrix (9 floats)
 */
struct FilmPreset {
    const char* name;
    float contrast;
    float shoulder;
    float toe;
    float matrix[9];
};

// Forward declare preset lookup
extern const FilmPreset kFilmPresets[DUXUN_FILM_STOCK_COUNT];

/**
 * Plugin instance data (per-effect-instance state).
 */
struct DuXunInstanceData {
    // Current parameter values
    int   presetIndex;
    float contrast;
    float pivot;
    float shoulder;
    float toe;
    float matrixMix;
    float lumKeep;
    float density;
    float chromaRoll;
    float shadowSat;
    float warmth;
    float halation;
    float grain;
    float displayGamma;

    // Cached preset data
    float activeMatrix[9];
};

#endif // DUXUN_FILM_PLUGIN_H
