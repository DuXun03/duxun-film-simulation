/**
 * DuXunPlugin.cpp
 * ===============
 * 独寻胶片模拟 -- Main OFX Plugin Implementation
 *
 * Handles plugin lifecycle: describe, create instance, parameter changed.
 */

#include "DuXunPlugin.h"
#include "DuXunProcess.h"

// ============================================================
// Film Stock Presets (auto-generated from spectral-film-lut)
// ============================================================
#include "film_presets.h"

// ============================================================
// Plugin Entry Point
// ============================================================
OfxExport int OfxGetNumberOfPlugins(void) {
    return 1;
}

OfxExport OfxPlugin* OfxGetPlugin(int nth) {
    if (nth != 0) return nullptr;

    static OfxPlugin plugin = {
        kOfxImageEffectPluginApi,        // pluginApi
        1,                                // apiVersion
        kPluginIdentifier,                // pluginIdentifier
        kPluginVersionMajor,              // pluginVersionMajor
        kPluginVersionMinor,              // pluginVersionMinor
        nullptr,                          // setHost
        nullptr                           // mainEntry
    };
    return &plugin;
}

// ============================================================
// Plugin Description
// ============================================================
static OfxStatus describeAction(OfxImageEffectHandle effect) {
    // Basic plugin properties
    OfxPropertySetHandle props;
    gEffectHost->getPropertySet(effect, &props);

    gPropHost->propSetString(props, kOfxPropLabel, 0, kPluginName);
    gPropHost->propSetString(props, kOfxImageEffectPluginPropGrouping, 0, kPluginGrouping);
    gPropHost->propSetString(props, kOfxPropShortLabel, 0, "独寻胶片");
    gPropHost->propSetString(props, kOfxPropLongLabel, 0, "独寻胶片模拟 (DuXun Film Simulation)");

    // Plugin type
    gPropHost->propSetString(props, kOfxImageEffectPropSupportedContexts, 0, kOfxImageEffectContextFilter);
    gPropHost->propSetString(props, kOfxImageEffectPropSupportedContexts, 1, kOfxImageEffectContextGeneral);

    // Pixel support
    gPropHost->propSetString(props, kOfxImageEffectPropSupportedPixelDepths, 0, kOfxBitDepthFloat);
    gPropHost->propSetInt(props, kOfxImageEffectPropSupportsTiles, 0, 1);
    gPropHost->propSetInt(props, kOfxImageEffectPropSupportsMultiResolution, 0, 1);
    gPropHost->propSetInt(props, kOfxImageEffectPropSupportsMultipleClipDepths, 0, 1);

    // Temporal access (for grain frame variation)
    gPropHost->propSetInt(props, kOfxImageEffectPropTemporalClipAccess, 0, 1);

    // Render quality
    gPropHost->propSetDouble(props, kOfxImageEffectPropPreMultiplication, 0, 1.0);

    // ========================================================
    // Define Parameters
    // ========================================================

    // -- Film Stock Preset Dropdown --
    OfxParamHandle presetParam;
    gParamHost->paramDefine(effect, kOfxParamTypeChoice, kParam_Preset, &presetParam);
    gPropHost->propSetString(presetParam, kOfxPropLabel, 0, kParam_PresetLabel);
    gPropHost->propSetString(presetParam, kOfxPropHint, 0, kParam_PresetHint);
    gPropHost->propSetInt(presetParam, kOfxParamPropDefault, 0, 0);
    // Set preset names from DUXUN_FILM_STOCK_NAMES
    gPropHost->propSetString(presetParam, kOfxParamPropChoiceOption, 0, DUXUN_FILM_STOCK_NAMES);

    // -- Contrast Curve Group --
    auto addSlider = [&](const char* name, const char* label, double def, double min, double max) -> OfxParamHandle {
        OfxParamHandle p;
        gParamHost->paramDefine(effect, kOfxParamTypeDouble, name, &p);
        gPropHost->propSetString(p, kOfxPropLabel, 0, label);
        gPropHost->propSetDouble(p, kOfxParamPropDefault, 0, def);
        gPropHost->propSetDouble(p, kOfxParamPropMin, 0, min);
        gPropHost->propSetDouble(p, kOfxParamPropMax, 0, max);
        gPropHost->propSetDouble(p, kOfxParamPropIncrement, 0, 0.01);
        gPropHost->propSetString(p, kOfxParamPropPage, 0, kParamGroup_Curve);
        return p;
    };

    addSlider(kParam_Contrast, kParam_ContrastLabel, 1.15, 0.5, 2.0);
    addSlider(kParam_Pivot,    kParam_PivotLabel,    0.18, 0.1, 0.25);
    addSlider(kParam_Shoulder, kParam_ShoulderLabel, 0.35, 0.0, 1.0);
    addSlider(kParam_Toe,      kParam_ToeLabel,      0.25, 0.0, 1.0);

    // -- Color Matrix Group --
    auto addMatrixSlider = [&](const char* name, const char* label, double def, double min, double max) -> OfxParamHandle {
        OfxParamHandle p;
        gParamHost->paramDefine(effect, kOfxParamTypeDouble, name, &p);
        gPropHost->propSetString(p, kOfxPropLabel, 0, label);
        gPropHost->propSetDouble(p, kOfxParamPropDefault, 0, def);
        gPropHost->propSetDouble(p, kOfxParamPropMin, 0, min);
        gPropHost->propSetDouble(p, kOfxParamPropMax, 0, max);
        gPropHost->propSetDouble(p, kOfxParamPropIncrement, 0, 0.01);
        gPropHost->propSetString(p, kOfxParamPropPage, 0, kParamGroup_Matrix);
        return p;
    };
    addMatrixSlider(kParam_MatrixMix, kParam_MatrixMixLabel, 0.75, 0.0, 1.0);
    addMatrixSlider(kParam_LumKeep,   kParam_LumKeepLabel,   0.5,  0.0, 1.0);

    // -- Density Group --
    auto addDensitySlider = [&](const char* name, const char* label, double def, double min, double max) -> OfxParamHandle {
        OfxParamHandle p;
        gParamHost->paramDefine(effect, kOfxParamTypeDouble, name, &p);
        gPropHost->propSetString(p, kOfxPropLabel, 0, label);
        gPropHost->propSetDouble(p, kOfxParamPropDefault, 0, def);
        gPropHost->propSetDouble(p, kOfxParamPropMin, 0, min);
        gPropHost->propSetDouble(p, kOfxParamPropMax, 0, max);
        gPropHost->propSetDouble(p, kOfxParamPropIncrement, 0, 0.01);
        gPropHost->propSetString(p, kOfxParamPropPage, 0, kParamGroup_Density);
        return p;
    };
    addDensitySlider(kParam_Density,    kParam_DensityLabel,    0.85, 0.0, 1.5);
    addDensitySlider(kParam_ChromaRoll, kParam_ChromaRollLabel, 0.6,  0.0, 1.0);
    addDensitySlider(kParam_ShadowSat,  kParam_ShadowSatLabel,  0.3,  0.0, 1.0);

    // -- CMY Group --
    auto addCMYSlider = [&](const char* name, const char* label, double def, double min, double max) -> OfxParamHandle {
        OfxParamHandle p;
        gParamHost->paramDefine(effect, kOfxParamTypeDouble, name, &p);
        gPropHost->propSetString(p, kOfxPropLabel, 0, label);
        gPropHost->propSetDouble(p, kOfxParamPropDefault, 0, def);
        gPropHost->propSetDouble(p, kOfxParamPropMin, 0, min);
        gPropHost->propSetDouble(p, kOfxParamPropMax, 0, max);
        gPropHost->propSetDouble(p, kOfxParamPropIncrement, 0, 0.01);
        gPropHost->propSetString(p, kOfxParamPropPage, 0, kParamGroup_CMY);
        return p;
    };
    addCMYSlider(kParam_Warmth, kParam_WarmthLabel, 0.05, -0.5, 0.5);

    // -- Texture Group --
    auto addTextureSlider = [&](const char* name, const char* label, double def, double min, double max) -> OfxParamHandle {
        OfxParamHandle p;
        gParamHost->paramDefine(effect, kOfxParamTypeDouble, name, &p);
        gPropHost->propSetString(p, kOfxPropLabel, 0, label);
        gPropHost->propSetDouble(p, kOfxParamPropDefault, 0, def);
        gPropHost->propSetDouble(p, kOfxParamPropMin, 0, min);
        gPropHost->propSetDouble(p, kOfxParamPropMax, 0, max);
        gPropHost->propSetDouble(p, kOfxParamPropIncrement, 0, 0.01);
        gPropHost->propSetString(p, kOfxParamPropPage, 0, kParamGroup_Texture);
        return p;
    };
    addTextureSlider(kParam_Halation, kParam_HalationLabel, 0.3,  0.0, 1.0);
    addTextureSlider(kParam_Grain,    kParam_GrainLabel,    0.15, 0.0, 0.5);

    // -- Display Group --
    OfxParamHandle gammaParam;
    gParamHost->paramDefine(effect, kOfxParamTypeDouble, kParam_DisplayGamma, &gammaParam);
    gPropHost->propSetString(gammaParam, kOfxPropLabel, 0, kParam_DisplayGammaLabel);
    gPropHost->propSetDouble(gammaParam, kOfxParamPropDefault, 0, 2.4);
    gPropHost->propSetDouble(gammaParam, kOfxParamPropMin, 0, 1.8);
    gPropHost->propSetDouble(gammaParam, kOfxParamPropMax, 0, 2.6);
    gPropHost->propSetString(gammaParam, kOfxParamPropPage, 0, kParamGroup_Display);

    return kOfxStatOK;
}

