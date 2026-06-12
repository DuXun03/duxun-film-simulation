# Release preflight - 2026-06-12

## Scope

This is the release-prep entry for DuXun Film Simulation OpenFX v5.0 after the second Resolve visual A/B pass.

- Current delivery target: `build\DuXunFilmSim.ofx`
- Installed bundle target: `C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle`
- Installed binary target: `C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx`
- Do not start HDR, OpenCL, Metal, new GPU expansion, or LUT runtime loading from this release-prep pass.
- Trial, activation, and buyout are design-only follow-ups. Do not implement licensing in this package.

## Visual Baseline

| Stock family | Status | Evidence |
| --- | --- | --- |
| Fuji Superia 100/200/400/800/1600/HG 1600 | Visual baseline frozen | `docs/resolve-visual-ab-2026-06-11.md`, `docs/visual-ab/2026-06-11/` |
| Agfa Vista 100/200/400 | Visual baseline frozen | `docs/resolve-visual-ab-2026-06-11.md`, `docs/visual-ab/2026-06-11/` |
| CineStill 800T | Visual baseline frozen | `docs/resolve-visual-ab-2026-06-11.md`, `docs/visual-ab/2026-06-11/` |
| CineStill 50D | First-round daylight exterior pass only | `docs/resolve-visual-ab-2026-06-09.md`, `docs/visual-ab/2026-06-09/` |

## Verification Snapshot

Fresh pre-commit verification on 2026-06-12:

```text
python -m unittest discover -s tests -q
Ran 56 tests
OK
```

`cmd /c scripts\install.bat` completed successfully and confirmed matching build/installed SHA256:

```text
A508CA29788705D29D78E046FB3DF7D2144C29ED32BBE6CC6662DA2063F48062
```

## Install And Reinstall Flow

Use `scripts\install.bat` for both fresh install and reinstall.

1. Close DaVinci Resolve.
2. Build or stage `build\DuXunFilmSim.ofx`.
3. Run `scripts\install.bat` from the project root, preferably in an Administrator shell.
4. Confirm the script prints matching `Build hash` and `Installed hash` values.
5. Restart DaVinci Resolve.

The script is reinstall-safe for the current v5.0 development package: it creates the standard OFX bundle path if missing, overwrites `Contents\Win64\DuXunFilmSim.ofx`, and fails if the installed hash differs from the build hash.

## Uninstall Flow

Use `scripts\uninstall.bat` to remove the development install.

1. Close DaVinci Resolve.
2. Run `scripts\uninstall.bat` in an Administrator shell.
3. Type `YES` when prompted.
4. Restart Resolve if it was open earlier.
5. If Resolve still lists the plug-in, clear the OFX cache and restart Resolve again.

For automated local cleanup only, `scripts\uninstall.bat /q` skips the prompt. The script removes only:

```text
C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle
```

## Resolve OFX Cache Cleanup

Use cache cleanup only when Resolve keeps an old load failure or stale plug-in state.

1. Close DaVinci Resolve.
2. Rename or delete this cache file if present:

```text
%APPDATA%\Blackmagic Design\DaVinci Resolve\Support\OFXPluginCacheV2.xml
```

3. Also check the older cache name if present:

```text
%APPDATA%\Blackmagic Design\DaVinci Resolve\Support\OFXPluginCache.xml
```

4. Restart Resolve and let it rescan OFX plug-ins.

Prefer renaming the cache file to `.bak` during development so the old cache can be inspected if needed.

## Release Package Layout

Use this minimal package shape for the next release packaging task:

```text
DuXunFilmSim-OFX-v5.0/
├── README.md
├── QUICKSTART.md
├── LICENSE
├── build/
│   └── DuXunFilmSim.ofx
├── scripts/
│   ├── install.bat
│   └── uninstall.bat
├── checksums/
│   └── SHA256SUMS.txt
└── docs/
    ├── release-preflight-2026-06-12.md
    └── resolve-visual-ab-2026-06-11.md
```

`scripts\install.bat` expects `build\DuXunFilmSim.ofx` one directory above `scripts`, so keep that relative layout unless the installer is rewritten.

## Minimal User Verification

After install:

1. Restart DaVinci Resolve.
2. Open a project and go to the Effects Library.
3. Find `DuXun -> DuXun Film Simulation`.
4. Add the effect to a clip.
5. In the effect controls, choose one frozen visual baseline stock such as Fuji Superia, Agfa Vista, or CineStill 800T.
6. Confirm the effect visibly changes the image, then toggle the effect or use a bypass/identity comparison to confirm the host is applying the OFX.
7. For version verification, use the install script output or run `certutil -hashfile` on the installed OFX and compare it with the release `SHA256SUMS.txt`.

## Licensing Follow-Up Draft

Keep licensing out of the current release-prep implementation. The next design pass can specify:

- Trial state storage and reset policy.
- Offline activation request/response shape.
- Buyout entitlement format.
- Failure modes that leave the visual effect usable or clearly bypassed.
- UI copy and support handoff for activation problems.
