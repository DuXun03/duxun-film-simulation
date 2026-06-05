"""
Generate C++ preset code from t3mujinpack analysis data + film characteristic knowledge.

Our plugin parameters:
  contrast  (0.5 - 2.0)  - S-curve midtone slope
  shoulder  (0.0 - 0.5)  - highlight rolloff
  toe       (0.0 - 0.5)  - shadow rolloff
  density   (0.5 - 1.5)  - overall density/brightness
  halation  (0.0 - 0.5)  - red highlight bloom
  grain     (0.0 - 0.5)  - film grain intensity
  saturation (0.0 - 2.0) - color saturation (0 = B&W)
  vignette  (0.0 - 0.5)  - edge darkening
"""
import json, os, sys

ANALYSIS_PATH = sys.argv[1] if len(sys.argv) > 1 else "film_analysis.json"

# Film characteristic knowledge overrides
# key: film name (as in t3mujinpack), value: overrides for specific params
# Only include films where we want to override the auto-derived values
MANUAL_OVERRIDES = {
    # Black & White
    "AGFA APX 100": {"grain": 0.12, "halation": 0.10},
    "AGFA APX 25": {"grain": 0.04, "halation": 0.05, "shoulder": 0.20},
    "Fuji Neopan 1600": {"grain": 0.40, "halation": 0.15, "shoulder": 0.25},
    "Fuji Neopan Acros 100": {"grain": 0.06, "halation": 0.05, "shoulder": 0.15},
    "Ilford Delta 100": {"grain": 0.05, "halation": 0.05, "shoulder": 0.18},
    "Ilford Delta 3200": {"grain": 0.45, "halation": 0.20, "shoulder": 0.30},
    "Ilford Delta 400": {"grain": 0.15, "halation": 0.10, "shoulder": 0.20},
    "Ilford FP4 125": {"grain": 0.08, "halation": 0.08, "shoulder": 0.18},
    "Ilford HP5 Plus 400": {"grain": 0.22, "halation": 0.12, "shoulder": 0.22},
    "Ilford XP2": {"grain": 0.14, "halation": 0.08, "shoulder": 0.20},
    "Kodak T-Max 3200": {"grain": 0.42, "halation": 0.18, "shoulder": 0.35},
    "Kodak Tri-X 400": {"grain": 0.28, "halation": 0.15, "shoulder": 0.22},

    # Color Negative - Kodak
    "Kodak Portra 160": {"grain": 0.04, "saturation": 0.88, "halation": 0.12, "shoulder": 0.25},
    "Kodak Portra 160 NC": {"grain": 0.04, "saturation": 0.82, "halation": 0.10, "shoulder": 0.28},
    "Kodak Portra 160 VC": {"grain": 0.05, "saturation": 0.98, "halation": 0.12, "shoulder": 0.22},
    "Kodak Portra 400": {"grain": 0.08, "saturation": 0.90, "halation": 0.15, "shoulder": 0.28},
    "Kodak Portra 400 NC": {"grain": 0.07, "saturation": 0.85, "halation": 0.13, "shoulder": 0.28},
    "Kodak Portra 400 UC": {"grain": 0.08, "saturation": 0.95, "halation": 0.14, "shoulder": 0.25},
    "Kodak Portra 400 VC": {"grain": 0.09, "saturation": 1.05, "halation": 0.15, "shoulder": 0.22},
    "Kodak Portra 800": {"grain": 0.18, "saturation": 0.92, "halation": 0.20, "shoulder": 0.30},
    "Kodak Ektar 100": {"grain": 0.03, "saturation": 1.18, "halation": 0.10, "shoulder": 0.20},
    "Kodak Gold 200": {"grain": 0.14, "saturation": 1.08, "halation": 0.15, "shoulder": 0.22},
    "Kodak ColorPlus 200": {"grain": 0.13, "saturation": 1.02, "halation": 0.14, "shoulder": 0.20},
    "Kodak Ultra Max 400": {"grain": 0.16, "saturation": 1.06, "halation": 0.16, "shoulder": 0.22},

    # Color Negative - CineStill
    "CineStill 50D": {"grain": 0.05, "saturation": 0.95, "halation": 0.35, "shoulder": 0.30},
    "CineStill 800T": {"grain": 0.22, "saturation": 0.88, "halation": 0.50, "shoulder": 0.35},

    # Color Negative - Fuji
    "Fuji Pro 160C": {"grain": 0.05, "saturation": 0.92, "halation": 0.12, "shoulder": 0.22},
    "Fuji Pro 400H": {"grain": 0.08, "saturation": 0.85, "halation": 0.12, "shoulder": 0.25},
    "Fuji Pro 800Z": {"grain": 0.18, "saturation": 0.95, "halation": 0.18, "shoulder": 0.28},
    "Fuji Superia 100": {"grain": 0.06, "saturation": 1.02, "halation": 0.12, "shoulder": 0.20},
    "Fuji Superia 200": {"grain": 0.09, "saturation": 1.05, "halation": 0.13, "shoulder": 0.20},
    "Fuji Superia 400": {"grain": 0.12, "saturation": 1.05, "halation": 0.14, "shoulder": 0.22},
    "Fuji Superia 800": {"grain": 0.18, "saturation": 1.00, "halation": 0.16, "shoulder": 0.25},
    "Fuji Superia 1600": {"grain": 0.30, "saturation": 1.08, "halation": 0.20, "shoulder": 0.28},
    "Fuji Superia HG 1600": {"grain": 0.32, "saturation": 1.10, "halation": 0.22, "shoulder": 0.28},

    # Color Negative - Agfa
    "Agfa Vista 100": {"grain": 0.06, "saturation": 0.98, "halation": 0.12, "shoulder": 0.22},
    "Agfa Vista 200": {"grain": 0.10, "saturation": 1.00, "halation": 0.13, "shoulder": 0.22},
    "Agfa Vista 400": {"grain": 0.15, "saturation": 1.02, "halation": 0.15, "shoulder": 0.22},

    # Color Slide - Fuji
    "Fuji Astia 100F": {"grain": 0.03, "saturation": 1.00, "halation": 0.10, "shoulder": 0.25},
    "Fuji Fortia SP 50": {"grain": 0.03, "saturation": 1.25, "halation": 0.10, "shoulder": 0.18},
    "Fuji Provia 100F": {"grain": 0.03, "saturation": 1.05, "halation": 0.10, "shoulder": 0.22},
    "Fuji Provia 400F": {"grain": 0.06, "saturation": 1.02, "halation": 0.12, "shoulder": 0.22},
    "Fuji Provia 400X": {"grain": 0.08, "saturation": 1.08, "halation": 0.12, "shoulder": 0.22},
    "Fuji Sensia 100": {"grain": 0.05, "saturation": 1.08, "halation": 0.10, "shoulder": 0.20},
    "Fuji Velvia 50": {"grain": 0.03, "saturation": 1.30, "halation": 0.08, "shoulder": 0.18},
    "Fuji Velvia 100": {"grain": 0.04, "saturation": 1.25, "halation": 0.08, "shoulder": 0.20},

    # Color Slide - Kodak
    "Kodak Ektachrome 100 G": {"grain": 0.03, "saturation": 1.05, "halation": 0.10, "shoulder": 0.22},
    "Kodak Ektachrome 100 GX": {"grain": 0.03, "saturation": 1.02, "halation": 0.10, "shoulder": 0.22},
    "Kodak Ektachrome 100 VS": {"grain": 0.04, "saturation": 1.18, "halation": 0.10, "shoulder": 0.20},
    "Kodak Elite Chrome 400": {"grain": 0.08, "saturation": 1.08, "halation": 0.12, "shoulder": 0.22},
    "Kodak Kodakchrome 25": {"grain": 0.02, "saturation": 1.15, "halation": 0.08, "shoulder": 0.15},
    "Kodak Kodakchrome 64": {"grain": 0.03, "saturation": 1.12, "halation": 0.08, "shoulder": 0.18},
    "Kodak Kodakchrome 200": {"grain": 0.05, "saturation": 1.10, "halation": 0.10, "shoulder": 0.20},
}