// ============================================================
// Instance Creation
// ============================================================
static OfxStatus createInstanceAction(OfxImageEffectHandle effect) {
    auto* data = new DuXunInstanceData();
    memset(data, 0, sizeof(DuXunInstanceData));

    // Set defaults
    data->contrast     = 1.15f;
    data->pivot        = 0.18f;
    data->shoulder     = 0.35f;
    data->toe          = 0.25f;
    data->matrixMix    = 0.75f;
    data->lumKeep      = 0.5f;
    data->density      = 0.85f;
    data->chromaRoll   = 0.6f;
    data->shadowSat    = 0.3f;
    data->warmth       = 0.05f;
    data->halation     = 0.3f;
    data->grain        = 0.15f;
    data->displayGamma = 2.4f;
    data->presetIndex  = 0;

    // Load default preset matrix
    if (DUXUN_FILM_STOCK_COUNT > 0) {
        for (int i = 0; i < 9; i++) {
            data->activeMatrix[i] = DUXUN_FILM_PRESETS[0][3 + i];
        }
    }

    gPropHost->propSetPointer(
        gEffectHost->getPropertySet(effect),
        kOfxPropInstanceData, 0, (void*)data
    );

    return kOfxStatOK;
}

// ============================================================
// Instance Destruction
// ============================================================
static OfxStatus destroyInstanceAction(OfxImageEffectHandle effect) {
    DuXunInstanceData* data = nullptr;
    gPropHost->propGetPointer(
        gEffectHost->getPropertySet(effect),
        kOfxPropInstanceData, 0, (void**)&data
    );
    delete data;
    return kOfxStatOK;
}

