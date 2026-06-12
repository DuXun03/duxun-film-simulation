from __future__ import annotations

import argparse
from pathlib import Path

from duxun_license import license_path, load_public_key, read_json, validate_license_document


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify a DuXun Film Simulation license file.")
    parser.add_argument("--public-key", type=Path, help="Optional ECDSA P-256 public key PEM for staging/QA keys")
    parser.add_argument("--machine-hash", help="Expected machineHash override for staging/QA checks")
    parser.add_argument("license", nargs="?", type=Path, default=license_path())
    args = parser.parse_args()

    public_key = load_public_key(args.public_key) if args.public_key else None
    validation = validate_license_document(
        read_json(args.license),
        public_key=public_key,
        expected_machine_hash=args.machine_hash,
    )
    print(f"{validation.code}: {validation.message}")
    return 0 if validation.ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