def clamp(v, lo, hi):
    return max(lo, min(hi, v))

def compute_params(film):
    """Compute final plugin parameters from analysis + overrides."""
    name = film['name']
    is_bw = film['is_bw']
    analysis = film.get('l_analysis', {})

    # Start with defaults
    params = {
        'contrast': 1.15,
        'shoulder': 0.20,
        'toe': 0.15,
        'density': 0.85,
        'halation': 0.10,
        'grain': 0.10,
        'saturation': 1.00,
        'vignette': 0.00,
    }

    # Apply extracted L-channel analysis
    if analysis:
        # Contrast: use extracted value
        raw_contrast = analysis.get('contrast', 1.15)
        params['contrast'] = clamp(raw_contrast, 0.7, 1.8)

        # Toe: use extracted value
        raw_toe = analysis.get('toe', 0.0)
        params['toe'] = clamp(raw_toe + 0.10, 0.0, 0.45)

        # Density: invert density_shift (positive shift = brighter = lower density)
        density_shift = analysis.get('density_shift', 0.0)
        params['density'] = clamp(1.0 - density_shift * 0.5, 0.65, 1.15)

    # Apply B&W detection
    if is_bw:
        params['saturation'] = 0.0

    # Apply manual overrides
    if name in MANUAL_OVERRIDES:
        for k, v in MANUAL_OVERRIDES[name].items():
            params[k] = v

    # Final clamping
    params['contrast'] = clamp(params['contrast'], 0.7, 1.8)
    params['shoulder'] = clamp(params['shoulder'], 0.0, 0.5)
    params['toe'] = clamp(params['toe'], 0.0, 0.45)
    params['density'] = clamp(params['density'], 0.6, 1.2)
    params['halation'] = clamp(params['halation'], 0.0, 0.5)
    params['grain'] = clamp(params['grain'], 0.0, 0.5)
    params['saturation'] = clamp(params['saturation'], 0.0, 1.5)
    params['vignette'] = clamp(params['vignette'], 0.0, 0.3)

    return params

