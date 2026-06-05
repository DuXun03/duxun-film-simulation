# OFX v5 Baseline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the DuXun Film Simulation OFX v5.0 project build, document, and verify from the current workspace without depending on the old WorkBuddy path.

**Architecture:** Keep the current single-file OFX v5.0 implementation as the source of truth. Add lightweight repository checks that validate build scripts, documentation, and plugin metadata stay aligned.

**Tech Stack:** Windows batch scripts, C++ OpenFX source, Python baseline tests, Markdown documentation.

---

### Task 1: Baseline Verification Script

**Files:**
- Create: `tests/test_ofx_baseline.py`

- [ ] **Step 1: Write the failing baseline tests**

```python
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read_text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def test_build_scripts_are_workspace_relative():
    for path in ["build_plugin.bat", "scripts/compile_all.bat"]:
        text = read_text(path)
        assert "WorkBuddy" not in text
        assert "%PROJECT_ROOT%" in text
        assert "%WORKSPACE_ROOT%" in text
        assert "openfx-sdk\\include" in text
        assert "/source-charset:utf-8" in text


def test_docs_describe_current_ofx_v5_baseline():
    readme = read_text("README.md")
    build_guide = read_text("ofx/BUILD_GUIDE.md")
    quickstart = read_text("QUICKSTART.md")

    for text in [readme, build_guide, quickstart]:
        assert "OpenFX" in text
        assert "v5.0" in text
        assert "54" in text
        assert "DuXun Film Simulation" in text

    assert "65" not in build_guide
    assert "DCTL 插件 - v2.0" not in readme


def test_ofx_source_metadata_matches_documented_baseline():
    source = read_text("ofx/DuXunFilm/DuXunFilmSim.cpp")
    assert 'PLUGIN_UID   = "com.duxun.filmsim"' in source
    assert 'PLUGIN_LABEL = "DuXun Film Simulation"' in source
    assert re.search(r"pluginVersionMajor\s*=\s*5", source)
    assert re.search(r"pluginVersionMinor\s*=\s*0", source)
    assert re.search(r"gNumPresets\s*=\s*54", source)
    assert "kOfxImageEffectPluginApi" in source
    assert not re.search(r"(?m)^LIBRARY\b", read_text("ofx/DuXunFilm/DuXunFilmSim.def"))
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `python tests/test_ofx_baseline.py`

Expected: FAIL because the current scripts still contain `WorkBuddy`, lack `/source-charset:utf-8`, and docs still describe the older DCTL v2.0 or 65-preset plan.

### Task 2: Workspace-Relative Build Scripts

**Files:**
- Modify: `build_plugin.bat`
- Modify: `scripts/compile_all.bat`

- [ ] **Step 1: Update both scripts to derive paths from their own location**

Use `%~dp0` in `build_plugin.bat` and `%~dp0..` in `scripts/compile_all.bat`. Set:

```bat
set "PROJECT_ROOT=%~dp0"
set "WORKSPACE_ROOT=%PROJECT_ROOT%.."
set "OFX_INC=%WORKSPACE_ROOT%\openfx-sdk\include"
set "SRC=%PROJECT_ROOT%ofx\DuXunFilm"
set "OUT=%PROJECT_ROOT%build"
```

For `scripts/compile_all.bat`, normalize the script directory first:

```bat
set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..") do set "PROJECT_ROOT=%%~fI\"
for %%I in ("%PROJECT_ROOT%..") do set "WORKSPACE_ROOT=%%~fI"
```

- [ ] **Step 2: Preserve the known-good compiler and linker flags**

Keep `/O2 /MT /LD`, `/DEF:"%SRC%\DuXunFilmSim.def"`, and `/SUBSYSTEM:CONSOLE`.

- [ ] **Step 3: Run baseline tests**

Run: `python tests/test_ofx_baseline.py`

Expected: docs checks still fail, script checks pass.

### Task 3: OFX v5 Documentation Alignment

**Files:**
- Modify: `README.md`
- Modify: `QUICKSTART.md`
- Modify: `ofx/BUILD_GUIDE.md`

- [ ] **Step 1: Rewrite README around the OFX v5.0 mainline**

State that the current deliverable is the Resolve OpenFX plugin, with the legacy DCTL pack kept as supporting material.

- [ ] **Step 2: Rewrite QUICKSTART for handoff**

Give the exact current paths, build command, output file, install destination, and Resolve cache note.

- [ ] **Step 3: Rewrite BUILD_GUIDE to match the current one-file build path**

Document `build_plugin.bat` and `scripts\compile_all.bat`, 54 embedded presets, and the optional 65 LUT resources as future integration material rather than current UI data.

- [ ] **Step 4: Run baseline tests**

Run: `python tests/test_ofx_baseline.py`

Expected: PASS.

### Task 4: Verification

**Files:**
- Read: `build/compile_log.txt`
- Read: `build/DuXunFilmSim.ofx`

- [ ] **Step 1: Run Python baseline tests**

Run: `python tests/test_ofx_baseline.py`

Expected: `Ran 3 tests` and `OK`.

- [ ] **Step 2: Run compile script if Visual Studio Build Tools are available**

Run: `scripts\compile_all.bat`

Expected: `SUCCESS: DuXunFilmSim.ofx created` and `build/compile_log.txt` includes `EXIT_CODE=0`.

- [ ] **Step 3: If compile cannot run, report the exact blocker**

Acceptable blocker: Visual Studio Build Tools environment script is missing on this machine.
