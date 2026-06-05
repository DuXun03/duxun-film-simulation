# 2026-06-03 接续记录：DuXun Film Simulation

## 读前必守

这是给新对话窗口继续开发用的接力文件。不要从零开始猜项目状态，先读本文件，再读相关源码和测试。

项目总则：

- 宿主环境是 DaVinci Resolve / DaVinci Resolve Studio。
- 操作 Resolve 桌面界面必须使用 Computer Use，不能绕过到 PowerShell SendKeys、Windows Run、Start 菜单搜索或其他 UI 自动化。
- 如果 Computer Use 不可用，先修 Computer Use 或明确阻塞，不要跳过桌面验证。
- 每个开发环节先思考，列举方案并自动评估；遇到实现、性能、算法或宿主问题，先查 GitHub、官方文档或互联网参考。
- 大工程拆成子任务推进，不要因为任务大就跳过。
- 需要用户配合权限时当即提出。
- 改代码前先写可失败的测试；完成前必须跑验证命令并读输出。

## 工作区

项目根目录：

```text
C:\Users\LI\Documents\调色插件开发\2026-05-28-19-53-09\film-sim-plugin
```

OFX 安装路径：

```text
C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx
```

DaVinci Resolve 项目：

```text
Untitled Project 2
```

当前仓库状态：

- 这个目录不是 git repo，不能依赖 `git status` 或分支历史。
- 主要源码在 `ofx\DuXunFilm\DuXunFilmSim.cpp`。
- 主要测试在 `tests\test_ofx_v6_design.py` 和 `tests\test_cuda_kernel_nvrtc.py`。

## 当前已完成

### HDR 工作流

- `hdrEnabled` 已加入 Color Management。
- UI 文案为 `HDR 工作流`，选项为 `跳过` / `启用`。
- 默认值是 `ENABLE_SKIP`，Resolve UI 已确认显示为 `跳过`。
- 只有 `hdrEnabled == ENABLE_ON` 且输入/输出是 PQ/HLG 时，才启用 HDR 胶片工作域映射。
- 关闭时不会默认把 SDR 效果映射到 HDR，避免卡顿和过强效果。

关键源码：

- `defineChoiceParam(paramSet, "hdrEnabled", "HDR 工作流", ...)`
- `getIntParamIfPresent(paramSet, "hdrEnabled", ENABLE_SKIP)`
- `const bool hdrWorkflowEnabled = (hdrEnabled == ENABLE_ON) && ...`

### 自定义 profile 联动

已实现下拉选项联动下方滑块：

- `filmGrainProfile`
- `halationProfile`
- `bloomProfile`
- `filmDamageProfile`
- `filmBreathProfile`
- `gateWeaveProfile`
- `overscanPreset`
- `printStock`
- `filmGrainMode`

注意：不是所有模块都有同一种“匹配方式”。已经去掉了每个模块强行共用同一组分类的做法；只有确实受画幅/介质影响的模块才保留相应下拉。

关键函数：

- `applyFilmGrainProfileDefaults`
- `applyHalationProfileDefaults`
- `applyBloomProfileDefaults`
- `applyDamageProfileDefaults`
- `applyBreathProfileDefaults`
- `applyWeaveProfileDefaults`
- `applyOverscanPresetDefaults`
- `applyPrintStockDefaults`
- `applyFilmGrainModeDefaults`
- `syncCustomProfileSliders`

### 光晕 / 辉光修正

用户反馈：辉光太强、很腻、各胶片预设光晕一样；有些胶片本来不该有明显光晕。

已修正：

- 新增 `StockHalationClass`：
  - `HALATION_STOCK_NONE`
  - `HALATION_STOCK_SUBTLE`
  - `HALATION_STOCK_NO_REMJET`
- `CineStill 800T` / `CineStill 50D` 按 No Remjet 胶片处理，默认有轻量红橙光晕。
- 普通现代彩负/反转片默认无明显光晕，`halationEnabled = ENABLE_SKIP`。
- 少数高感普通胶片只给非常弱的高光边缘反应，例如 `Portra 800`、`Superia 1600`、`Pro 800Z`、`Ultra Max 400`。
- 渲染强度不再叠加旧隐藏参数 `gPresets[idx].halation`，避免不同胶片都变成同款红晕。
- 新增 `stockHalationRenderAmount(...)`，按 stock-specific defaults 和用户自定义值计算渲染强度。
- 自动 profile 遵循胶片默认开关；手动选择 profile 才会启用模块并联动滑块。
- CineStill 800T 已降低强度、提高阈值、减小半径和全局雾化，避免“一点点就很腻”。

关键源码：

- `stockHalationClassForPreset`
- `stockHalationRenderAmount`
- `presetCustomDefaultsForStock`
- `applyHalationProfileDefaults`
- `applyBloomProfileDefaults`
- `applyCineStillHalation`
- `applyHalationControls`
- `float halationRenderBase = stockHalationRenderAmount(...)`

