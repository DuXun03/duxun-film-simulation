from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


def read_text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


class WindowsReleasePackageTests(unittest.TestCase):
    def test_release_packaging_templates_exist(self):
        required_paths = [
            "packaging/windows/install.bat",
            "packaging/windows/uninstall.bat",
            "packaging/windows/README.zh-CN.md",
            "packaging/windows/README.en.md",
            "scripts/package_windows_release.ps1",
        ]

        for path in required_paths:
            self.assertTrue((ROOT / path).is_file(), path)

    def test_installer_targets_standard_openfx_bundle(self):
        installer = read_text("packaging/windows/install.bat")
        for token in [
            "DuXunFilmSim.ofx.bundle",
            "Contents\\Win64\\DuXunFilmSim.ofx",
            "%CommonProgramFiles%\\OFX\\Plugins",
            "Start-Process",
            "robocopy",
            "certutil -hashfile",
            "--dry-run",
        ]:
            self.assertIn(token, installer)

    def test_release_docs_are_bilingual_and_point_to_releases(self):
        readme_zh = read_text("README.md")
        readme_en = read_text("README.en.md")
        guide_zh = read_text("packaging/windows/README.zh-CN.md")
        guide_en = read_text("packaging/windows/README.en.md")

        for text in [readme_zh, readme_en]:
            self.assertIn("DuXunFilmSimulation-v5.0-free-windows.zip", text)
            self.assertIn("https://github.com/DuXun03/duxun-film-simulation/releases", text)
            self.assertIn("Install.bat", text)

        self.assertIn("一键安装包", readme_zh)
        self.assertIn("one-click package", readme_en)
        self.assertIn("关闭 DaVinci Resolve", guide_zh)
        self.assertIn("Close DaVinci Resolve", guide_en)

    def test_packaging_script_writes_expected_zip_and_checksums(self):
        script = read_text("scripts/package_windows_release.ps1")
        for token in [
            "DuXunFilmSimulation-$Version-windows",
            "CHECKSUMS-SHA256.txt",
            "Compress-Archive",
            "Get-FileHash",
            "LICENSE.txt",
        ]:
            self.assertIn(token, script)


if __name__ == "__main__":
    unittest.main()
