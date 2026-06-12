from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


def read_text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


class OfxLicensingRuntimeTests(unittest.TestCase):
    def test_ofx_source_wires_license_runtime_without_touching_visual_defaults(self):
        source = read_text("ofx/DuXunFilm/DuXunFilmSim.cpp")

        for token in [
            '#include "DuXunLicense.h"',
            "duxun_license::evaluateLocalLicense",
            "duxun_license::writeActivationRequest",
            "grpLicense",
            "licenseStatus",
            "generateActivationRequest",
            "reloadLicense",
            "applyLicenseWatermark",
            "gMessageSuite",
        ]:
            self.assertIn(token, source)

        self.assertIn("kMatFujiSuperia", source)
        self.assertIn("kMatCineStill800T", source)
        self.assertIn("kMatAgfaVista100", source)

    def test_license_runtime_uses_programdata_signed_license_and_public_key_only(self):
        header = read_text("ofx/DuXunFilm/DuXunLicense.h")

        for token in [
            "ProgramData",
            "DuXun\\FilmSim",
            "license.json",
            "activation-request.json",
            "logs\\license.log",
            "BCRYPT_ECDSA_P256_ALGORITHM",
            "BCryptVerifySignature",
            "DUXUN_LICENSE_PUBLIC_KEY_V1_X",
            "DUXUN_LICENSE_PUBLIC_KEY_V1_Y",
            "licenseType",
            "machineHash",
            "signature",
            "keyVersion",
            "trialStartedAt",
            "lastSeenAt",
        ]:
            self.assertIn(token, header)

        self.assertNotIn("PRIVATE KEY", header)
        self.assertNotIn("private_key", header)


if __name__ == "__main__":
    unittest.main()