def main():
    with open(ANALYSIS_PATH, 'r', encoding='utf-8') as f:
        films = json.load(f)

    # Compute all presets
    presets = []
    for film in films:
        name = film['name']
        cat = film['category']
        params = compute_params(film)
        presets.append({
            'name': name,
            'category': cat,
            **params,
        })

    # Add Custom at the end
    presets.append({
        'name': 'Custom',
        'category': 'Custom',
        'contrast': 1.15, 'shoulder': 0.20, 'toe': 0.15,
        'density': 0.85, 'halation': 0.10, 'grain': 0.10,
        'saturation': 1.00, 'vignette': 0.00,
    })

    # Generate C++ code
    print("// Auto-generated from t3mujinpack analysis")
    print(f"// Total presets: {len(presets)}")
    print("")
    print("struct FilmPreset {")
    print("    const char* name;")
    print("    double contrast, shoulder, toe, density, halation, grain, saturation, vignette;")
    print("};")
    print("")
    print(f"static const int gNumPresets = {len(presets)};")
    print("static const FilmPreset gPresets[] = {")

    for p in presets:
        c = p['contrast']
        s = p['shoulder']
        t = p['toe']
        d = p['density']
        h = p['halation']
        g = p['grain']
        sat = p['saturation']
        v = p['vignette']
        print(f'    {{ "{p["name"]}", {c:.2f}, {s:.2f}, {t:.2f}, {d:.2f}, {h:.2f}, {g:.2f}, {sat:.2f}, {v:.2f} }},')

    print("};")

    # Also generate the Choice option names for reference
    print("\n// Choice option names (for kOfxParamPropChoiceOption):")
    for i, p in enumerate(presets):
        print(f'// [{i}] {p["name"]}')

    # Save JSON for reference
    outpath = os.path.join(os.path.dirname(ANALYSIS_PATH), 'final_presets.json')
    with open(outpath, 'w', encoding='utf-8') as f:
        json.dump(presets, f, indent=2, ensure_ascii=False)
    print(f"\nSaved final presets to {outpath}")

if __name__ == '__main__':
    main()
