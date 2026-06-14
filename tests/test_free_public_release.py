from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class FreePublicReleaseTests(unittest.TestCase):
    def test_paid_and_licensing_modules_are_not_shipped(self):
        removed_paths = [
            "portal",
            "portal-data",
            "DuXunFilmSim-OFX-v5.0-license-mvp",
            "DuXunFilmSim-OFX-v5.0-license-mvp.zip",
            "DuXunFilmSim-OFX-v5.0-license-mvp.zip.sha256",
            "DuXunFilmSim-OFX-v5.0",
            "DuXunFilmSim-OFX-v5.0.zip",
            "DuXunFilmSim-OFX-v5.0.zip.sha256",
            "t3mujinpack-data",
            "ofx/DuXunFilm/DuXunLicense.h",
            "scripts/duxun_license.py",
            "scripts/generate_activation_request.py",
            "scripts/generate_activation_request.bat",
            "scripts/install_license.py",
            "scripts/install_license.bat",
            "scripts/license_sign_tool.py",
            "scripts/verify_license_test.py",
            "scripts/resolve_visual_ab_plan.py",
            "scripts/resolve_export_ab_once.py",
            "tests/test_license_tools.py",
            "tests/test_licensing_design.py",
            "tests/test_ofx_licensing_runtime.py",
            "tests/test_portal_cdk_flow.py",
            "docs/licensing-trial-design-2026-06-12.md",
            "docs/licensing-mvp-qa-2026-06-12.md",
            "docs/license-key-custody-and-issuance-2026-06-13.md",
            "docs/beta-customer-test-guide-2026-06-13.md",
            "docs/beta-feedback-template-2026-06-13.md",
            "docs/beta-operator-ledger-template-2026-06-13.md",
            "docs/beta-operator-runbook-2026-06-13.md",
            "docs/beta-release-readiness-2026-06-13.md",
            "docs/beta-support-drill-2026-06-13.md",
            "docs/beta-support-drill-result-2026-06-13.md",
            "docs/superpowers/plans/2026-06-13-cdk-purchase-portal.md",
        ]

        for path in removed_paths:
            self.assertFalse((ROOT / path).exists(), path)

    def test_ofx_source_has_no_runtime_paywall_or_watermark_hooks(self):
        source = (ROOT / "ofx/DuXunFilm/DuXunFilmSim.cpp").read_text(encoding="utf-8")

        blocked_tokens = [
            'DuXunLicense.h',
            "defineLicenseParams",
            "grpLicense",
            "licenseStatus",
            "activateCdk",
            "Generate Activation Request",
            "reloadLicense",
            "evaluateLocalLicense",
            "applyLicenseWatermark",
            "license_watermark_requires_cpu",
        ]

        for token in blocked_tokens:
            self.assertNotIn(token, source)

    def test_public_docs_do_not_describe_paid_distribution(self):
        text = (ROOT / "README.md").read_text(encoding="utf-8")
        blocked_tokens = [
            "CDK",
            "Stripe",
            "Payment",
            "payment",
            "checkout",
            "activation-request",
            "install_license",
            "license-mvp",
            "buyout",
            "trial",
            "watermark",
        ]

        for token in blocked_tokens:
            self.assertNotIn(token, text)


if __name__ == "__main__":
    unittest.main()
