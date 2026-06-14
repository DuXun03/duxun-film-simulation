from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read_text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


class OfxBaselineTests(unittest.TestCase):
    def test_build_scripts_are_workspace_relative(self):
        for path in ["build_plugin.bat", "scripts/compile_all.bat"]:
            text = read_text(path)
            self.assertNotIn("WorkBuddy", text)
            self.assertIn("%PROJECT_ROOT%", text)
            self.assertIn("%WORKSPACE_ROOT%", text)
            self.assertIn("openfx-sdk\\include", text)
            self.assertIn("/source-charset:utf-8", text)
            self.assertIn("/execution-charset:utf-8", text)

    def test_build_plugin_resolves_openfx_sdk_from_project_or_worktree(self):
        text = read_text("build_plugin.bat")
        self.assertIn("OFX_INC_PRIMARY", text)
        self.assertIn("OFX_INC_WORKTREE", text)
        self.assertIn("..\\..\\..\\openfx-sdk\\include", text)
        self.assertIn("OFX_SDK_NOT_FOUND", text)

    def test_public_docs_describe_free_github_release_scope(self):
        readme = read_text("README.md")
        listing = read_text("docs/GITHUB_LISTING.md")
        for token in [
            "免费的 DaVinci Resolve OpenFX",
            "54",
            "DuXun Film Simulation",
            "无试用限制",
            "无水印",
        ]:
            self.assertIn(token, readme)
        for token in [
            "Free DaVinci Resolve OpenFX",
            "54 film stocks",
            "no account",
            "source code",
        ]:
            self.assertIn(token, listing)

    def test_docs_describe_current_ofx_v5_baseline(self):
        readme = read_text("README.md")
        build_guide = read_text("ofx/BUILD_GUIDE.md")
        quickstart = read_text("QUICKSTART.md")

        for text in [readme, build_guide, quickstart]:
            self.assertIn("OpenFX", text)
            self.assertIn("v5.0", text)
            self.assertIn("54", text)
            self.assertIn("DuXun Film Simulation", text)

        self.assertNotIn("65", build_guide)
        self.assertNotIn("DCTL 插件 - v2.0", readme)

    def test_install_script_uses_standard_ofx_v5_bundle(self):
        script = read_text("scripts/install.bat")
        for token in [
            "OpenFX v5.0",
            "DuXunFilmSim.ofx.bundle",
            "Contents\\Win64",
            "build\\DuXunFilmSim.ofx",
            "certutil -hashfile",
            "Build hash:",
            "Installed hash:",
        ]:
            self.assertIn(token, script)
        self.assertNotIn("独立胶片模拟_IDT", script)
        self.assertNotIn("DaVinci Resolve Plugin Package", script)

    def test_ofx_source_metadata_matches_documented_baseline(self):
        source = read_text("ofx/DuXunFilm/DuXunFilmSim.cpp")
        exports = read_text("ofx/DuXunFilm/DuXunFilmSim.def")
        self.assertIn('PLUGIN_UID   = "com.duxun.filmsim"', source)
        self.assertIn('PLUGIN_LABEL = "DuXun Film Simulation"', source)
        self.assertRegex(source, r"pluginVersionMajor\s*=\s*5")
        self.assertRegex(source, r"pluginVersionMinor\s*=\s*0")
        self.assertRegex(source, r"gNumPresets\s*=\s*54")
        self.assertIn("kOfxImageEffectPluginApi", source)
        self.assertNotRegex(exports, r"(?m)^LIBRARY\b")


if __name__ == "__main__":
    unittest.main()
