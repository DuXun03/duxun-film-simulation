"""Decode all t3mujinpack .dtstyle files and extract film characteristics."""
import base64, zlib, struct, os, sys, re, json

DTSTYLE_DIR = sys.argv[1] if len(sys.argv) > 1 else "."

def decode_op_params(b64_data):
    try:
        raw = base64.b64decode(b64_data)
        return zlib.decompress(raw)
    except:
        for strip in range(1, 8):
            try:
                raw = base64.b64decode(b64_data[strip:])
                return zlib.decompress(raw)
            except:
                continue
    return None

def extract_curve_points(floats, n_points):
    """Extract n (x,y) pairs from float array."""
    points = []
    for i in range(n_points):
        x = floats[i*2]
        y = floats[i*2 + 1]
        points.append((x, y))
    return points

def analyze_l_curve(points):
    """Analyze L-channel curve to derive contrast, shoulder, toe, density."""
    if not points or len(points) < 3:
        return None

    # Separate x and y values
    xs = [p[0] for p in points]
    ys = [p[1] for p in points]

    # Density: overall brightness shift at midpoint
    mid_idx = len(points) // 2
    density_shift = ys[mid_idx] - xs[mid_idx] if xs[mid_idx] > 0.4 else 0

    # Contrast: slope around middle (0.3-0.7 range)
    # Find points near 0.4 and 0.6
    y_at_04 = None
    y_at_06 = None
    for x, y in points:
        if y_at_04 is None and x >= 0.35:
            y_at_04 = y
            x_at_04 = x
        if y_at_06 is None and x >= 0.55:
            y_at_06 = y
            x_at_06 = x

    if y_at_04 is not None and y_at_06 is not None and (x_at_06 - x_at_04) > 0.01:
        contrast = (y_at_06 - y_at_04) / (x_at_06 - x_at_04)
    else:
        contrast = 1.0

    # Shoulder: highlight rolloff - compare y at x=0.8 vs linear
    y_at_hi = None
    for x, y in reversed(points):
        if x <= 0.85:
            y_at_hi = y
            x_at_hi = x
            break

    if y_at_hi is not None and x_at_hi > 0.6:
        # Linear would be y=x, so deviation = x_at_hi - y_at_hi
        shoulder_deviation = x_at_hi - y_at_hi
    else:
        shoulder_deviation = 0.1

    # Toe: shadow rolloff - compare y at x=0.2 vs linear
    y_at_lo = None
    for x, y in points:
        if x >= 0.15:
            y_at_lo = y
            x_at_lo = x
            break

    if y_at_lo is not None and x_at_lo > 0:
        toe_deviation = y_at_lo - x_at_lo
    else:
        toe_deviation = 0.05

    return {
        'contrast': round(contrast, 3),
        'shoulder': round(max(0.0, min(0.5, shoulder_deviation)), 3),
        'toe': round(max(0.0, min(0.5, toe_deviation)), 3),
        'density_shift': round(density_shift, 3),
        'num_points': len(points),
        'points': [(round(x,4), round(y,4)) for x, y in points],
    }

def main():
    files = sorted([f for f in os.listdir(DTSTYLE_DIR) if f.endswith('.dtstyle')])
    results = []

    for fname in files:
        filepath = os.path.join(DTSTYLE_DIR, fname)
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        tc_match = re.search(r'<operation>tonecurve</operation>\s*<op_params>([^<]+)</op_params>', content)
        cm_match = re.search(r'<operation>channelmixerrgb</operation>\s*<op_params>([^<]+)</op_params>', content)
        name_match = re.search(r'<name>t3mujinpack - ([^|]+)\|(.+)</name>', content)

        if not tc_match or not name_match:
            continue

        category = name_match.group(1).strip()
        film_name = name_match.group(2).strip()

        # Decode tonecurve
        data = decode_op_params(tc_match.group(1))
        if not data:
            continue

        n_floats = len(data) // 4
        floats = struct.unpack(f'<{n_floats}f', data[:n_floats*4])

        # L channel: find where zeros start (after L curve)
        # L curve has some points, then a/b curves
        # Find first zero-run to determine L curve end
        l_end = len(floats)
        for i in range(len(floats)):
            if floats[i] == 0.0 and i > 2:
                # Check if it's the start of a zero section
                is_zero_section = True
                for j in range(i, min(i+10, len(floats))):
                    if floats[j] != 0.0:
                        is_zero_section = False
                        break
                if is_zero_section:
                    l_end = i
                    break

        l_points_raw = [(floats[i], floats[i+1]) for i in range(0, min(l_end, len(floats)-1), 2)]
        l_points = [(x, y) for x, y in l_points_raw if x > 0 or y > 0]

        # Check if B&W (a/b channels all zero)
        is_bw = cm_match is not None
        if not is_bw:
            # Check if a/b channels are zeroed
            remaining = floats[l_end:]
            is_bw = all(v == 0.0 for v in remaining) if remaining else False

        l_analysis = analyze_l_curve(l_points)

        # Get channel mixer data for B&W
        cm_data = {}
        if cm_match:
            cm_decoded = decode_op_params(cm_match.group(1))
            if cm_decoded:
                cm_n = len(cm_decoded) // 4
                cm_floats = struct.unpack(f'<{cm_n}f', cm_decoded[:cm_n*4])
                cm_data = [round(v, 4) for v in cm_floats]

        result = {
            'name': film_name,
            'category': category,
            'is_bw': is_bw,
            'l_curve_points': len(l_points),
            'l_analysis': l_analysis,
            'channel_mixer': cm_data if cm_data else None,
        }
        results.append(result)

        print(f"{category} | {film_name}")
        if l_analysis:
            print(f"  contrast={l_analysis['contrast']}, shoulder={l_analysis['shoulder']}, "
                  f"toe={l_analysis['toe']}, density_shift={l_analysis['density_shift']}, "
                  f"points={l_analysis['num_points']}, B&W={is_bw}")
        else:
            print(f"  (no L curve analysis)")

    # Save full results
    outpath = os.path.join(DTSTYLE_DIR, '..', 'film_analysis.json')
    with open(outpath, 'w', encoding='utf-8') as f:
        json.dump(results, f, indent=2, ensure_ascii=False)
    print(f"\nSaved analysis to {outpath}")
    print(f"Total films analyzed: {len(results)}")

if __name__ == '__main__':
    main()
