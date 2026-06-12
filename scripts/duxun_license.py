"""Shared licensing helpers for DuXun Film Simulation v5.0.

The OFX plugin uses the same product id, payload canonicalization, and machine
hash contract. Private keys are always loaded from operator-provided files.
"""

from __future__ import annotations

import argparse
import base64
import ctypes
import hashlib
import json
import os
import platform
import shutil
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils


PRODUCT_ID = "duxun-filmsim-ofx-v5"
PRODUCT_VERSION = "5.0"
KEY_VERSION = "v1"
MACHINE_FINGERPRINT_VERSION = "machine-v1"
DEFAULT_FEATURES = ["core", "export"]
TRIAL_HOURS = 24

PUBLIC_KEY_V1_X = int("7C7D6AD71F542641D3CB8F346B999A34CFC6E0F00DA7CCB341EB08D5E69E8D76", 16)
PUBLIC_KEY_V1_Y = int("F714FC69F49C1AC7855DC7DB409882F8903E1304F4ACE0C172CD101EE655852B", 16)

LICENSE_FIELDS = [
    "customerId",
    "expiresAt",
    "features",
    "issuedAt",
    "keyVersion",
    "licenseType",
    "machineHash",
    "orderId",
    "productId",
]


@dataclass(frozen=True)
class LicenseValidation:
    ok: bool
    code: str
    message: str


def utc_now() -> datetime:
    return datetime.now(timezone.utc).replace(microsecond=0)


def isoformat_utc(value: datetime) -> str:
    return value.astimezone(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def parse_utc(value: str | None) -> datetime | None:
    if value is None:
        return None
    if not value.endswith("Z"):
        raise ValueError("UTC timestamps must end with Z")
    return datetime.fromisoformat(value[:-1] + "+00:00").astimezone(timezone.utc)


def program_data_root() -> Path:
    override = os.environ.get("DUXUN_FILMSIM_LICENSE_DIR")
    if override:
        return Path(override)
    base = os.environ.get("PROGRAMDATA", r"C:\ProgramData")
    return Path(base) / "DuXun" / "FilmSim"


def license_path() -> Path:
    return program_data_root() / "license.json"


def trial_path() -> Path:
    return program_data_root() / "trial.json"


def activation_request_path() -> Path:
    return program_data_root() / "activation-request.json"


def log_path() -> Path:
    return program_data_root() / "logs" / "license.log"


def ensure_program_data_layout() -> None:
    (program_data_root() / "logs").mkdir(parents=True, exist_ok=True)


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def append_log(message: str) -> None:
    ensure_program_data_layout()
    with log_path().open("a", encoding="utf-8") as handle:
        handle.write(f"{isoformat_utc(utc_now())} {message}\n")


def _normalize_field(value: Any) -> str:
    return str(value or "").strip().lower()


def _windows_machine_guid() -> str:
    if platform.system().lower() != "windows":
        return ""
    try:
        import winreg

        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Microsoft\Cryptography") as key:
            value, _ = winreg.QueryValueEx(key, "MachineGuid")
            return str(value)
    except OSError:
        return ""


def _system_drive_volume_serial() -> str:
    if platform.system().lower() != "windows":
        return ""
    root = os.environ.get("SystemDrive", "C:") + "\\"
    serial = ctypes.c_ulong(0)
    result = ctypes.windll.kernel32.GetVolumeInformationW(
        ctypes.c_wchar_p(root),
        None,
        0,
        ctypes.byref(serial),
        None,
        None,
        None,
        0,
    )
    if not result:
        return ""
    return f"{serial.value:08X}"


def machine_fingerprint_fields() -> dict[str, str]:
    return {
        "guid": _normalize_field(_windows_machine_guid()),
        "volume": _normalize_field(_system_drive_volume_serial()),
        "arch": _normalize_field(os.environ.get("PROCESSOR_ARCHITECTURE") or platform.machine()),
        "cpu": _normalize_field(os.environ.get("PROCESSOR_IDENTIFIER") or platform.processor()),
    }


def canonical_machine_fingerprint(fields: dict[str, str] | None = None) -> str:
    values = fields or machine_fingerprint_fields()
    return (
        f"{MACHINE_FINGERPRINT_VERSION}|"
        f"guid={_normalize_field(values.get('guid'))}|"
        f"volume={_normalize_field(values.get('volume'))}|"
        f"arch={_normalize_field(values.get('arch'))}|"
        f"cpu={_normalize_field(values.get('cpu'))}"
    )


def machine_hash(fields: dict[str, str] | None = None) -> str:
    canonical = canonical_machine_fingerprint(fields)
    return hashlib.sha256(f"{PRODUCT_ID}|{canonical}".encode("utf-8")).hexdigest().upper()


def canonical_license_payload(document: dict[str, Any]) -> str:
    payload: dict[str, Any] = {}
    for field in LICENSE_FIELDS:
        if field not in document:
            raise ValueError(f"license missing required field: {field}")
        payload[field] = document[field]
    return json.dumps(payload, ensure_ascii=False, separators=(",", ":"), sort_keys=True)


def default_public_key() -> ec.EllipticCurvePublicKey:
    numbers = ec.EllipticCurvePublicNumbers(PUBLIC_KEY_V1_X, PUBLIC_KEY_V1_Y, ec.SECP256R1())
    return numbers.public_key()


def load_private_key(path: Path) -> ec.EllipticCurvePrivateKey:
    key = serialization.load_pem_private_key(path.read_bytes(), password=None)
    if not isinstance(key, ec.EllipticCurvePrivateKey):
        raise TypeError("private key must be an ECDSA P-256 PEM key")
    if not isinstance(key.curve, ec.SECP256R1):
        raise TypeError("private key must use secp256r1 / P-256")
    return key


def load_public_key(path: Path) -> ec.EllipticCurvePublicKey:
    key = serialization.load_pem_public_key(path.read_bytes())
    if not isinstance(key, ec.EllipticCurvePublicKey):
        raise TypeError("public key must be an ECDSA P-256 PEM key")
    if not isinstance(key.curve, ec.SECP256R1):
        raise TypeError("public key must use secp256r1 / P-256")
    return key


def write_public_key(path: Path, public_key: ec.EllipticCurvePublicKey) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        public_key.public_bytes(
            serialization.Encoding.PEM,
            serialization.PublicFormat.SubjectPublicKeyInfo,
        )
    )
    return path


