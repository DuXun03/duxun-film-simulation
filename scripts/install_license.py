from __future__ import annotations

import argparse
from pathlib import Path

from duxun_license import copy_license_atomically, license_path, load_public_key, read_json, validate_license_document


def main() -> int:
    parser = argparse.ArgumentParser(description="Validate and install a DuXun Film Simulation license.")
    parser.add_argument("license", type=Path, help="Signed license.json returned by the offline signer")
    parser.add_argument("--public-key", type=Path, help="Optional ECDSA P-256 public key PEM for staging/QA keys")
    parser.add_argument("--machine-hash", help="Expected machineHash override for staging/QA checks")
    parser.add_argument("-o", "--output", type=Path, default=license_path(), help=f"Install path, default: {license_path()}")
    args = parser.parse_args()

    document = read_json(args.license)
    public_key = load_public_key(args.public_key) if args.public_key else None
    validation = validate_license_document(
        document,
        public_key=public_key,
        expected_machine_hash=args.machine_hash,
    )
    if not validation.ok:
        print(f"License rejected: {validation.code}: {validation.message}")
        return 1

    installed = copy_license_atomically(args.license, args.output)
    print(f"License installed: {installed}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
