# DuXun Film Simulation

[Chinese](README.md) | [English](README.en.md)

A free DaVinci Resolve OpenFX film simulation plugin.

DuXun Film Simulation brings common film looks, grain, halation, glow, gate motion, and print response into one Resolve effect. The plugin does not require internet access, accounts, redeem codes, or background services. Source code and build scripts are kept in this repository for learning, local grading workflows, and further development.

## Highlights

- 54 built-in film stock presets covering Agfa, CineStill, Fuji, Ilford, Kodak, and custom entries
- One integrated OpenFX effect instead of scattered legacy DCTL installation
- Adjustable curve, density, saturation, print strength, grain, halation, glow, vignette, film damage, breathing, gate weave, and edge magnification controls
- Chinese parameter groups with the common sliders gathered in one Resolve effect panel
- Presets synchronize visible grain, halation, glow, and print defaults for a more practical film-selection workflow
- Complete CPU rendering path, with guarded CUDA fallback for future real-time GPU optimization
- Free and open, with no trial limits, no watermark, and no background dependency

## Current Status

- Host: DaVinci Resolve
- Plugin type: OpenFX image effect
- Plugin group: `DuXun`
- Plugin name: `DuXun Film Simulation`
- Plugin ID: `com.duxun.filmsim`
- Current version: v5.0
- Core source: `ofx/DuXunFilm/DuXunFilmSim.cpp`
- Build output: `build/DuXunFilmSim.ofx`
- Windows install path: `C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx`

## Repository Layout

```text
film-sim-plugin/
├── README.md
├── README.en.md
├── QUICKSTART.md
├── CHANGELOG.md
├── LICENSE
├── THIRD_PARTY_NOTICES.md
├── build_plugin.bat
├── ofx/
│   ├── BUILD_GUIDE.md
│   └── DuXunFilm/
│       ├── DuXunFilmSim.cpp
│       └── DuXunFilmSim.def
├── scripts/
│   ├── compile_all.bat
│   ├── install.bat
│   ├── uninstall.bat
│   └── analyze_gpu_log.py
├── presets/
│   ├── film_stocks/
│   └── idt/
├── dctl/
├── plugin/
├── docs/
└── tests/
```

## Build Requirements

- Windows 10/11
- DaVinci Resolve
- Visual Studio 2022 Build Tools with the C++ toolchain
- OpenFX SDK headers in the sibling directory:

```text
2026-05-28-19-53-09/
├── film-sim-plugin/
└── openfx-sdk/
```

## Build

Run from the repository root:

```bat
build_plugin.bat
```

Or:

```bat
scripts\compile_all.bat
```

Successful builds generate:

```text
build\DuXunFilmSim.ofx
```

## Install In Resolve

Close DaVinci Resolve first, then run from an administrator terminal:

```bat
scripts\install.bat
```

The installer copies the standard OpenFX bundle and prints build/install SHA256 values. After the hashes match, restart Resolve and find the effect under:

```text
DuXun -> DuXun Film Simulation
```

Uninstall the local copy with:

```bat
scripts\uninstall.bat
```

## Verification

Run the unit tests:

```bat
python -m unittest discover -s tests -q
```

Key checks cover:

- Build scripts use repository-relative paths
- OpenFX metadata remains `com.duxun.filmsim` / `DuXun Film Simulation` / v5.0
- The free public release contains no paid, redemption, online-service, or watermark logic
- GPU log analysis, CUDA kernel text, and the main rendering path remain statically verifiable

## Documentation

- `QUICKSTART.md`: Local build, installation, and Resolve troubleshooting
- `docs/color-science.md`: Film curve, density, CMY, and halation model notes
- `docs/gpu-acceleration-plan.md`: OpenFX GPU/CUDA optimization roadmap
- `docs/preset-resource-mapping-2026-06-05.md`: Built-in film preset and resource mapping
- `docs/GITHUB_LISTING.md`: GitHub repository description, topics, and release copy

## License

This project is released under GPL-2.0-or-later. Third-party sources and reference projects are recorded in `THIRD_PARTY_NOTICES.md`.
