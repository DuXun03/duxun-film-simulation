# Third Party Notices

This project includes or adapts ideas and code from third-party open-source projects.

## NativeEnhancer-FE

- Project: https://github.com/dddfault/NativeEnhancer-FE
- Source revision used for review: `6321e7bd4bb9c957d736fbbf8734899f59db858a`
- License: MIT
- Copyright: Copyright (c) 2020-2022 dddfault
- Additional source credits in the upstream shader include prod80 and other helper authors listed in NativeEnhancer-FE.

The current OFX implementation ports the high-level film grain, film breath, gate weave, halation, and diffusion/bloom shader ideas into CPU C++ code for DaVinci Resolve OpenFX. It does not embed the ReShade runtime.

## glsl-film-grain

- Project: https://github.com/mattdesl/glsl-film-grain
- License: MIT

Reviewed as a reference for natural animated film-grain structure based on 3D noise. The current binary keeps the existing CPU noise path and adds stock/ISO-aware grain profiles rather than embedding upstream GLSL dependencies.

## CineStill 800T Film Simulation by zhirendashu

- Project: https://github.com/zhirendashu/CineStill-800T-Film-Simulation-Zhirendashu-CINESTILL-LAB
- License: no explicit repository license observed during review; user stated direct author authorization has been obtained.

The current OFX implementation uses this project as a target reference for CineStill-style halation behavior: red-channel-biased highlight extraction, thresholded glow, warm red/orange overlay, grain, and LUT ordering. No upstream files are embedded in the current binary yet.

## spectral_film_lut

- Project: https://github.com/JanLohse/spectral_film_lut
- License: MIT

This project is currently a planned reference for negative/print stock color science, printer lights, and future LUT generation. No upstream files are embedded in the current binary yet.

## Natron openfx-misc

- Project: https://github.com/NatronGitHub/openfx-misc
- License: GPL-2.0

The current project uses this repository as a GPL-compatible reference for OpenFX parameter design, color matrix, gamma/color transform, random-noise, transform, and radial/vignette behavior. Direct migration of larger SupportExt-based plugin code is deferred because the current Resolve target is a compact single-file OFX implementation.

## Natron openfx-arena

- Project: https://github.com/NatronGitHub/openfx-arena
- License: GPL-2.0 and LGPL components depending on module

The current project uses this repository as a GPL-compatible reference for texture, spatial disturbance, roll/wave, and film-damage style effects. Heavy ImageMagick/OpenCL/Natron-specific modules are not directly embedded in the current OFX binary.

## OpenFX SDK

- Project: https://github.com/ofxa/openfx
- License: BSD-style license in the OpenFX SDK distribution

The plugin builds against OpenFX headers and follows the OpenFX Image Effect API.

## t3mujinpack Preset Data

- Included derived resources: `presets/film_stocks`
- License: MIT, as recorded in the original data package.

Preset and film-stock data are used as film simulation source material.