def sign_license_document(
    document: dict[str, Any],
    private_key: ec.EllipticCurvePrivateKey,
) -> dict[str, Any]:
    signed = dict(document)
    signed.pop("signature", None)
    payload = canonical_license_payload(signed).encode("utf-8")
    der_signature = private_key.sign(payload, ec.ECDSA(hashes.SHA256()))
    r_value, s_value = utils.decode_dss_signature(der_signature)
    raw_signature = r_value.to_bytes(32, "big") + s_value.to_bytes(32, "big")
    signed["signature"] = base64.b64encode(raw_signature).decode("ascii")
    return signed


def verify_license_signature(
    document: dict[str, Any],
    public_key: ec.EllipticCurvePublicKey | None = None,
) -> bool:
    signature_text = str(document.get("signature", ""))
    try:
        raw_signature = base64.b64decode(signature_text, validate=True)
    except ValueError:
        return False
    if len(raw_signature) != 64:
        return False
    payload_document = dict(document)
    payload_document.pop("signature", None)
    payload = canonical_license_payload(payload_document).encode("utf-8")
    r_value = int.from_bytes(raw_signature[:32], "big")
    s_value = int.from_bytes(raw_signature[32:], "big")
    der_signature = utils.encode_dss_signature(r_value, s_value)
    try:
        (public_key or default_public_key()).verify(der_signature, payload, ec.ECDSA(hashes.SHA256()))
    except InvalidSignature:
        return False
    return True


def build_activation_request(machine_hash: str | None = None) -> dict[str, Any]:
    created_at = isoformat_utc(utc_now())
    hash_value = machine_hash or machine_hash_current()
    request_basis = f"{PRODUCT_ID}|{hash_value}|{created_at}"
    request_id = hashlib.sha256(request_basis.encode("utf-8")).hexdigest().upper()[:24]
    return {
        "productId": PRODUCT_ID,
        "productVersion": PRODUCT_VERSION,
        "requestType": "offline_activation",
        "createdAt": created_at,
        "machineHash": hash_value,
        "machineFingerprintVersion": MACHINE_FINGERPRINT_VERSION,
        "requestId": request_id,
    }


def machine_hash_current() -> str:
    return machine_hash()