调研依据：

- CineStill 官方说明：Remjet / anti-halation backing 会抑制光晕，CineStill 的移除 remjet 是其红橙光晕来源之一。
- Dehancer Halation 文档：标准乳剂和 No Remjet 胶片的 halation 应区分。
- 结论：No Remjet 默认有光晕；普通防光晕层胶片默认不强行加光晕。

### CUDA P0 状态

当前已实现真正 CUDA kernel 路径：

- 构建脚本已启用 `/DDUXUN_ENABLE_CUDA_RENDER=1`。
- 插件通过 CUDA Driver API + Resolve/NVRTC DLL 运行时编译内嵌 kernel。
- Resolve 宿主中已确认有 CUDA 请求和成功调用，不再只是“编译存在”。

已确认日志路径：

```text
C:\Users\LI\AppData\Local\Temp\DuXunFilmSim-gpu.log
```

注意：shell 的 `%TEMP%` 曾显示为 `C:\TEMPNEW`，但 Resolve 进程实际写入 `C:\Users\LI\AppData\Local\Temp`。以后查 CUDA 日志时优先查这个路径。

关键日志证据：

```text
phase=request reason=received width=3840 height=2160 requested=1 available=1 cuda=1 opencl=0 doBloom=0 doDamage=0 doSpatial=0
phase=cuda_success reason=ok width=3840 height=2160 requested=1 available=1 cuda=1 opencl=0 doBloom=0 doDamage=0 doSpatial=0
```

当前限制：

- `doBloom=0` 的基础路径可以 `cuda_success`。
- `doBloom=1` 会 `cuda_fallback reason=unsupported_effects`。
- 这正是下一步 CUDA P1 要解决的核心问题。

## 最新验证

测试命令：

```text
python -m unittest tests.test_ofx_v6_design tests.test_cuda_kernel_nvrtc -q
```

结果：

```text
Ran 32 tests in 0.234s
OK
```

构建命令：

```text
cmd /c build_plugin.bat
```

结果：

```text
RC=0
BUILD_OK
```

最新构建和安装 hash 均为：

```text
B1AE217258343731FAAF396124B9FEAF21D5F7523DACFCBEB3E239B466B6C86B
```

安装状态：

- `build\DuXunFilmSim.ofx` hash 已匹配安装目录 OFX。
- Resolve `Untitled Project 2` 中已加载 `DuXun Film Simulation`。
- UI 已确认能看到 `CineStill / CineStill 800T`、`HDR 工作流: 跳过`、自定义区。
- 已通过拖动参数触发 OFX render，并在 Resolve 日志中看到 `cuda_success`。

## 当前 Resolve 状态

最后一次操作时：

- Resolve 已打开。
- 当前项目是 `Untitled Project 2`。
- Color 页面右侧设置面板显示 `DuXun Film Simulation`。
- 为触发渲染曾轻微拖动 `对比度` 滑块，随后已拖回接近原值。若新对话要做严格视觉对比，建议先手动或通过 UI reset 恢复该节点参数，再测试。
- 如果要覆盖 OFX DLL，必须先用 Computer Use 正常保存/关闭 Resolve，否则 DLL 会被进程占用。

## 下一步：CUDA P1

目标：把 bloom/halation 的 glow buffer 拆成 GPU 多 pass，减少开启辉光/光晕时的 CPU fallback 和卡顿。

当前根因：

- CPU 路径里 `buildGlowBuffer(...)` 会为 halation/bloom 做高光提取和模糊缓存。
- CUDA P0 只覆盖基础单 pass：色彩、曲线、密度、矩阵、调色头、呼吸、简化光晕、颗粒、暗角等。
- 真正的 bloom/halation glow 需要空间模糊，多 pass 还没迁移到 CUDA。
- `isCudaGpuPathSupported(args)` 目前对 `args.doBloom` 返回 false，因此 bloom 开启时必然 fallback。

推荐 P1 架构：

1. Highlight Mask Pass
   - 输入原图 CUDA buffer。
   - 输出低分辨率 RGB 或单通道 mask。
   - 根据 `GLOW_HALATION` / `GLOW_BLOOM` 使用不同 key：
     - halation：偏红通道和高亮边缘。
     - bloom：高亮区域、`bloomRegion`、`bloomSoftness`。

2. Downsample Pass
   - 1/2、1/4 或按 `previewQuality` 选择分辨率。
   - P1 可先固定 1/4，后续再动态。

3. Separable Blur Pass
   - 横向 blur kernel。
   - 纵向 blur kernel。
   - 半径由 `halationRadius` / `bloomRadius` 映射。
   - 先做单层 Gaussian；不要一开始做复杂多尺度。

