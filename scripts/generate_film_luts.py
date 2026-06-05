#!/usr/bin/env python3
"""
独寻胶片模拟 -- Pure Python Film Stock LUT Generator
No numpy dependency. Parses spectral-film-lut source files via AST.
"""
import ast
import math
import json
import os
import sys

SITE_PKG = r"C:\Users\LI\.workbuddy\binaries\python\versions\3.13.12\Lib\site-packages\spectral_film_lut"
DST_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "presets", "film_stocks")
LUT_SIZE = 33

def parse_dict(node):
    """Recursively parse an AST dict node to Python dict."""
    if isinstance(node, ast.Dict):
        result = {}
        for k, v in zip(node.keys, node.values):
            key = parse_node(k)
            val = parse_node(v)
            result[key] = val
        return result
    return parse_node(node)

def parse_node(node):
    if isinstance(node, ast.Constant):
        return node.value
    elif isinstance(node, ast.Dict):
        return parse_dict(node)
    elif isinstance(node, ast.List):
        return [parse_node(item) for item in node.elts]
    elif isinstance(node, ast.Tuple):
        return tuple(parse_node(item) for item in node.elts)
    elif isinstance(node, ast.Name):
        return node.id
    elif isinstance(node, ast.Call):
        return {"_call": parse_node(node.func), "args": [parse_node(a) for a in node.args]}
    elif isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
        return -parse_node(node.operand)
    elif isinstance(node, ast.BinOp):
        return None  # skip expressions
    return None

def find_film_stocks():
    """Walk the package and find all film stock .py files."""
    stocks = []
    for root, dirs, files in os.walk(SITE_PKG):
        for f in files:
            if f.endswith('.py') and f != '__init__.py':
                path = os.path.join(root, f)
                rel = os.path.relpath(path, SITE_PKG)
                category = rel.replace('\\', '/').split('/')[0]  # negative_film, print_film, etc.
                stocks.append((category, path))
    return stocks

def extract_film_data(filepath):
    """Parse a film stock .py file and extract FilmData arguments."""
    with open(filepath, 'r', encoding='utf-8') as f:
        source = f.read()
    try:
        tree = ast.parse(source)
    except SyntaxError:
        return None

    # Find FilmData() call
    for node in ast.walk(tree):
        if isinstance(node, ast.Assign) and len(node.targets) == 1:
            value = node.value
            if isinstance(value, ast.Call):
                # Check if it's FilmData() call
                func = value.func
                if isinstance(func, ast.Name) and func.id == 'FilmData':
                    kwargs = {}
                    for kw in value.keywords:
                        kwargs[kw.arg] = parse_node(kw.value)
                    return kwargs
    return None

def interpolate(x1, y1, x2, y2, x):
    """Linear interpolation."""
    if x2 == x1:
        return y1
    return y1 + (y2 - y1) * (x - x1) / (x2 - x1)

def dict_lookup(d, x):
    """Look up value in a wavelength→value dict via linear interpolation."""
    keys = sorted(d.keys())
    if not keys:
        return 0.0
    if x <= keys[0]:
        return d[keys[0]]
    if x >= keys[-1]:
        return d[keys[-1]]
    for i in range(len(keys)-1):
        if keys[i] <= x <= keys[i+1]:
            return interpolate(keys[i], d[keys[i]], keys[i+1], d[keys[i+1]], x)
    return 0.0

def compute_rgb_matrix(sensitivity):
    """Compute 3x3 RGB cross-coupling from spectral sensitivity curves."""
    wls = list(range(380, 781, 5))  # 380-780nm at 5nm steps

    # Simplified RGB response from spectral sensitivity
    # R: 580-680nm, G: 500-580nm, B: 380-500nm (approximate)
    r_response = 0.0
    g_response = 0.0
    b_response = 0.0

    if len(sensitivity) >= 3:
        r_curve = sensitivity[0] if isinstance(sensitivity[0], dict) else {}
        g_curve = sensitivity[1] if len(sensitivity) > 1 and isinstance(sensitivity[1], dict) else {}
        b_curve = sensitivity[2] if len(sensitivity) > 2 and isinstance(sensitivity[2], dict) else {}

        for wl in wls:
            rs = dict_lookup(r_curve, wl)
            gs = dict_lookup(g_curve, wl)
            bs = dict_lookup(b_curve, wl)

            if 580 <= wl <= 680:
                weight = 1.0 - abs(wl - 630) / 50
                r_response += max(0, weight) * (rs + gs * 0.1 + bs * 0.05)
            if 500 <= wl <= 580:
                weight = 1.0 - abs(wl - 540) / 40
                g_response += max(0, weight) * (gs + rs * 0.15 + bs * 0.1)
            if 380 <= wl <= 500:
                weight = 1.0 - abs(wl - 440) / 60
                b_response += max(0, weight) * (bs + gs * 0.2 + rs * 0.05)

    # Build 3x3 matrix (normalized to preserve white)
    total = r_response + g_response + b_response
    if total < 0.001:
        return [[1.0, 0.0, 0.0], [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]]

    # Cross-coupling matrix
    matrix = [
        [r_response/total * 1.1, g_response/total * -0.05, b_response/total * -0.05],
        [r_response/total * 0.02, g_response/total * 1.05, b_response/total * -0.06],
        [r_response/total * -0.05, g_response/total * -0.03, b_response/total * 1.08],
    ]

    # Normalize rows to sum ≈ 1
    for i in range(3):
        s = sum(matrix[i])
        if abs(s) > 0.001:
            matrix[i] = [v / s for v in matrix[i]]

    return matrix

