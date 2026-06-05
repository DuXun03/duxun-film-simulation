# 2026-06-05 restart handoff: DuXun Film Simulation

## Current direction

The old 2026-06-04 HDR CUDA continuation is superseded. HDR was removed because the mapping was visually wrong. Do not resume the Rec.2100 PQ/HLG task from the older handoff.

Hardware acceleration is no longer the main product track. Keep the existing CUDA implementation, but pause OpenCL, Metal, HDR CUDA, and additional GPU expansion unless a measured commercial blocker appears.

## Verified today

- Removed Rec.2100 PQ/HLG transform choices.
- Removed HDR workflow UI, HDR reference white, HDR highlight rolloff, and HDR effect compensation.
- Removed HDR CPU mapping helpers and render-path compensation.
- Removed HDR CUDA device helpers and kernel launch parameters.
- Updated `docs/gpu-acceleration-plan.md` to reflect paused GPU expansion.
- Added stock-specific black-and-white defaults for fine-grain, classic mid-speed, high-speed, and chromogenic B&W presets.
- Added resource-backed near-neighbor matrices for Fuji C200, Fuji Natura 1600, Fuji Sensia/Provia, Kodak ColorPlus/Gold, and Kodak Ektachrome/Elite Chrome.
- Added tuned defaults and near-neighbor matrix routing for Fuji Pro 800Z, Fuji Astia 100F, Fuji Fortia SP 50, and Kodak Elite Chrome 400.
- Added `docs/preset-resource-mapping-2026-06-05.md` to separate implemented mappings from unresolved calibration work.
- Built and installed the no-HDR OFX binary with the B&W preset and matrix refinements.

Installed OFX SHA256:

```text
B5C9197999EF3271475055EEC431D60AC1F995C10A2E8C15324DB872F25A994D
```

## Verification commands

```text
python -m unittest tests.test_ofx_v6_design tests.test_cuda_kernel_nvrtc tests.test_gpu_log_analyzer -q
python -m unittest discover -s tests -q
cmd /c build_plugin.bat
```

Results:

```text
Ran 44 tests OK
Ran 49 tests OK
build_plugin.bat exit code 0
build/install hash match: B5C9197999EF3271475055EEC431D60AC1F995C10A2E8C15324DB872F25A994D
```

## Activation and UI boundary

Do not automate Resolve activation, licensing, or crack-related UI. If Resolve opens to an activation screen, stop UI automation and continue local code/build verification. Ask the user to manually get Resolve into a usable project if host UI verification is needed.

## Next product work

Focus on commercial plugin value:

1. Improve film-look quality and preset differentiation.
2. Map the embedded 54 presets to the available LUT/preset resources where safe.
3. Tighten Dehancer/Filmbox feature parity: print stock, grain, halation, bloom, film damage, gate weave, overscan, and color controls.
4. After the look pipeline is credible, build installer/licensing/trial/activation.