4. Composite Pass
   - 将 blurred glow 与主 CUDA 单 pass 输出合成。
   - halation 用红橙 tint 和 defringe。
   - bloom 用 screen blend、detail/save lights/defringe。

5. Fallback 策略
   - 如果任意 CUDA buffer 分配或 kernel launch 失败，返回 `kOfxStatGPURenderFailed` 让 Resolve CPU fallback。
   - 必须记录 `cuda_fallback reason=...`。

## P1 实施建议

先写测试，再改源码：

1. 在 `tests\test_ofx_v6_design.py` 增加测试：
   - 存在 `tryCudaGlowRender` 或类似函数。
   - 存在 `cudaHighlightMaskKernel`、`cudaDownsampleKernel`、`cudaBlurHorizontalKernel`、`cudaBlurVerticalKernel`、`cudaCompositeGlowKernel`。
   - `isCudaGpuPathSupported` 不再因为 `args.doBloom` 直接拒绝。
   - `doDamage`、`doSpatial` 可以继续 fallback，P1 不要一次做太多。

2. 在 `tests\test_cuda_kernel_nvrtc.py` 增加 NVRTC 编译覆盖：
   - 多个 kernel 名称都能从嵌入 CUDA source 中找到。
   - CUDA source 不依赖 CPU-only API。

3. 源码实现顺序：
   - 先重构 `CudaRuntimeState`，支持多个 `DuxunCuFunction`。
   - 再加入 device buffer 分配/释放 API：`cuMemAlloc`、`cuMemcpyDtoD` 或必要的 driver entry points。
   - 然后做 highlight mask + blur + composite。
   - 最后放宽 `isCudaGpuPathSupported` 对 bloom/halation 的限制。

4. 验证顺序：
   - `python -m unittest tests.test_ofx_v6_design tests.test_cuda_kernel_nvrtc -q`
   - `cmd /c build_plugin.bat`
   - 关闭 Resolve 后复制 OFX，校验 hash。
   - 用 Computer Use 打开 Resolve `Untitled Project 2`。
   - 开启/调整 bloom 或 halation，触发 render。
   - 检查 `C:\Users\LI\AppData\Local\Temp\DuXunFilmSim-gpu.log`。

P1 验收标准：

```text
phase=request ... requested=1 available=1 cuda=1 ... doBloom=1 ...
phase=cuda_success reason=ok ... doBloom=1 ...
```

同时，视觉上 bloom/halation 必须只作用于高光区域，不能整体泛白或油腻。

## 不要踩的坑

- 不要说“CUDA 已实现”但只用编译测试证明；必须看 Resolve 日志。
- 不要只为了让日志成功而关闭 bloom/halation；P1 的目标就是让这些模块少 fallback。
- 不要把所有胶片默认开启同样辉光/光晕；No Remjet 和普通防光晕层胶片必须区分。
- 不要把 `PROFILE_AUTO` 当成强制启用模块；自动模式必须尊重 stock defaults。
- 不要扩大到 film damage、gate weave、overscan 的 CUDA 迁移，除非 P1 bloom/halation 已稳定。
- 不要覆盖旧用户改动或误删项目文件；本项目目前不是 git repo，改动前后要自己记录。

## 快速接续命令

进入项目：

```powershell
Set-Location 'C:\Users\LI\Documents\调色插件开发\2026-05-28-19-53-09\film-sim-plugin'
```

跑测试：

```powershell
python -m unittest tests.test_ofx_v6_design tests.test_cuda_kernel_nvrtc -q
```

编译：

```powershell
cmd /c build_plugin.bat
```

查看构建结果：

```powershell
Get-Content .\build\build_result.txt
Get-FileHash -Algorithm SHA256 .\build\DuXunFilmSim.ofx
```

安装前确认 Resolve 是否关闭：

```powershell
Get-Process -Name Resolve -ErrorAction SilentlyContinue
```

复制安装：

```powershell
Copy-Item -LiteralPath '.\build\DuXunFilmSim.ofx' -Destination 'C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx' -Force
```

查看 Resolve CUDA 日志：

```powershell
Get-Content 'C:\Users\LI\AppData\Local\Temp\DuXunFilmSim-gpu.log' -Tail 120
```

## 新对话第一步建议

1. 读本文件。
2. 读 `docs\gpu-acceleration-plan.md`。
3. 读 `ofx\DuXunFilm\DuXunFilmSim.cpp` 中这些区域：
   - GPU logging / CUDA runtime：约 `describeGpuCapabilities` 到 `tryGpuRender`
   - CUDA kernel source：`kCudaKernelSource`
   - glow CPU path：`buildGlowBuffer`、`applyCineStillHalation`、`applyBloomDiffusion`
   - render action：`renderAction`
4. 写 CUDA P1 的失败测试。
5. 再改生产代码。
