import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec

from scripts import duxun_license


class LicenseToolTests(unittest.TestCase):
    def test_canonical_payload_excludes_signature_and_uses_stable_order(self):
        document = {
            "signature": "ignored",
            "productId": duxun_license.PRODUCT_ID,
            "licenseType": "buyout",
            "issuedAt": "2026-06-12T00:00:00Z",
            "expiresAt": None,
            "machineHash": "ABCD",
            "customerId": "cust_1",
            "orderId": "order_1",
            "features": ["core", "export"],
            "keyVersion": "v1",
        }

        payload = duxun_license.canonical_license_payload(document)

        self.assertEqual(
            payload,
            (
                '{"customerId":"cust_1","expiresAt":null,"features":["core","export"],'
                '"issuedAt":"2026-06-12T00:00:00Z","keyVersion":"v1",'
                '"licenseType":"buyout","machineHash":"ABCD","orderId":"order_1",'
                '"productId":"duxun-filmsim-ofx-v5"}'
            ),
        )

    def test_sign_and_verify_license_roundtrip_with_generated_offline_key(self):
        private_key = ec.generate_private_key(ec.SECP256R1())
        public_key = private_key.public_key()
        document = {
            "productId": duxun_license.PRODUCT_ID,
            "licenseType": "buyout",
            "issuedAt": "2026-06-12T00:00:00Z",
            "expiresAt": None,
            "machineHash": "ABCD",
            "customerId": "cust_1",
            "orderId": "order_1",
            "features": ["core", "export"],
            "keyVersion": "v1",
        }

        signed = duxun_license.sign_license_document(document, private_key)

        self.assertIn("signature", signed)
        self.assertTrue(duxun_license.verify_license_signature(signed, public_key))
        signed["orderId"] = "changed"
        self.assertFalse(duxun_license.verify_license_signature(signed, public_key))

    def test_activation_request_contains_product_and_machine_hash(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            output_path = Path(temp_dir) / "activation-request.json"
            request = duxun_license.build_activation_request(machine_hash="A" * 64)
            duxun_license.write_json(output_path, request)
            loaded = json.loads(output_path.read_text(encoding="utf-8"))

        self.assertEqual(loaded["productId"], duxun_license.PRODUCT_ID)
        self.assertEqual(loaded["requestType"], "offline_activation")
        self.assertEqual(loaded["machineHash"], "A" * 64)
        self.assertEqual(loaded["machineFingerprintVersion"], "machine-v1")

    def test_private_keys_are_loaded_from_external_files_not_hardcoded(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            key_path = Path(temp_dir) / "offline-private-key.pem"
            private_key = ec.generate_private_key(ec.SECP256R1())
            key_path.write_bytes(
                private_key.private_bytes(
                    serialization.Encoding.PEM,
                    serialization.PrivateFormat.PKCS8,
                    serialization.NoEncryption(),
                )
            )

            loaded_key = duxun_license.load_private_key(key_path)

        self.assertIsInstance(loaded_key, ec.EllipticCurvePrivateKey)

    def test_verify_cli_accepts_operator_public_key_for_temporary_signing_keys(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            request_path = root / "activation-request.json"
            private_key_path = root / "offline-private-key.pem"
            public_key_path = root / "offline-public-key.pem"
            license_path = root / "license.json"

            request = duxun_license.build_activation_request(machine_hash="B" * 64)
            duxun_license.write_json(request_path, request)
            private_key = duxun_license.generate_private_key(private_key_path)
            public_key_path.write_bytes(
                private_key.public_key().public_bytes(
                    serialization.Encoding.PEM,
                    serialization.PublicFormat.SubjectPublicKeyInfo,
                )
            )

            subprocess.run(
                [
                    sys.executable,
                    "scripts/license_sign_tool.py",
                    "--request",
                    str(request_path),
                    "--private-key",
                    str(private_key_path),
                    "--license-type",
                    "buyout",
                    "--customer-id",
                    "cust_cli",
                    "--order-id",
                    "order_cli",
                    "-o",
                    str(license_path),
                ],
                cwd=Path(__file__).resolve().parents[1],
                check=True,
                capture_output=True,
                text=True,
            )

            result = subprocess.run(
                [
                    sys.executable,
                    "scripts/verify_license_test.py",
                    "--public-key",
                    str(public_key_path),
                    "--machine-hash",
                    "B" * 64,
                    str(license_path),
                ],
                cwd=Path(__file__).resolve().parents[1],
                capture_output=True,
                text=True,
            )

        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("ok: license valid", result.stdout)

    def test_install_cli_rejects_invalid_license_without_overwriting_existing_file(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            request = duxun_license.build_activation_request(machine_hash="C" * 64)
            private_key = ec.generate_private_key(ec.SECP256R1())
            signed = duxun_license.sign_license_document(
                duxun_license.build_license_document(
                    request=request,
                    license_type="buyout",
                    customer_id="cust_install",
                    order_id="order_install",
                    expires_at=None,
                ),
                private_key,
            )
            signed["orderId"] = "tampered"
            invalid_path = root / "invalid-license.json"
            install_path = root / "installed-license.json"
            invalid_path.write_text(json.dumps(signed), encoding="utf-8")
            install_path.write_text("OLD_LICENSE", encoding="utf-8")

            result = subprocess.run(
                [
                    sys.executable,
                    "scripts/install_license.py",
                    "--machine-hash",
                    "C" * 64,
                    "-o",
                    str(install_path),
                    str(invalid_path),
                ],
                cwd=Path(__file__).resolve().parents[1],
                capture_output=True,
                text=True,
            )
            installed_text = install_path.read_text(encoding="utf-8")

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("License rejected", result.stdout)
        self.assertEqual(installed_text, "OLD_LICENSE")


if __name__ == "__main__":
    unittest.main()
