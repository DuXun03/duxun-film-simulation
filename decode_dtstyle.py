"""Decode Darktable .dtstyle files - handle gz prefix."""
import base64
import zlib
import struct
import os
import sys
import re

DTSTYLE_DIR = sys.argv[1] if len(sys.argv) > 1 else "."

def decode_op_params(op_params_b64):
    """Decode base64 + zlib compressed op_params.
    Darktable prepends 'gz0X' or 'gz1X' header to the base64 data."""

    # Try raw base64 decode first
    try:
        raw = base64.b64decode(op_params_b64)
        decompressed = zlib.decompress(raw)
        return decompressed
    except:
        pass

    # Strip leading 'gz' prefix bytes (may be gz0X, gz1X, etc.)
    # The prefix length varies; try stripping 1-4 base64 chars (which is 0-3 bytes)
    for strip in range(1, 6):
        try:
            raw = base64.b64decode(op_params_b64[strip:])
            # Try zlib decompress
            try:
                decompressed = zlib.decompress(raw)
                return decompressed
            except:
                # Try with wbits=-15 (raw deflate)
                try:
                    decompressed = zlib.decompress(raw, -15)
                    return decompressed
                except:
                    continue
        except:
            continue

    # Last resort: decode everything and try different offsets
    raw = base64.b64decode(op_params_b64)
    for offset in range(len(raw)):
        try:
            decompressed = zlib.decompress(raw[offset:])
            return decompressed
        except:
            continue
    return None

def main():
    files = sorted([f for f in os.listdir(DTSTYLE_DIR) if f.endswith('.dtstyle')])

    for fname in files[:3]:
        filepath = os.path.join(DTSTYLE_DIR, fname)
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        tc_match = re.search(r'<operation>tonecurve</operation>\s*<op_params>([^<]+)</op_params>', content)
        name_match = re.search(r'<name>([^<]+)</name>', content)
        name = name_match.group(1) if name_match else fname

        print(f"\n{'='*60}")
        print(f"Film: {name}")
        print(f"{'='*60}")

        if tc_match:
            b64_data = tc_match.group(1)
            print(f"B64 prefix: {b64_data[:10]}")
            print(f"B64 length: {len(b64_data)}")

            data = decode_op_params(b64_data)
            if data:
                n_floats = len(data) // 4
                floats = struct.unpack(f'<{n_floats}f', data[:n_floats*4])
                print(f"Decompressed: {len(data)} bytes, {n_floats} floats")
                print(f"First 40 values:")
                for i, v in enumerate(floats[:40]):
                    print(f"  [{i}] {v:.6f}")
            else:
                print("FAILED to decompress")

if __name__ == '__main__':
    main()
