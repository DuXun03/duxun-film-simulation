from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


def read_text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


class LicensingDesignTests(unittest.TestCase):
    def test_trial_activation_buyout_design_doc_covers_required_product_rules(self):
        text = read_text("docs/licensing-trial-design-2026-06-12.md")

        for token in [
            "24 小时 trial",
            "离线 challenge / response",
            "buyout",
            "永久授权",
            "machineHash",
            "换机",
            "重置",
        ]:
            self.assertIn(token, text)

    def test_design_doc_defines_local_layout_schema_security_and_tools(self):
        text = read_text("docs/licensing-trial-design-2026-06-12.md")

        for token in [
            "%PROGRAMDATA%\\DuXun\\FilmSim\\license.json",
            "activation-request.json",
            "logs\\license.log",
            "ProgramData",
            "productId",
            "licenseType",
            "issuedAt",
            "expiresAt",
            "customerId",
            "orderId",
            "features",
            "signature",
            "keyVersion",
            "public key",
            "私钥",
            "Microsoft SignTool",
            "generate_activation_request.bat",
            "install_license.bat",
            "license_sign_tool.py",
            "verify_license_test.py",
        ]:
            self.assertIn(token, text)

    def test_design_doc_keeps_current_visual_and_gpu_boundaries(self):
        text = read_text("docs/licensing-trial-design-2026-06-12.md")

        for token in [
            "不启动 HDR",
            "不启动 OpenCL",
            "不启动 Metal",
            "不改现有视觉默认值",
            "不改 matrix",
        ]:
            self.assertIn(token, text)


if __name__ == "__main__":
    unittest.main()
