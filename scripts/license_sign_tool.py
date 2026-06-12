from __future__ import annotations

import argparse
from pathlib import Path

from duxun_license import (
    build_license_document,
    expires_from_days,
    generate_private_key,
    load_private_key,
    public_key_constants,
    read_json,
    sign_license_document,
    write_public_key,
    write_json,
)


def main() -> int:
    parser = argparse.ArgumentParser(description="Offline DuXun Film Simulation license signer.")
    parser.add_argument("--generate-private-key", type=Path, help="Create a new offline ECDSA P-256 private key")
    parser.add_argument("--public-key-output", type=Path, help="Write the generated public key PEM for QA verification")
    parser.add_argument("--request", type=Path, help="activation-request.json from the customer machine")
    parser.add_argument("--private-key", type=Path, help="Offline ECDSA P-256 private key PEM")
    parser.add_argument("-o", "--output", type=Path, default=Path("license.json"), help="Output signed license path")
    parser.add_argument("--license-type", choices=["trial", "activated", "buyout"], default="buyout")
    parser.add_argument("--customer-id", default="manual-customer")
    parser.add_argument("--order-id", default="manual-order")
    parser.add_argument("--expires-at", help="UTC expiry such as 2026-07-12T00:00:00Z")
    parser.add_argument("--days", type=int, help="Set expiresAt to now + days for trial/activated licenses")
    args = parser.parse_args()

    if args.generate_private_key:
        key = generate_private_key(args.generate_private_key)
        print(f"Private key written: {args.generate_private_key}")
        if args.public_key_output:
            write_public_key(args.public_key_output, key.public_key())
            print(f"Public key written: {args.public_key_output}")
        print(public_key_constants(key.public_key()))
        return 0

    if not args.request or not args.private_key:
        parser.error("--request and --private-key are required unless --generate-private-key is used")

    request = read_json(args.request)
    expires_at = args.expires_at
    if args.license_type != "buyout" and not expires_at:
        if not args.days:
            parser.error("--days or --expires-at is required for trial/activated licenses")
        expires_at = expires_from_days(args.days)

    document = build_license_document(
        request=request,
        license_type=args.license_type,
        customer_id=args.customer_id,
        order_id=args.order_id,
        expires_at=expires_at,
    )
    signed = sign_license_document(document, load_private_key(args.private_key))
    write_json(args.output, signed)
    print(f"Signed license written: {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