def compute_contrast(curves):
    """Extract contrast from sensiometric curve."""
    if not curves or len(curves) < 1:
        return 1.15
    curve = curves[0] if isinstance(curves[0], dict) else {}
    if not curve:
        return 1.15
    keys = sorted(curve.keys())
    if len(keys) < 5:
        return 1.15
    mid = len(keys) // 2
    return 0.9 + len(keys) / 100.0  # rough heuristic

def generate_lut(output_path, name, manufacturer, iso, year, film_type, matrix, contrast):
    """Generate .cube 3D LUT."""
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(f'# 独寻胶片模拟 -- {name}\n')
        f.write(f'# {manufacturer}, ISO {iso}, {year}, {film_type}\n')
        f.write(f'TITLE "{name}"\n')
        f.write(f'LUT_3D_SIZE {LUT_SIZE}\n')
        f.write(f'DOMAIN_MIN 0.0 0.0 0.0\n')
        f.write(f'DOMAIN_MAX 1.0 1.0 1.0\n')

        m = matrix
        for b in range(LUT_SIZE):
            for g in range(LUT_SIZE):
                for r in range(LUT_SIZE):
                    rin, gin, bin_val = r/(LUT_SIZE-1), g/(LUT_SIZE-1), b/(LUT_SIZE-1)
                    rout = m[0][0]*rin + m[0][1]*gin + m[0][2]*bin_val
                    gout = m[1][0]*rin + m[1][1]*gin + m[1][2]*bin_val
                    bout = m[2][0]*rin + m[2][1]*gin + m[2][2]*bin_val
                    rout = rout ** (1.0/contrast) if rout > 0 else 0
                    gout = gout ** (1.0/contrast) if gout > 0 else 0
                    bout = bout ** (1.0/contrast) if bout > 0 else 0
                    rout = max(0.0, min(1.0, rout))
                    gout = max(0.0, min(1.0, gout))
                    bout = max(0.0, min(1.0, bout))
                    f.write(f'{rout:.6f} {gout:.6f} {bout:.6f}\n')

def main():
    os.makedirs(DST_DIR, exist_ok=True)
    stock_files = find_film_stocks()
    print(f"Found {len(stock_files)} film stock files\n")

    index = []
    skipped = 0

    for category, filepath in stock_files:
        data = extract_film_data(filepath)
        if not data:
            skipped += 1
            continue

        name = data.get('name', os.path.basename(filepath))
        manufacturer = data.get('manufacturer', 'Unknown')
        iso = data.get('iso', 0)
        year = data.get('year', 0)
        film_type = data.get('film_type', category)
        sensitivity = data.get('log_sensitivity', [])
        curves = data.get('sensiometric_curve', [])

        matrix = compute_rgb_matrix(sensitivity)
        contrast = compute_contrast(curves)

        safe_name = name.replace(' ', '_').replace('/', '-').replace('.', '')
        lut_path = os.path.join(DST_DIR, f"{safe_name}.cube")

        try:
            generate_lut(lut_path, name, manufacturer, iso, year, film_type, matrix, contrast)
            info = {
                "name": name, "manufacturer": manufacturer,
                "iso": iso, "year": year, "film_type": film_type,
                "contrast": round(contrast, 2),
                "matrix": [[round(v, 4) for v in row] for row in matrix]
            }
            index.append(info)
            print(f"  [+] {name} ({manufacturer}, {film_type})")
        except Exception as e:
            print(f"  [X] {name}: {e}")

    # Write index
    with open(os.path.join(DST_DIR, "film_stocks.json"), 'w', encoding='utf-8') as f:
        json.dump(index, f, indent=2, ensure_ascii=False)

    # Write DCTL presets header
    with open(os.path.join(DST_DIR, "film_presets.h"), 'w', encoding='utf-8') as f:
        f.write(f"// 独寻胶片模拟 -- {len(index)} Film Stock Presets\n\n")
        f.write(f"#define DUXUN_FILM_STOCK_COUNT {len(index)}\n\n")
        names = "|".join(s["name"] for s in index)
        f.write(f'#define DUXUN_FILM_STOCK_NAMES "{names}"\n\n')
        f.write("__CONSTANT__ float DUXUN_FILM_PRESETS[DUXUN_FILM_STOCK_COUNT][12] = {\n")
        for s in index:
            m = s["matrix"]
            row = [s["contrast"], 0.35, 0.25,
                   m[0][0], m[0][1], m[0][2],
                   m[1][0], m[1][1], m[1][2],
                   m[2][0], m[2][1], m[2][2]]
            f.write(f'    {{ {", ".join(f"{v:.4f}f" for v in row)} }},  // {s["name"]}\n')
        f.write("};\n")

    print(f"\nDone: {len(index)} LUTs, {skipped} skipped")
    return 0

if __name__ == "__main__":
    sys.exit(main())