// ============================================================
// Parameter Changed
// ============================================================
static OfxStatus paramChangedAction(OfxImageEffectHandle effect,
                                     OfxParamHandle param,
                                     const char* paramName) {
    DuXunInstanceData* data = nullptr;
    gPropHost->propGetPointer(
        gEffectHost->getPropertySet(effect),
        kOfxPropInstanceData, 0, (void**)&data
    );

    // Handle preset selection: load preset parameters
    if (strcmp(paramName, kParam_Preset) == 0) {
        int idx = 0;
        gPropHost->propGetInt(param, kOfxParamPropValue, 0, &idx);
        data->presetIndex = idx;

        if (idx >= 0 && idx < DUXUN_FILM_STOCK_COUNT) {
            // Load preset values
            data->contrast = DUXUN_FILM_PRESETS[idx][0];
            data->shoulder = DUXUN_FILM_PRESETS[idx][1];
            data->toe      = DUXUN_FILM_PRESETS[idx][2];

            for (int i = 0; i < 9; i++) {
                data->activeMatrix[i] = DUXUN_FILM_PRESETS[idx][3 + i];
            }

            // Update UI sliders to reflect preset values
            auto updateSlider = [&](const char* name, float val) {
                OfxParamHandle p;
                gParamHost->paramGetHandle(effect, name, &p);
                gPropHost->propSetDouble(p, kOfxParamPropValue, 0, (double)val);
            };
            updateSlider(kParam_Contrast, data->contrast);
            updateSlider(kParam_Shoulder, data->shoulder);
            updateSlider(kParam_Toe, data->toe);
        }
        return kOfxStatOK;
    }

    // Individual parameter changes
    auto readSlider = [&](const char* name, float* dst) {
        if (strcmp(paramName, name) == 0) {
            double val = 0.0;
            gPropHost->propGetDouble(param, kOfxParamPropValue, 0, &val);
            *dst = (float)val;
            return true;
        }
        return false;
    };

    readSlider(kParam_Contrast,    &data->contrast);
    readSlider(kParam_Pivot,       &data->pivot);
    readSlider(kParam_Shoulder,    &data->shoulder);
    readSlider(kParam_Toe,         &data->toe);
    readSlider(kParam_MatrixMix,   &data->matrixMix);
    readSlider(kParam_LumKeep,     &data->lumKeep);
    readSlider(kParam_Density,     &data->density);
    readSlider(kParam_ChromaRoll,  &data->chromaRoll);
    readSlider(kParam_ShadowSat,   &data->shadowSat);
    readSlider(kParam_Warmth,      &data->warmth);
    readSlider(kParam_Halation,    &data->halation);
    readSlider(kParam_Grain,       &data->grain);
    readSlider(kParam_DisplayGamma,&data->displayGamma);

    return kOfxStatOK;
}

