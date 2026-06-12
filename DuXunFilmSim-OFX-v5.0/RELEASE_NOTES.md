# DuXun Film Simulation OpenFX v5.0 Visual Baseline Package

This is the first distributable OpenFX v5.0 package for DuXun Film Simulation after the second Resolve visual A/B pass.

## Package Contents

```text
DuXunFilmSim-OFX-v5.0/
├── README.md
├── QUICKSTART.md
├── LICENSE
├── RELEASE_NOTES.md
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

## Install

1. Close DaVinci Resolve.
2. Open a Command Prompt or PowerShell window at the package root.
3. Run:

```bat
scripts\install.bat
```

4. Confirm `Build hash` and `Installed hash` match.
5. Restart DaVinci Resolve.
6. Find the effect under `DuXun -> DuXun Film Simulation`.

## Uninstall

1. Close DaVinci Resolve.
2. From the package root, run:

```bat
scripts\uninstall.bat
```

3. Type `YES` when prompted.

For unattended local cleanup:

```bat
scripts\uninstall.bat /q
```

## Resolve OFX Cache Cleanup

Use this only when Resolve keeps an old failed load or stale plugin state.

1. Close DaVinci Resolve.
2. Rename or delete:

```text
%APPDATA%\Blackmagic Design\DaVinci Resolve\Support\OFXPluginCacheV2.xml
```

3. If present, also check:

```text
%APPDATA%\Blackmagic Design\DaVinci Resolve\Support\OFXPluginCache.xml
```

4. Restart Resolve and let it rescan OpenFX plugins.

## Frozen Visual Baselines

- Fuji Superia 100/200/400/800/1600/HG 1600: visual baseline frozen.
- Agfa Vista 100/200/400: visual baseline frozen.
- CineStill 800T: visual baseline frozen with red-orange CPU/CUDA halation leakage.

## Known Limits

- CineStill 50D remains at first-round daylight exterior pass status.
- Licensing, trial mode, activation, and buyout are not implemented in this package.
- HDR, OpenCL, Metal, new GPU expansion, and LUT runtime loading are not part of this release package.

## Verification

Use `checksums\SHA256SUMS.txt` to verify package files. The release OFX hash is the authoritative binary identity for this package.

Package smoke test completed on 2026-06-12:

```text
scripts\install.bat
scripts\uninstall.bat /q
scripts\install.bat
```

Final installed OFX SHA256 matches `checksums\SHA256SUMS.txt`:

```text
A508CA29788705D29D78E046FB3DF7D2144C29ED32BBE6CC6662DA2063F48062
```
