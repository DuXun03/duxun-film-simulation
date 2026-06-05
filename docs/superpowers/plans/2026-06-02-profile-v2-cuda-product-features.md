# Profile System v2 + CUDA Logs Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the generic profile dropdown problem, add CUDA call/fallback logging, and land the product-research feature set as real controls with render wiring.

**Architecture:** Keep this release inside the current OFX plugin: descriptor controls, preset reset logic, CPU render path, and CUDA P1 path all live in `ofx/DuXunFilm/DuXunFilmSim.cpp`. Python tests assert the wiring and CUDA kernel surface.

**Tech Stack:** C++17-ish OFX plugin, embedded CUDA/NVRTC kernel source, Python `unittest`, existing Windows batch build.

---

## Tasks

- [ ] Add failing tests for Profile System v2:
  - module-specific profile helpers replace `defineStockProfileParam`;
  - only film-dependent modules get profile choices;
  - bloom and film breath get their own meaningful profile menus.
- [ ] Add failing tests for CUDA diagnostics and P1 coverage:
  - GPU request/success/fallback events are written to a temp log;
  - fallback reasons are explicit;
  - CUDA kernel includes spatial sampling and basic damage tokens.
- [ ] Add failing tests for the product feature controls:
  - grain tone/chroma/resolution controls;
  - halation local/global/hue/blue-suppression controls;
  - bloom detail/highlight-preserve/defringe controls;
  - print stock, push/pull, acutance, color density, vibrance, printer lights, split tone, and effect mode.
- [ ] Implement Profile System v2:
  - remove generic stock profile helper;
  - add module-specific choice helpers and defaults;
  - map choices to per-module gains instead of one shared list.
- [ ] Implement new render controls:
  - define descriptor controls, read them during render, add them to `RenderThreadArgs`;
  - wire CPU helpers into the render order without changing unrelated host contracts.
- [ ] Implement CUDA call logs and P1 expansion:
  - add bounded `%TEMP%/DuXunFilmSim-gpu.log` logging;
  - return clear failure reasons from `tryGpuRender`;
  - support spatial sampling and simple film damage in the CUDA path; keep true bloom blur on CPU fallback until a multi-pass CUDA buffer exists.
- [ ] Verify:
  - run design and CUDA kernel tests;
  - run plugin build;
  - install or stage the built OFX bundle if build succeeds;
  - use DaVinci/Computer Use only after local verification if plugin install needs UI validation.
