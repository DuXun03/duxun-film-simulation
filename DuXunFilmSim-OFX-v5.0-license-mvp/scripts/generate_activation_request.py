from __future__ import annotations

import argparse

from duxun_license import activation_request_path, add_common_output_arg, build_activation_request, write_json


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate a DuXun Film Simulation offline activation request.")
    add_common_output_arg(parser, activation_request_path())
    args = parser.parse_args()

    request = build_activation_request()
    write_json(args.output, request)
    print(f"Activation request written: {args.output}")
    print(f"Machine hash: {request['machineHash']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
