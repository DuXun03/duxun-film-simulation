#!/usr/bin/env python3
"""
FilmSim IDT LUT Converter

Converts mycolor 3D LUTs (JSON format, 64^3 per-channel arrays)
to Resolve-compatible .cube LUT format.

Usage: python convert_luts.py
Output: luts/ directory in CWD
"""
import json
import os
import sys

LUT_SIZE = 64
SRC_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "luts_src")
DST_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "luts")

# Camera IDT name mapping
LUT_MAP = {
    "ACEScct-to-2383-ACEScct-CUBE64.json": "ACEScct_to_Kodak2383",
    "ACEScct-to-DWG-CUBE64.json": "ACEScct_to_DWG",
    "BMD_4_6k_Film_to_ACEScct_CUBE64.json": "BMD_URSAMini46k_Film_to_ACEScct",
    "BMD_4k_Film_to_ACEScct_CUBE64.json": "BMD_URSAMini4k_Film_to_ACEScct",
    "BMD_Broadcast_Film_v4_to_ACEScct_CUBE64.json": "BMD_Broadcast_Film_to_ACEScct",
    "BMD_CinemaCamera_Film_to_ACEScct_CUBE64.json": "BMD_CinemaCamera_Film_to_ACEScct",
    "BMD_Pocket_6k_Film_to_ACEScct_CUBE64.json": "BMD_Pocket6k_Film_to_ACEScct",
    "BMD_Pocket_Film_to_ACEScct_CUBE64.json": "BMD_Pocket4k_Film_to_ACEScct",
    "LogC4-to-Rec2020-CUBE64.json": "ARRI_LogC4_to_Rec2020",
    "LogC4-to-Rec709-CUBE64.json": "ARRI_LogC4_to_Rec709",
    "OM_LOG400_BT2020_to_ACEScct_CUBE64.json": "OM_LOG400_BT2020_to_ACEScct",
    "OM_LOG400_P3_to_ACEScct_CUBE64.json": "OM_LOG400_P3_to_ACEScct",
}

def convert_json_to_cube(src_path, dst_path, title):
    """Convert a mycolor JSON 3D LUT to .cube format."""
    with open(src_path, 'r') as f:
        data = json.load(f)

    # Data is a flat array: [R,G,B,1, R,G,B,1, ...] with LUT_SIZE^3 entries
    expected_entries = LUT_SIZE ** 3
    expected_floats = expected_entries * 4

    if len(data) != expected_floats:
        print(f"  WARNING: Expected {expected_floats} floats, got {len(data)}")

    with open(dst_path, 'w') as f:
        f.write(f'# FilmSim IDT LUT: {title}\n')
        f.write(f'# Source: boomyjee/mycolor (MIT License)\n')
        f.write(f'# Converted by film-sim-plugin\n')
        f.write(f'TITLE "{title}"\n')
        f.write(f'LUT_3D_SIZE {LUT_SIZE}\n')
        f.write(f'DOMAIN_MIN 0.0 0.0 0.0\n')
        f.write(f'DOMAIN_MAX 1.0 1.0 1.0\n')

        # Write RGB triplets (skip the '1' alpha)
        for i in range(0, len(data), 4):
            if i + 3 < len(data):
                r = max(0.0, min(1.0, data[i]))
                g = max(0.0, min(1.0, data[i+1]))
                b = max(0.0, min(1.0, data[i+2]))
                f.write(f'{r:.6f} {g:.6f} {b:.6f}\n')

    size_kb = os.path.getsize(dst_path) / 1024
    print(f'  -> {os.path.basename(dst_path)} ({size_kb:.0f} KB)')


def main():
    os.makedirs(DST_DIR, exist_ok=True)
    print(f"Converting {len(LUT_MAP)} LUTs to {DST_DIR}/\n")

    converted = 0
    for src_name, title in LUT_MAP.items():
        src_path = os.path.join(SRC_DIR, src_name)
        dst_path = os.path.join(DST_DIR, f"{title}.cube")

        if not os.path.exists(src_path):
            print(f"  SKIP: {src_name} not found")
            continue

        print(f"  {src_name}")
        try:
            convert_json_to_cube(src_path, dst_path, title)
            converted += 1
        except Exception as e:
            print(f"    ERROR: {e}")

    # Also create the non-LUT IDT list (math-based IDTs from mycolor)
    idt_list_path = os.path.join(DST_DIR, "available-idts.md")
    with open(idt_list_path, 'w') as f:
        f.write("""# FilmSim -- Available IDT Profiles

## LUT-based IDTs (in luts/ directory)

Use these .cube files as 3D LUTs in Resolve's CST node or LUT OFX.

""")
        for src_name, title in LUT_MAP.items():
            f.write(f"- **{title}** → `luts/{title}.cube`\n")

        f.write("""
## Math-based IDTs (built into mycolor engine)

These are implemented as GLSL/DCTL algorithms (no external LUT needed).
Available in FilmSim-Mycolor.h and can be exposed as DCTL modules in Phase 4.

| Profile | Color Space | Method |
|---------|-------------|--------|
| sRGB | sRGB → ACEScct | Inverse sRGB EOTF + gamut compression |
| Rec709 | Rec.709 → ACEScct | Inverse Rec.709 OETF + gamut compression |
| ArriLogC | ARRI LogC3 → ACEScct | Normalized logC to linear + AWG4 matrix |
| ArriLogC4 | ARRI LogC4 → ACEScct | LogC4 to scene-linear + AWG4 matrix (Alexa 35) |
| Cineon | Cineon Log → ACEScct | Cineon CV to density to linear + gamut compression |
| ProPhotoRGB | ProPhoto → ACEScct | Linear conversion + gamut compression |
| P3D60/D65 | Display P3 → ACEScct | EOTF inverse + gamut compression |
| VisionLog | VisionLog → ACEScct | Custom log curve + AWG4 matrix |
| BMD Gen5 | BMD Film Gen5 → ACEScct | Piecewise log curve + gamut compression |
| Bolex Log | BolexLog → ACEScct | Custom log curve + conversion matrix |
""")

    print(f"\nDone: {converted} LUTs converted, IDT list saved to {idt_list_path}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