def build_license_document(
    request: dict[str, Any],
    license_type: str,
    customer_id: str,
    order_id: str,
    expires_at: str | None,
    issued_at: datetime | None = None,
    features: list[str] | None = None,
) -> dict[str, Any]:
    if license_type not in {"trial", "activated", "buyout"}:
        raise ValueError("license type must be trial, activated, or buyout")
    if license_type != "buyout" and not expires_at:
        raise ValueError("trial and activated licenses require expiresAt")
    if license_type == "buyout":
        expires_at = None
    return {
        "productId": PRODUCT_ID,
        "licenseType": license_type,
        "issuedAt": isoformat_utc(issued_at or utc_now()),
        "expiresAt": expires_at,
        "machineHash": request["machineHash"],
        "customerId": customer_id,
        "orderId": order_id,
        "features": features or list(DEFAULT_FEATURES),
        "keyVersion": KEY_VERSION,
    }


def validate_license_document(
    document: dict[str, Any],
    public_key: ec.EllipticCurvePublicKey | None = None,
    expected_machine_hash: str | None = None,
    now: datetime | None = None,
) -> LicenseValidation:
    try:
        canonical_license_payload({key: document[key] for key in LICENSE_FIELDS})
    except (KeyError, TypeError, ValueError) as exc:
        return LicenseValidation(False, "schema", str(exc))

    if document.get("productId") != PRODUCT_ID:
        return LicenseValidation(False, "product", "productId mismatch")
    if document.get("keyVersion") != KEY_VERSION:
        return LicenseValidation(False, "key", "unsupported keyVersion")
    if document.get("licenseType") not in {"trial", "activated", "buyout"}:
        return LicenseValidation(False, "type", "unsupported licenseType")
    if not isinstance(document.get("features"), list) or "core" not in document["features"]:
        return LicenseValidation(False, "features", "core feature missing")
    if not verify_license_signature(document, public_key):
        return LicenseValidation(False, "signature", "invalid signature")

    expected_hash = expected_machine_hash or machine_hash_current()
    if document.get("machineHash") != expected_hash:
        return LicenseValidation(False, "machine", "machineHash mismatch")

    if document.get("licenseType") != "buyout":
        try:
            expires_at = parse_utc(document.get("expiresAt"))
        except ValueError as exc:
            return LicenseValidation(False, "expiresAt", str(exc))
        if expires_at is None:
            return LicenseValidation(False, "expiresAt", "expiresAt required")
        if (now or utc_now()) > expires_at:
            return LicenseValidation(False, "expired", "license expired")

    return LicenseValidation(True, "ok", "license valid")


def generate_private_key(path: Path) -> ec.EllipticCurvePrivateKey:
    key = ec.generate_private_key(ec.SECP256R1())
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(
        key.private_bytes(
            serialization.Encoding.PEM,
            serialization.PrivateFormat.PKCS8,
            serialization.NoEncryption(),
        )
    )
    return key


def public_key_constants(public_key: ec.EllipticCurvePublicKey) -> str:
    numbers = public_key.public_numbers()
    x_bytes = numbers.x.to_bytes(32, "big")
    y_bytes = numbers.y.to_bytes(32, "big")
    x_values = ", ".join(f"0x{byte:02X}" for byte in x_bytes)
    y_values = ", ".join(f"0x{byte:02X}" for byte in y_bytes)
    return (
        "static const unsigned char DUXUN_LICENSE_PUBLIC_KEY_V1_X[32] = { "
        + x_values
        + " };\n"
        "static const unsigned char DUXUN_LICENSE_PUBLIC_KEY_V1_Y[32] = { "
        + y_values
        + " };"
    )


def copy_license_atomically(source: Path, destination: Path | None = None) -> Path:
    dest = destination or license_path()
    dest.parent.mkdir(parents=True, exist_ok=True)
    temp_path = dest.with_suffix(dest.suffix + ".tmp")
    shutil.copyfile(source, temp_path)
    temp_path.replace(dest)
    return dest


def expires_from_days(days: int) -> str:
    return isoformat_utc(utc_now() + timedelta(days=days))


def add_common_output_arg(parser: argparse.ArgumentParser, default: Path) -> None:
    parser.add_argument("-o", "--output", type=Path, default=default, help=f"Output path, default: {default}")