// ============================================================
// Render Action
// ============================================================
static OfxStatus renderAction(OfxImageEffectHandle effect,
                               OfxPropertySetHandle inArgs,
                               OfxPropertySetHandle outArgs) {
    DuXunInstanceData* data = nullptr;
    gPropHost->propGetPointer(
        gEffectHost->getPropertySet(effect),
        kOfxPropInstanceData, 0, (void**)&data
    );

    // Get input clip
    OfxImageClipHandle srcClip;
    gEffectHost->clipGetHandle(effect, kOfxImageEffectSimpleSourceClipName, &srcClip, nullptr);

    // Get output clip
    OfxImageClipHandle dstClip;
    gEffectHost->clipGetHandle(effect, kOfxImageEffectOutputClipName, &dstClip, nullptr);

    // Get render window
    OfxRectI renderWindow;
    gPropHost->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4, (int*)&renderWindow);

    // Get image data
    OfxPropertySetHandle srcImg, dstImg;
    gEffectHost->clipGetImage(srcClip, renderWindow.x2 - renderWindow.x1, nullptr, &srcImg);
    gEffectHost->clipGetImage(dstClip, renderWindow.x2 - renderWindow.x1, nullptr, &dstImg);

    void* srcPtr = nullptr;
    void* dstPtr = nullptr;
    int srcRowBytes = 0, dstRowBytes = 0;

    gPropHost->propGetPointer(srcImg, kOfxImagePropData, 0, &srcPtr);
    gPropHost->propGetPointer(dstImg, kOfxImagePropData, 0, &dstPtr);
    gPropHost->propGetInt(srcImg, kOfxImagePropRowBytes, 0, &srcRowBytes);
    gPropHost->propGetInt(dstImg, kOfxImagePropRowBytes, 0, &dstRowBytes);

    int width  = renderWindow.x2 - renderWindow.x1;
    int height = renderWindow.y2 - renderWindow.y1;

    // Process image (delegated to DuXunProcess.cpp)
    processImage(
        (const float*)srcPtr, (float*)dstPtr,
        width, height, srcRowBytes, dstRowBytes,
        data
    );

    gEffectHost->clipReleaseImage(srcImg);
    gEffectHost->clipReleaseImage(dstImg);

    return kOfxStatOK;
}

// ============================================================
// Plugin Action Dispatch
// ============================================================
static OfxStatus pluginMain(const char* action, const void* handle,
                             OfxPropertySetHandle inArgs,
                             OfxPropertySetHandle outArgs) {
    auto effect = (OfxImageEffectHandle)handle;

    if (strcmp(action, kOfxActionDescribe) == 0)
        return describeAction(effect);
    if (strcmp(action, kOfxActionCreateInstance) == 0)
        return createInstanceAction(effect);
    if (strcmp(action, kOfxActionDestroyInstance) == 0)
        return destroyInstanceAction(effect);
    if (strcmp(action, kOfxImageEffectActionRender) == 0)
        return renderAction(effect, inArgs, outArgs);
    if (strcmp(action, kOfxActionInstanceChanged) == 0) {
        const char* paramName = nullptr;
        gPropHost->propGetString(inArgs, kOfxPropChangedParam, 0, &paramName);
        OfxParamHandle param;
        gParamHost->paramGetHandle(effect, paramName, &param);
        return paramChangedAction(effect, param, paramName);
    }

    return kOfxStatReplyDefault;
}

// Set the main entry
OfxExport void OfxPluginSetHost(const OfxHost* host) {
    gHost = host;
}

namespace {
    struct Init {
        Init() {
            static OfxPlugin p = {
                kOfxImageEffectPluginApi, 1,
                kPluginIdentifier, kPluginVersionMajor, kPluginVersionMinor,
                nullptr, pluginMain
            };
            OfxPlugin* plugins[] = {&p};
            // Plugin registered during static init
        }
    };
    Init _init;
}
