from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read_text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


class OfxV6DesignTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = read_text("ofx/DuXunFilm/DuXunFilmSim.cpp")

    def test_double_params_are_slider_friendly_and_realtime(self):
        self.assertIn("kOfxParamPropDisplayMin", self.source)
        self.assertIn("kOfxParamPropDisplayMax", self.source)
        self.assertIn("kOfxParamPropDigits", self.source)
        self.assertIn("kOfxParamPropEvaluateOnChange", self.source)
        self.assertIn("kOfxParamInvalidateValueChange", self.source)

    def test_color_management_controls_are_explicit(self):
        for param in ["inputTransform", "outputTransform", "profileStrength", "printStrength"]:
            self.assertIn(param, self.source)
        self.assertIn("Timeline / no conversion", self.source)
        self.assertIn("Rec.709 Gamma 2.4", self.source)
        self.assertIn("DaVinci Wide Gamut / Intermediate", self.source)

    def test_custom_effect_sections_are_defined_in_chinese(self):
        for param in [
            "expandAmount",
            "printAmount",
            "colorHeadAmount",
            "filmGrainAmount",
            "halationAmount",
            "bloomAmount",
            "filmDamageAmount",
            "filmBreathAmount",
            "gateWeaveAmount",
            "overscanAmount",
            "vignetteAmount",
        ]:
            self.assertIn(param, self.source)
        for label in [
            "自定义",
            "动态范围",
            "成片反差",
            "色彩校正",
            "胶片颗粒",
            "光晕",
            "辉光",
            "胶片损伤",
            "胶片呼吸",
            "片门抖动",
            "边缘放大",
            "暗角",
        ]:
            self.assertIn(label, self.source)

    def test_compile_uses_utf8_execution_charset(self):
        for script in ["scripts/compile_all.bat", "build_plugin.bat"]:
            text = read_text(script)
            self.assertIn("/source-charset:utf-8", text)
            self.assertIn("/execution-charset:utf-8", text)

    def test_build_artifact_contains_utf8_custom_labels_when_present(self):
        artifact = ROOT / "build" / "DuXunFilmSim.ofx"
        if not artifact.exists():
            self.skipTest("Build artifact not present yet")
        data = artifact.read_bytes()
        for label in ["自定义", "强度", "动态范围", "成片反差", "色彩校正", "胶片损伤", "胶片呼吸", "片门抖动", "边缘放大", "暗角"]:
            self.assertIn(label.encode("utf-8"), data)
            self.assertNotIn(label.encode("gbk"), data)

    def test_render_uses_film_matrix_and_skips_default_double_gamma(self):
        self.assertRegex(self.source, re.compile(r"struct\s+FilmPreset\s*{[^}]*matrix", re.S))
        self.assertIn("applyFilmMatrix", self.source)
        self.assertIn("applyInputTransform", self.source)
        self.assertIn("applyOutputTransform", self.source)
        self.assertNotIn("double saturation = 1.0, vignette = 0.0, gamma = 2.4;", self.source)

    def test_custom_controls_are_wired_into_render(self):
        for param in [
            "expandAmount",
            "expandBlack",
            "expandWhite",
            "printAmount",
            "printContrast",
            "colorHeadAmount",
            "colorHeadCyan",
            "colorHeadMagenta",
            "colorHeadYellow",
            "filmGrainAmount",
            "filmGrainSize",
            "filmGrainColor",
            "halationAmount",
            "halationThreshold",
            "halationRadius",
            "halationWarmth",
            "bloomAmount",
            "bloomThreshold",
            "bloomRadius",
            "filmDamageAmount",
            "filmDust",
            "filmScratch",
            "filmBreathAmount",
            "filmBreathExposure",
            "filmBreathSaturation",
            "gateWeaveAmount",
            "gateWeaveX",
            "gateWeaveY",
            "overscanAmount",
            "vignetteAmount",
            "vignetteRadius",
            "vignetteFeather",
        ]:
            self.assertIn(param, self.source)
        for helper in [
            "applyExpand",
            "applyColorHead",
            "applyNativeEnhancerFilmGrain",
            "buildGlowBuffer",
            "sampleGlowBuffer",
            "applyFilmBreath",
            "applyFilmDamage",
        ]:
            self.assertIn(helper, self.source)
        self.assertIn("NativeEnhancer-FE", self.source)

    def test_render_uses_separate_glow_buffers_and_multi_slider_arguments(self):
        self.assertIn("buildGlowBuffer(halationGlow", self.source)
        self.assertIn("buildGlowBuffer(bloomGlow", self.source)
        self.assertIn("sampleGlowBuffer(*args.halationGlow", self.source)
        self.assertIn("sampleGlowBuffer(*args.bloomGlow", self.source)
        self.assertNotIn("sampleBrightBlur", self.source)
        self.assertIn("applyExpand(args.fExpand, args.fExpandBlack, args.fExpandWhite", self.source)
        self.assertIn("applyFilmBreath(args.fBreath, args.fFilmBreathExposure, args.fFilmBreathSaturation", self.source)
        self.assertIn("applyColorHead(args.fColorHead, args.fColorHeadCyan, args.fColorHeadMagenta, args.fColorHeadYellow", self.source)
        self.assertIn("applyProfiledFilmGrain(args.fGr, args.fFilmGrainSize, args.fFilmGrainColor", self.source)
        self.assertIn("applyFilmDamage(args.fDamage, args.fFilmDust, args.fFilmScratch", self.source)
        self.assertIn("kOfxImageEffectPropSupportsTiles, 0, 0", self.source)

    def test_custom_modules_have_real_effect_functions(self):
        for helper in [
            "applyPrintResponse",
            "computeGateWeaveSample",
            "applyCineStillHalation",
            "applyBloomDiffusion",
            "applyVignetteMask",
            "applyDustAndScratches",
        ]:
            self.assertIn(helper, self.source)
        for render_token in [
            "GlowBuffer halationGlow",
            "GlowBuffer bloomGlow",
            "weaveRotation",
            "applyPrintResponse(args.fPrintCustom",
            "computeGateWeaveSample(",
            "applyCineStillHalation(args.fH",
            "applyBloomDiffusion(args.fBloom",
            "applyVignetteMask(args.fV",
        ]:
            self.assertIn(render_token, self.source)

    def test_film_effect_group_is_hidden_and_custom_modules_have_enabled_controls(self):
        self.assertNotIn('kOfxParamPropPageChild, 4, "grpEffect"', self.source)
        self.assertIn('kOfxParamPropSecret', self.source)
        for param in [
            "expandEnabled",
            "printEnabled",
            "colorHeadEnabled",
            "filmGrainEnabled",
            "halationEnabled",
            "bloomEnabled",
            "filmDamageEnabled",
            "filmBreathEnabled",
            "gateWeaveEnabled",
            "overscanEnabled",
            "vignetteEnabled",
        ]:
            self.assertIn(param, self.source)
        for label in ["跳过", "启用"]:
            self.assertIn(label, self.source)
        self.assertIn("defineEnabledParam", self.source)
        self.assertNotIn('defineChoiceParam(ps, name, "Enabled"', self.source)
        self.assertNotIn('defineChoiceParam(ps, name, "Profile"', self.source)
        self.assertNotIn('"Profile Strength"', self.source)
        self.assertNotIn('"Print Strength"', self.source)

    def test_profile_system_v2_uses_module_specific_profiles(self):
        for helper in [
            "defineGrainProfileParam",
            "defineHalationProfileParam",
            "defineBloomProfileParam",
            "defineDamageProfileParam",
            "defineBreathProfileParam",
            "defineWeaveProfileParam",
        ]:
            self.assertIn(helper, self.source)
        for param in [
            "filmGrainProfile",
            "halationProfile",
            "bloomProfile",
            "filmDamageProfile",
            "filmBreathProfile",
            "gateWeaveProfile",
            "overscanPreset",
            "previewQuality",
        ]:
            self.assertIn(param, self.source)
        for param in [
            "expandProfile",
            "printProfile",
            "colorHeadProfile",
            "vignetteProfile",
        ]:
            self.assertNotIn(param, self.source)
        self.assertNotIn("defineStockProfileParam", self.source)
        self.assertEqual(0, len(re.findall(r'defineStockProfileParam\(ps, "', self.source)))
        self.assertIn('defineOverscanPresetParam(ps, "grpOverscan")', self.source)
        for label in [
            "65mm",
            "Super8",
            "CineStill",
            "Kodak 2383",
            "Fuji 3513",
            "35mm",
            "16mm",
            "8mm",
            "1.33 Academy",
            "1.85 Widescreen",
            "2.39 Scope",
            "快速预览",
        ]:
            self.assertIn(label, self.source)
        for helper in [
            "stockGrainProfileForPreset",
            "stockIsoForPreset",
            "overscanScaleForPreset",
            "qualityGlowScale",
            "halationGainForProfile",
            "bloomGainForProfile",
            "damageGainForProfile",
            "breathGainForProfile",
            "weaveGainForProfile",
        ]:
            self.assertIn(helper, self.source)

    def test_product_research_controls_are_wired_into_render(self):
        for param in [
            "filmGrainShadows",
            "filmGrainMids",
            "filmGrainHighlights",
            "filmGrainResolution",
            "filmGrainChroma",
            "filmGrainMode",
            "halationLocal",
            "halationGlobal",
            "halationHue",
            "halationBlueComp",
            "halationImpact",
            "halationDefringe",
            "bloomDetail",
            "bloomSaveLights",
            "bloomDefringe",
            "printStock",
            "pushPull",
            "acutance",
            "colorDensity",
            "vibrance",
            "printerRed",
            "printerGreen",
            "printerBlue",
            "splitToneAmount",
            "splitToneWarmth",
            "effectMode",
        ]:
            self.assertIn(param, self.source)
        for helper in [
            "applyPushPullResponse",
            "applyAcutance",
            "applyVibrance",
            "applyPrinterLights",
            "applySplitTone",
            "printStockChannelBias",
            "applyProfiledFilmGrain",
            "applyHalationControls",
            "applyBloomControls",
        ]:
            self.assertIn(helper, self.source)

    def test_preset_switch_overwrites_custom_defaults_instead_of_stacking(self):
        self.assertIn("setDoubleParamIfPresent", self.source)
        self.assertIn("setIntParamIfPresent", self.source)
        self.assertIn("applyPresetCustomDefaults(pSet, globalIdx);", self.source)
        for token in [
            'setDoubleParamIfPresent(pSet, "filmGrainAmount", d.filmGrainAmount)',
            'setDoubleParamIfPresent(pSet, "halationAmount", d.halationAmount)',
            'setDoubleParamIfPresent(pSet, "bloomAmount", d.bloomAmount)',
            'setDoubleParamIfPresent(pSet, "vignetteAmount", d.vignetteAmount)',
        ]:
            self.assertIn(token, self.source)

    def test_preset_switch_applies_visible_stock_defaults(self):
        self.assertIn("struct PresetCustomDefaults", self.source)
        self.assertIn("presetCustomDefaultsForStock(globalIdx)", self.source)
        self.assertNotIn("(void)globalIdx;", self.source)
        for token in [
            'setDoubleParamIfPresent(pSet, "filmGrainAmount", d.filmGrainAmount)',
            'setDoubleParamIfPresent(pSet, "halationAmount", d.halationAmount)',
            'setDoubleParamIfPresent(pSet, "bloomAmount", d.bloomAmount)',
            'setDoubleParamIfPresent(pSet, "printAmount", d.printAmount)',
            'setDoubleParamIfPresent(pSet, "vignetteAmount", d.vignetteAmount)',
        ]:
            self.assertIn(token, self.source)
        for stale_zero in [
            'setDoubleParamIfPresent(pSet, "filmGrainAmount", 0.0)',
            'setDoubleParamIfPresent(pSet, "halationAmount", 0.0)',
            'setDoubleParamIfPresent(pSet, "bloomAmount", 0.0)',
        ]:
            self.assertNotIn(stale_zero, self.source)

    def test_bloom_is_highlight_gated_and_not_overpowered(self):
        for token in [
            "bloomRegion",
            "bloomSoftness",
            "bloomMaskPreview",
            "GLOW_BLOOM",
            "bloomHighlightFloor",
            "showBloomMask",
        ]:
            self.assertIn(token, self.source)
        self.assertNotIn("float fBloom = useBloom ? clampf((float)bloomAmount * 1.35f * bloomGain", self.source)
        self.assertNotIn("float mix = clampf(a * 0.92f", self.source)
        self.assertRegex(self.source, re.compile(r"if\s*\(\s*glowKind\s*==\s*GLOW_BLOOM\s*\).*?key\s*=\s*lum", re.S))

    def test_stock_presets_do_not_enable_same_halation_and_bloom_for_every_film(self):
        for token in [
            "StockHalationClass",
            "HALATION_STOCK_NONE",
            "HALATION_STOCK_NO_REMJET",
            "stockHalationClassForPreset",
            "d.halationEnabled",
            "d.bloomEnabled",
            'setIntParamIfPresent(pSet, "halationEnabled", d.halationEnabled)',
            'setIntParamIfPresent(pSet, "bloomEnabled", d.bloomEnabled)',
            'presetNameHas(idx, "CineStill 800T")',
            'presetNameHas(idx, "CineStill 50D")',
        ]:
            self.assertIn(token, self.source)
        self.assertNotIn('for (int i = 0; i < 11; i++) setIntParamIfPresent(pSet, enabledParams[i], ENABLE_ON);', self.source)
        self.assertRegex(self.source, re.compile(r"HALATION_STOCK_NONE.*?halationAmount\s*=\s*0\.0", re.S))
        self.assertRegex(self.source, re.compile(r"CineStill 800T.*?halationEnabled\s*=\s*ENABLE_ON", re.S))

    def test_grain_uses_stock_specific_physical_profile(self):
        for token in [
            "grainTextureSeed",
            "grainClump",
            "grainSilverRetention",
            "physicalGrainProfileForPreset",
            "stockGrainDefaultsForPreset",
        ]:
            self.assertIn(token, self.source)
        for stale_label in ["快速颗粒", "黑白颗粒"]:
            self.assertNotIn(stale_label, self.source)

    def test_hdr_workflow_is_removed_from_surface_and_render_paths(self):
        support_match = re.search(r"static bool isCudaGpuPathSupported\(const RenderThreadArgs& args\).*?^}", self.source, re.S | re.M)
        self.assertIsNotNone(support_match)
        support_body = support_match.group(0)
        for token in [
            "TRANSFORM_REC2100_PQ",
            "TRANSFORM_REC2100_HLG",
            "Rec.2100 PQ",
            "Rec.2100 HLG",
            "hdrEnabled",
            "hdrReferenceWhite",
            "hdrHighlightRolloff",
            "hdrEffectScale",
            "pqToSceneLinear",
            "sceneLinearToPq",
            "hlgToSceneLinear",
            "sceneLinearToHlg",
            "applyHdrFilmWorkingMap",
            "hdrEffectCompensation",
            "isHdrTransform",
            "duxunCudaPqToSceneLinear",
            "duxunCudaSceneLinearToPq",
            "duxunCudaHlgToSceneLinear",
            "duxunCudaSceneLinearToHlg",
            "duxunCudaApplyHdrFilmWorkingMap",
            "duxunCudaApplyInputTransform(inputTransform, hdrReferenceWhite",
            "duxunCudaApplyOutputTransform(outputTransform, hdrReferenceWhite",
            "if (hdrWorkflowEnabled)",
            "(void*)&args.hdrReferenceWhite, (void*)&args.hdrHighlightRolloff",
        ]:
            self.assertNotIn(token, self.source)
        self.assertNotIn("isHdrTransform", support_body)

    def test_custom_profile_choices_sync_visible_sliders(self):
        for helper in [
            "applyFilmGrainProfileDefaults",
            "applyHalationProfileDefaults",
            "applyBloomProfileDefaults",
            "applyDamageProfileDefaults",
            "applyBreathProfileDefaults",
            "applyWeaveProfileDefaults",
            "applyOverscanPresetDefaults",
            "applyPrintStockDefaults",
            "applyFilmGrainModeDefaults",
            "syncCustomProfileSliders",
        ]:
            self.assertIn(helper, self.source)
        for param in [
            "filmGrainProfile",
            "halationProfile",
            "bloomProfile",
            "filmDamageProfile",
            "filmBreathProfile",
            "gateWeaveProfile",
            "overscanPreset",
            "printStock",
            "filmGrainMode",
        ]:
            self.assertRegex(
                self.source,
                re.compile(rf'strcmp\(paramName,\s*"{param}"\).*?syncCustomProfileSliders', re.S),
            )
        for slider in [
            '"filmGrainSize"',
            '"halationRadius"',
            '"bloomRadius"',
            '"filmDust"',
            '"filmBreathExposure"',
            '"gateWeaveX"',
            '"overscanAmount"',
            '"printContrast"',
        ]:
            self.assertIn(f"setDoubleParamIfPresent(pSet, {slider}", self.source)

    def test_manual_glow_profiles_have_nonzero_slider_fallbacks(self):
        for token in [
            "manualHalationBase",
            "manualBloomBase",
            "choice != PROFILE_AUTO",
            "HALATION_35MM_NO_REMJET",
            "BLOOM_35MM",
            "d.halationAmount <= 0.0001",
            "d.bloomAmount <= 0.0001",
        ]:
            self.assertIn(token, self.source)
        self.assertIn(
            'setDoubleParamIfPresent(pSet, "halationAmount", clampf((float)(manualHalationBase * gain)',
            self.source,
        )
        self.assertIn(
            'setDoubleParamIfPresent(pSet, "bloomAmount", clampf((float)(manualBloomBase * gain)',
            self.source,
        )

    def test_auto_glow_profiles_preserve_stock_bypass_defaults(self):
        self.assertIn("choice == PROFILE_AUTO", self.source)
        self.assertIn('setIntParamIfPresent(pSet, "halationEnabled", d.halationEnabled)', self.source)
        self.assertIn('setIntParamIfPresent(pSet, "bloomEnabled", d.bloomEnabled)', self.source)
        self.assertRegex(
            self.source,
            re.compile(
                r"if \(choice == PROFILE_AUTO\).*?"
                r'setIntParamIfPresent\(pSet, "halationEnabled", d\.halationEnabled\).*?'
                r'setDoubleParamIfPresent\(pSet, "halationAmount", d\.halationAmount\).*?'
                r"return;.*?double manualHalationBase",
                re.S,
            ),
        )
        self.assertRegex(
            self.source,
            re.compile(
                r"if \(choice == PROFILE_AUTO\).*?"
                r'setIntParamIfPresent\(pSet, "bloomEnabled", d\.bloomEnabled\).*?'
                r'setDoubleParamIfPresent\(pSet, "bloomAmount", d\.bloomAmount\).*?'
                r"return;.*?double manualBloomBase",
                re.S,
            ),
        )

    def test_halation_render_strength_uses_stock_specific_defaults_not_legacy_base(self):
        for token in [
            "stockHalationRenderAmount",
            "HALATION_STOCK_NO_REMJET",
            "HALATION_STOCK_SUBTLE",
        ]:
            self.assertIn(token, self.source)
        self.assertIn("stockHalationRenderAmount(filmIdx, halationProfile, (float)halationAmount)", self.source)
        self.assertNotIn("((float)halation * halationGain) + (float)halationAmount * 1.25f", self.source)

    def test_preset_custom_defaults_initializer_keeps_print_stock_aligned(self):
        self.assertRegex(
            self.source,
            re.compile(
                r"0\.0,\s*0\.82,\s*0\.55,\s*0\.34,\s*0\.34,\s*0\.48,\s*0\.24,\s*0\.24,\s*"
                r"0\.06 \+ \(double\)gPresets\[idx\]\.vignette \* 0\.40,\s*"
                r"bw \? PRINT_BW_BROMOPORTRAIT : PRINT_KODAK_2383",
                re.S,
            ),
        )

    def test_black_and_white_presets_have_stock_specific_defaults(self):
        for token in [
            "applyBlackAndWhiteStockDefaults",
            'presetNameHas(idx, "Tri-X")',
            'presetNameHas(idx, "T-Max 3200")',
            'presetNameHas(idx, "Acros")',
            'presetNameHas(idx, "APX 25")',
            'presetNameHas(idx, "HP5")',
            'presetNameHas(idx, "XP2")',
            "d.printStock = PRINT_BW_BROMOPORTRAIT",
            "d.filmGrainChroma = 0.0",
        ]:
            self.assertIn(token, self.source)
        self.assertRegex(
            self.source,
            re.compile(
                r"if \(bw\) \{\s*"
                r"applyBlackAndWhiteStockDefaults\(idx, d, speed\);\s*"
                r"\}",
                re.S,
            ),
        )

    def test_matrix_mapping_uses_resource_backed_near_neighbors(self):
        for token in [
            "kMatFujiC200",
            "kMatFujiNatura1600",
            "kMatKodakEktachrome100D",
            "case 11: return kMatFujiPro400H;",
            "case 18: return kMatFujiProvia100F;",
            "case 19: return kMatFujiVelvia50;",
            "case 14: return kMatFujiC200;",
            "case 23: return kMatFujiProvia100F;",
            "case 34: return kMatKodakGold200;",
        ]:
            self.assertIn(token, self.source)
        self.assertRegex(
            self.source,
            re.compile(r"case 13:\s*case 17: return kMatFujiNatura1600;", re.S),
        )
        self.assertRegex(
            self.source,
            re.compile(r"case 46:\s*case 47:\s*case 48:\s*case 49: return kMatKodakEktachrome100D;", re.S),
        )

    def test_high_value_fuji_kodak_stocks_have_tuned_defaults(self):
        for token in [
            "applyHighValueColorStockDefaults",
            'presetNameHas(idx, "Pro 800Z")',
            'presetNameHas(idx, "Astia")',
            'presetNameHas(idx, "Fortia")',
            'presetNameHas(idx, "Elite Chrome")',
            "d.filmGrainResolution = 0.94",
            "d.colorDensity = 0.50",
            "d.halationAmount = 0.025",
        ]:
            self.assertIn(token, self.source)
        self.assertRegex(
            self.source,
            re.compile(
                r"if \(!bw\) \{\s*"
                r"(?:applyFujiAgfaCineStillDefaults\(idx, d, speed\);\s*)?"
                r"(?:applyKodakColorNegativeDefaults\(idx, d, speed\);\s*)?"
                r"applyHighValueColorStockDefaults\(idx, d, speed\);\s*"
                r"\}",
                re.S,
            ),
        )

    def test_kodak_color_negative_presets_have_stock_specific_defaults(self):
        for token in [
            "applyKodakColorNegativeDefaults",
            'presetNameHas(idx, "Ektar 100")',
            'presetNameHas(idx, "Gold 200")',
            'presetNameHas(idx, "ColorPlus 200")',
            'presetNameHas(idx, "Ultra Max 400")',
            'presetNameHas(idx, "Portra 160")',
            'presetNameHas(idx, "Portra 400")',
            'presetNameHas(idx, "Portra 800")',
            "d.colorDensity = 0.46",
            "d.filmGrainResolution = 0.92",
            "d.halationAmount = 0.018",
            "d.filmGrainAmount = 0.26",
        ]:
            self.assertIn(token, self.source)
        self.assertRegex(
            self.source,
            re.compile(
                r"if \(!bw\) \{\s*"
                r"applyFujiAgfaCineStillDefaults\(idx, d, speed\);\s*"
                r"applyKodakColorNegativeDefaults\(idx, d, speed\);\s*"
                r"applyHighValueColorStockDefaults\(idx, d, speed\);\s*"
                r"\}",
                re.S,
            ),
        )

    def test_fuji_agfa_cinestill_presets_have_stock_specific_defaults(self):
        for token in [
            "applyFujiAgfaCineStillDefaults",
            'presetNameHas(idx, "Superia 1600")',
            'presetNameHas(idx, "Superia HG 1600")',
            'presetNameHas(idx, "Superia")',
            'presetNameHas(idx, "Agfa Vista")',
            'presetNameHas(idx, "CineStill 800T")',
            'presetNameHas(idx, "CineStill 50D")',
            "d.filmGrainAmount = 0.34",
            "d.filmGrainResolution = 0.54",
            "d.colorDensity = 0.37",
            "d.halationAmount = 0.22",
            "d.halationAmount = 0.08",
        ]:
            self.assertIn(token, self.source)
        self.assertRegex(
            self.source,
            re.compile(
                r"if \(!bw\) \{\s*"
                r"applyFujiAgfaCineStillDefaults\(idx, d, speed\);\s*"
                r"applyKodakColorNegativeDefaults\(idx, d, speed\);\s*"
                r"applyHighValueColorStockDefaults\(idx, d, speed\);\s*"
                r"\}",
                re.S,
            ),
        )

    def test_render_uses_enabled_flags_profiles_and_quality_scale(self):
        for token in [
            "filmGrainEnabled == 1",
            "halationEnabled == 1",
            "bloomEnabled == 1",
            "overscanEnabled == 1",
            "GrainProfile grainProfile",
            "qualityGlowScale(previewQuality)",
            "buildGlowBuffer(halationGlow",
            "glowScale",
        ]:
            self.assertIn(token, self.source)

    def test_openfx_gpu_acceleration_scaffold_is_guarded(self):
        for token in [
            '#include "ofxGPURender.h"',
            "DUXUN_ENABLE_OPENCL_RENDER",
            "DUXUN_ENABLE_CUDA_RENDER",
            "DUXUN_GPU_ACCELERATION_AVAILABLE",
            "describeGpuCapabilities",
            "GpuRenderRequest",
            "readGpuRenderRequest",
            "tryGpuRender",
        ]:
            self.assertIn(token, self.source)
        for token in [
            "kOfxImageEffectPropCPURenderSupported",
            "kOfxImageEffectPropOpenCLRenderSupported",
            "kOfxImageEffectPropOpenCLSupported",
            "kOfxImageEffectPropOpenCLEnabled",
            "kOfxImageEffectPropOpenCLCommandQueue",
            "kOfxImageEffectPropCudaRenderSupported",
            "kOfxImageEffectPropCudaStreamSupported",
            "kOfxImageEffectPropCudaEnabled",
            "kOfxImageEffectPropCudaStream",
            "kOfxStatGPURenderFailed",
        ]:
            self.assertIn(token, self.source)
        self.assertIn('propSetString(props, kOfxImageEffectPropCPURenderSupported, 0, "true")', self.source)
        self.assertIn("gpuRequest.requested && !gpuRequest.available", self.source)

    def test_render_pixel_loop_uses_ofx_multithreading(self):
        for token in [
            "RenderThreadArgs",
            "renderRows",
            "renderThreadFunction",
            "gThreadSuite->multiThreadNumCPUs",
            "gThreadSuite->multiThread(renderThreadFunction",
            "renderRows(args, 0, height)",
        ]:
            self.assertIn(token, self.source)

    def test_cuda_kernel_runtime_path_is_real_not_stubbed(self):
        for token in [
            "kCudaKernelSource",
            "duxunFilmSimCudaKernel",
            "nvrtcCreateProgram",
            "cuModuleLoadData",
            "cuLaunchKernel",
            "loadCudaRuntime",
            "isCudaGpuPathSupported",
            "tryGpuRender(gpuRequest, args, curveContrast, curveShoulder, curveToe, &gpuFailureReason, &gpuTiming)",
        ]:
            self.assertIn(token, self.source)
        self.assertNotIn("static bool tryGpuRender(const GpuRenderRequest& gpuRequest) {\n    if (gpuRequest.requested && !gpuRequest.available) return false;\n    (void)gpuRequest;\n    return false;\n}", self.source)

    def test_cuda_gpu_logging_and_p1_fallback_reasons_are_defined(self):
        for token in [
            "DUXUN_GPU_LOG_MAX_WRITES",
            "DuXunFilmSim-gpu.log",
            "logGpuEvent",
            "GpuRenderAttempt",
            "gpuFailureReason",
            "unsupported_effects",
            "cuda_success",
            "cuda_fallback",
            "launch_failed",
            "runtime_not_loaded",
            "duxunSampleBilinear",
            "fDamage",
            "fOverscanScale",
            "weaveRotation",
        ]:
            self.assertIn(token, self.source)

    def test_cuda_gpu_logging_records_timing_size_and_path_counters(self):
        for token in [
            "#include <chrono>",
            "classifyGpuRenderSize",
            "gpuElapsedMilliseconds",
            "elapsedMs=%.3f",
            "pixels=%lld",
            "sizeClass=%s",
            "requestIndex=%d",
            "successCount=%d",
            "fallbackCount=%d",
            "std::chrono::steady_clock::now()",
        ]:
            self.assertIn(token, self.source)

    def test_cuda_gpu_logging_splits_setup_and_kernel_timing(self):
        for token in [
            "struct GpuRenderTiming",
            "setupMs=%.3f",
            "kernelMs=%.3f",
            "coldStart=%d",
            "gCudaSuccessfulRenderCount",
            "const auto setupStart = std::chrono::steady_clock::now()",
            "const auto kernelStart = std::chrono::steady_clock::now()",
            "timing->setupMs",
            "timing->kernelMs",
        ]:
            self.assertIn(token, self.source)

    def test_cuda_attempt_happens_before_cpu_glow_preprocessing(self):
        gpu_request = self.source.find("GpuRenderRequest gpuRequest = readGpuRenderRequest")
        gpu_launch = self.source.find("tryGpuRender(gpuRequest, args, curveContrast, curveShoulder, curveToe, &gpuFailureReason, &gpuTiming)")
        halation_glow = self.source.find("buildGlowBuffer(halationGlow")
        bloom_glow = self.source.find("buildGlowBuffer(bloomGlow")
        self.assertNotEqual(gpu_request, -1)
        self.assertNotEqual(gpu_launch, -1)
        self.assertNotEqual(halation_glow, -1)
        self.assertNotEqual(bloom_glow, -1)
        self.assertLess(gpu_request, halation_glow)
        self.assertLess(gpu_launch, halation_glow)
        self.assertLess(gpu_launch, bloom_glow)

    def test_cuda_p1_declares_glow_multipass_and_does_not_reject_bloom(self):
        for token in [
            "tryCudaGlowRender",
            "launchCudaGlowPass",
            "cudaHighlightMaskKernel",
            "cudaDownsampleKernel",
            "cudaBlurHorizontalKernel",
            "cudaBlurVerticalKernel",
            "cudaCompositeGlowKernel",
            "glowDeviceBuffer",
            "cuMemAlloc",
            "cuMemFree",
        ]:
            self.assertIn(token, self.source)
        self.assertNotIn("return !args.doBloom && !isHdrTransform", self.source)
        support_match = re.search(r"static bool isCudaGpuPathSupported\(const RenderThreadArgs& args\).*?^}", self.source, re.S | re.M)
        self.assertIsNotNone(support_match)
        self.assertNotIn("args.doSpatial", support_match.group(0))

    def test_cuda_damage_uses_embedded_kernel_and_does_not_force_cpu_fallback(self):
        support_match = re.search(r"static bool isCudaGpuPathSupported\(const RenderThreadArgs& args\).*?^}", self.source, re.S | re.M)
        self.assertIsNotNone(support_match)
        support_body = support_match.group(0)
        self.assertNotIn("args.doDamage", support_body)
        self.assertIn("if (fDamage > 0.001f)", self.source)
        self.assertIn("float dust = duxunNativeNoise((float)x, (float)y + frame * 13.0f, 31.0f)", self.source)
        self.assertIn("float scratchColumn = duxunNativeNoise(floorf((float)x * 0.04f), frame, 57.0f)", self.source)
        self.assertIn("1.0f + 0.80f * a * scratchA", self.source)
        self.assertIn("1.0f + 0.80f * fDamage * fFilmScratch", self.source)
        self.assertIn("(void*)&args.fDamage, (void*)&args.fFilmDust, (void*)&args.fFilmScratch", self.source)

    def test_cuda_p1_keeps_bloom_and_halation_glow_buffers_separate(self):
        for token in [
            "halationGlowDeviceBuffer",
            "bloomGlowDeviceBuffer",
            "launchCudaGlowPass(gpuRequest, args, GLOW_HALATION",
            "launchCudaGlowPass(gpuRequest, args, GLOW_BLOOM",
            "const float* bloomGlow",
            "const float* halationGlow",
        ]:
            self.assertIn(token, self.source)
        self.assertNotIn("int glowKind = args.doBloom ? GLOW_BLOOM : GLOW_HALATION", self.source)
        self.assertRegex(
            self.source,
            re.compile(r"if\s*\(\s*args\.doHalation\s*\).*?GLOW_HALATION.*?if\s*\(\s*args\.doBloom\s*\).*?GLOW_BLOOM", re.S),
        )

    def test_cuda_p1_allocates_only_enabled_glow_buffers(self):
        glow_match = re.search(r"static bool tryCudaGlowRender\(.*?^}\n#endif", self.source, re.S | re.M)
        self.assertIsNotNone(glow_match)
        glow_body = glow_match.group(0)
        self.assertIn("PFN_cuMemAllocAsync", self.source)
        self.assertIn("PFN_cuMemFreeAsync", self.source)
        self.assertIn("cudaAllocDeviceBuffer(&glowMaskDeviceBuffer", glow_body)
        self.assertIn("allocResult = args.doHalation ? cudaAllocDeviceBuffer(&halationGlowDeviceBuffer", glow_body)
        self.assertIn("allocResult = args.doBloom ? cudaAllocDeviceBuffer(&bloomGlowDeviceBuffer", glow_body)
        self.assertIn("cudaFreeDeviceBuffer(halationGlowDeviceBuffer, gpuRequest.cudaStream)", glow_body)
        self.assertIn("cudaFreeDeviceBuffer(bloomGlowDeviceBuffer, gpuRequest.cudaStream)", glow_body)
        self.assertNotIn(
            "gCudaRuntime.cuMemAlloc(&halationGlowDeviceBuffer, glowDeviceBufferBytes) != 0 ||\n"
            "        gCudaRuntime.cuMemAlloc(&bloomGlowDeviceBuffer, glowDeviceBufferBytes) != 0",
            glow_body,
        )

    def test_cuda_binds_resolve_stream_context_before_allocating_glow_buffers(self):
        for token in [
            "PFN_cuStreamGetCtx",
            "PFN_cuCtxGetCurrent",
            "PFN_cuCtxSetCurrent",
            "PFN_cuPointerGetAttribute",
            "bindCudaContextForRequest(gpuRequest.cudaStream, args.src, args.dst, contextBinding, failureReason)",
            "gCudaRuntime.cuPointerGetAttribute(&streamCtx, 1, devicePtr)",
            "ensureCudaModuleLoaded(gCudaRuntime)",
        ]:
            self.assertIn(token, self.source)
        render_match = re.search(r"static bool tryGpuRender\(.*?^}\n\nstatic OfxStatus", self.source, re.S | re.M)
        self.assertIsNotNone(render_match)
        render_body = render_match.group(0)
        bind_idx = render_body.find("bindCudaContextForRequest(gpuRequest.cudaStream, args.src, args.dst, contextBinding, failureReason)")
        module_idx = render_body.find("ensureCudaModuleLoaded(gCudaRuntime)")
        launch_idx = render_body.find("gCudaRuntime.cuLaunchKernel")
        glow_idx = render_body.find("tryCudaGlowRender")
        self.assertNotEqual(bind_idx, -1)
        self.assertNotEqual(module_idx, -1)
        self.assertLess(bind_idx, module_idx)
        self.assertLess(module_idx, launch_idx)
        self.assertLess(bind_idx, launch_idx)
        self.assertLess(bind_idx, glow_idx)

    def test_cuda_spatial_uses_existing_overscan_and_gate_weave_kernel_path(self):
        self.assertIn("(void*)&args.overscanScale, (void*)&args.weaveX, (void*)&args.weaveY, (void*)&args.weaveRotation", self.source)
        self.assertIn("fabsf(fOverscanScale - 1.0f)", self.source)
        self.assertIn("sx = px * cr - py * sr + cx + fWeaveX", self.source)
        self.assertIn("sy = px * sr + py * cr + cy + fWeaveY", self.source)
        self.assertIn("doSpatial", self.source)
        self.assertNotIn("args.doDamage || args.doSpatial", self.source)

    def test_build_scripts_enable_cuda_render_path(self):
        for script in ["scripts/compile_all.bat", "build_plugin.bat"]:
            text = read_text(script)
            self.assertIn("/DDUXUN_ENABLE_CUDA_RENDER=1", text)
            self.assertIn('/Fo"%OUT%\\DuXunFilmSim.obj"', text)

    def test_gpu_plan_documents_real_kernel_requirements(self):
        plan = read_text("docs/gpu-acceleration-plan.md")
        for token in [
            "OpenFX GPU Render",
            "DUXUN_ENABLE_OPENCL_RENDER",
            "DUXUN_ENABLE_CUDA_RENDER",
            "CPU fallback",
            "kOfxStatGPURenderFailed",
        ]:
            self.assertIn(token, plan)


if __name__ == "__main__":
    unittest.main()
