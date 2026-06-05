# GPU Acceleration Plan

## 2026-06-05 product decision

GPU expansion is paused. The plugin keeps the existing CUDA implementation that already helps heavy texture/glow/spatial/damage combinations, but remaining hardware acceleration work is no longer the top product priority.

HDR support is removed from the plugin. The previous Rec.2100 PQ/HLG mapping produced incorrect image behavior, and a bad color-management feature is worse for a film-look product than no HDR feature.

The no-HDR build was rebuilt and installed on 2026-06-05. The latest installed build also includes black-and-white defaults, resource-backed matrix mapping refinements, tuned high-value Fuji/Kodak defaults, and differentiated Kodak color-negative defaults. Build and installed OFX SHA256:

```text
63895497913AB771331997953CCDFF45E19DD603A8E6FBABA15A0CD3A1E76290
```

## Current acceleration baseline

- OpenFX GPU Render support is still the host contract: Resolve only passes GPU buffers/queues when the plugin declares the matching GPU capability.
- The OFX plugin declares CPU rendering and CUDA rendering support.
- The build still defines `DUXUN_ENABLE_OPENCL_RENDER` and `DUXUN_ENABLE_CUDA_RENDER`; current Windows builds compile the CUDA path through `DUXUN_ENABLE_CUDA_RENDER=1`.
- Runtime CUDA loading still uses `nvcuda.dll` plus Resolve's bundled NVRTC DLL, so the plugin does not require a local CUDA Toolkit install.
- CUDA covers the current core path: color pipeline, print curve/LUT, density, film matrix, color correction, film breath, grain, vignette, gate weave/overscan spatial sampling, film damage, halation glow, and bloom glow.
- CUDA glow uses separate highlight mask, downsample, separable blur, and composite passes for halation and bloom.
- CPU fallback remains required. If Resolve requests GPU rendering and the CUDA path cannot complete, the plugin returns `kOfxStatGPURenderFailed` so the host can retry on CPU.
- GPU diagnostic logging remains useful for later product decisions through `scripts\analyze_gpu_log.py`.

## Removed scope

- No Rec.2100 PQ transform.
- No Rec.2100 HLG transform.
- No HDR workflow switch.
- No HDR reference white parameter.
- No HDR highlight rolloff parameter.
- No HDR effect compensation parameter.
- No HDR CUDA device helpers or HDR kernel launch arguments.

## Paused GPU work

These items are not current product work:

1. OpenCL P2 for non-NVIDIA GPUs.
2. Metal/macOS acceleration.
3. Additional CUDA equivalence/performance tuning beyond bug fixes.
4. HDR CUDA support.

Resume GPU work only if a real commercial blocker appears, such as broad non-NVIDIA demand, Resolve playback being unusable with existing CUDA/CPU fallback, or measured 4K latency showing GPU work would improve the user-visible product more than look quality, UX, installer, licensing, or preset depth.

## Next product focus

Prioritize plugin value over acceleration breadth:

1. Film-look quality and preset behavior.
2. Dehancer/Filmbox feature parity gaps.
3. Stability and Resolve usability.
4. Installer, licensing, trial/activation, and paid buyout delivery.
5. Documentation and release packaging.

## GPU log command

After Resolve verification, keep the log and analyze only the new lines:

```text
python scripts\analyze_gpu_log.py C:\Users\LI\AppData\Local\Temp\DuXunFilmSim-gpu.log
```
