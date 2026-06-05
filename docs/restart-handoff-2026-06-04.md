# 2026-06-04 接续记录：DuXun Film Simulation

这是为了切换到新对话继续开发用的接力文档。新对话不要从零猜状态，先读本文，再读源码和测试。

## 必守约束

- 项目根目录：
  `C:\Users\LI\Documents\调色插件开发\2026-05-28-19-53-09\film-sim-plugin`
- 旧对话 ID：
  `019e7858-fdc6-7820-96d2-4a0849bed3c0`
- 旧交接文档：
  `docs\restart-handoff-2026-06-03.md`
- DaVinci Resolve 桌面 UI 操作必须使用 Computer Use，不要用 PowerShell SendKeys、Windows Run、Start 菜单搜索或其它 UI 自动化绕过。
- 如果 Computer Use 不可用，先修 Computer Use 或明确阻塞，不要跳过宿主 UI 验证。
- 用户已说明：需要的权限可以自行发起提权，不用在对话里反复询问；但绝对不能删除项目之外的东西。
- 读取日志时不要删除或清空项目外日志文件。尤其不要清空：
  `C:\Users\LI\AppData\Local\Temp\DuXunFilmSim-gpu.log`
  用行数快照加 `Select-Object -Skip <count>` 读取新增日志。
- 代码修改前按 TDD：先写会失败的测试，再改实现，再跑验证。

## 当前目标

总目标不是只做一个能跑的 OFX，而是做一个有商业竞争力的收费级胶片模拟插件。

当前阶段优先级：

1. 先把硬件加速做完整，尽量减少 CUDA fallback。
2. 再补 Dehancer / Filmbox 级别的功能深度、胶片差异、可靠性和商业化能力。
3. 最后处理授权、安装器、试用限制、售后可诊断日志等收费插件能力。

不要把目标标记为完成；目前仍在 GPU 加速阶段。

## 已完成并验证过的 GPU 进度

### CUDA P0 基础路径

已实现真实 CUDA 渲染路径：

- 通过 CUDA Driver API + Resolve/NVRTC DLL 运行时加载。
- 内嵌 CUDA C kernel 编译为 PTX。
- Resolve 宿主内已出现 `phase=cuda_success`。
- 构建脚本启用 `DUXUN_ENABLE_CUDA_RENDER=1`。

### CUDA P1 glow 多 pass

已完成并安装验证：

- `cudaHighlightMaskKernel`
- `cudaDownsampleKernel`
- `cudaBlurHorizontalKernel`
- `cudaBlurVerticalKernel`
- `cudaCompositeGlowKernel`
- bloom 和 halation 使用独立 glow buffer，不再共用同一份结果。
- CUDA async allocation 已修复 `CUDA_ERROR_INVALID_CONTEXT` 类问题。
- `doHalation=1 doBloom=1 doSpatial=1` 曾在 Resolve 日志中验证为 `cuda_success`。

### CUDA spatial / damage

已完成并安装验证：

- `doSpatial` 不再强制 fallback。
- overscan / gate weave 走 CUDA kernel 内的采样坐标变换。
- film damage 不再强制 fallback。
- CUDA 内部已有 dust / scratch 逻辑：
  - `float dust = duxunNativeNoise((float)x, (float)y + frame * 13.0f, 31.0f)`
  - `float scratchColumn = duxunNativeNoise(floorf((float)x * 0.04f), frame, 57.0f)`
  - scratch 亮线强度已对齐 CPU：`1.0f + 0.80f * fDamage * fFilmScratch`

上一次安装验证的 OFX hash：

```text
A0C83C479B67F3E95C5EEA7F4317D41232D081271E70659E58F506A8F26A4FC3
```

注意：这个 hash 对应的是 film damage CUDA 验证后的已安装版本，不包含本文下面“今天刚改完但还没构建安装”的 HDR CUDA 改动。

## 今天刚完成但尚未安装验证的改动

### HDR CUDA transform 支持

本轮新增的主要改动是让 HDR transform 不再让 CUDA fallback。

已修改文件：

- `ofx\DuXunFilm\DuXunFilmSim.cpp`
- `tests\test_ofx_v6_design.py`

已新增/调整测试：

- `test_hdr_workflow_has_explicit_transforms_and_film_working_mapping`
- `test_cuda_hdr_transforms_are_supported_without_cpu_fallback`

TDD RED 已确认：

```text
python -m unittest tests.test_ofx_v6_design.OfxV6DesignTests.test_cuda_hdr_transforms_are_supported_without_cpu_fallback -q
```

失败原因是：

```text
static bool isCudaGpuPathSupported(const RenderThreadArgs& args) {
    return !isHdrTransform(args.inputTransform) && !isHdrTransform(args.outputTransform);
}
```

已实现的 GREEN 改动：

- `isCudaGpuPathSupported(...)` 现在不再检查 `isHdrTransform`，当前实现为：
  `return true;`
- CUDA raw source 新增设备端 HDR helper：
  - `duxunCudaPqToSceneLinear`
  - `duxunCudaSceneLinearToPq`
  - `duxunCudaHlgToSceneLinear`
  - `duxunCudaSceneLinearToHlg`
  - `duxunCudaApplyHdrFilmWorkingMap`
  - `duxunCudaApplyOutputTransform`
- `duxunCudaApplyInputTransform` 已从只处理 Rec.709 / DWG 扩展到 PQ / HLG。
- `cudaHighlightMaskKernel` 不再 `(void)` 掉 HDR 参数，而是：
  - 用 `duxunCudaApplyInputTransform(inputTransform, hdrReferenceWhite, ...)`
  - 当 `hdrWorkflowEnabled` 时应用 `duxunCudaApplyHdrFilmWorkingMap(...)`
- `duxunFilmSimCudaKernel` 新增参数：
  - `float hdrReferenceWhite`
  - `float hdrHighlightRolloff`
  - `int hdrWorkflowEnabled`
- 主 CUDA kernel 现在用 helper 做输入 transform、HDR film-working map 和输出 transform。
- host 侧 `tryGpuRender` 的 kernel 参数数组已补入：
  - `(void*)&args.hdrReferenceWhite`
  - `(void*)&args.hdrHighlightRolloff`
  - `(void*)&hdrWorkflowEnabled`

已通过的验证：

```text
python -m unittest tests.test_ofx_v6_design.OfxV6DesignTests.test_cuda_hdr_transforms_are_supported_without_cpu_fallback -q
```

结果：

```text
Ran 1 test
OK
```

```text
python -m unittest tests.test_cuda_kernel_nvrtc -q
```

结果：

```text
Ran 2 tests
OK
```

```text
python -m unittest tests.test_ofx_v6_design tests.test_cuda_kernel_nvrtc -q
```

结果：

```text
Ran 40 tests
OK
```

尚未完成：

- 尚未运行 `cmd /c build_plugin.bat` 验证 C++ 主构建。
- 尚未把包含 HDR CUDA 改动的新 OFX 安装到系统插件目录。
- 尚未用 Resolve 日志验证 HDR PQ/HLG 开启时仍是 `cuda_success`。

## 下一个对话第一步

从这里继续，不要重复已经完成的实现。

1. 先确认当前源码测试仍过：

```text
python -m unittest tests.test_ofx_v6_design tests.test_cuda_kernel_nvrtc -q
```

2. 构建：

```text
cmd /c build_plugin.bat
```

3. 如果 Resolve 打开导致插件文件被锁，使用 Computer Use 正常保存并关闭 Resolve，不要用非 Computer Use 的桌面操作绕过。

4. 安装新 OFX：

```text
Copy-Item -LiteralPath "build\DuXunFilmSim.ofx" -Destination "C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx" -Force
```

5. 用 `Get-FileHash` 确认 build 文件和系统 OFX 文件一致。

6. 记录 GPU 日志当前行数，不要清空日志：

```text
if (Test-Path "C:\Users\LI\AppData\Local\Temp\DuXunFilmSim-gpu.log") { (Get-Content "C:\Users\LI\AppData\Local\Temp\DuXunFilmSim-gpu.log").Count } else { 0 }
```

7. 用 Computer Use 打开 Resolve 的 `Untitled Project 2`，在插件参数里打开 HDR 工作流并选择 Rec.2100 PQ 或 Rec.2100 HLG 输入/输出，触发渲染。

8. 读取新增日志，验收目标是出现类似：

```text
phase=request ... cuda=1 ... doHalation=1 doBloom=1 doDamage=1 doSpatial=1
phase=cuda_success reason=ok ... cuda=1 ...
```

并且不应再因为 HDR transform 出现：

```text
phase=cuda_fallback reason=unsupported_effects
```

## 之后的 GPU 缺口

当前 CUDA 路径已覆盖：

- base curve/color/grain/vignette/expand/breath/color head
- bloom / halation glow multipass
- spatial gate weave / overscan
- film damage
- HDR PQ / HLG input/output transform（源码和测试已过，待 build/install/Resolve 验证）

剩余较大的 GPU 平台缺口：

- OpenCL 仍是 `opencl_not_implemented`。
- Metal/macOS 尚未实现。
- HDR CUDA 做完安装验证后，需要更新 `docs\gpu-acceleration-plan.md`，旧文档仍可能写着 film damage/HDR fallback 的过期描述。

## 重要宿主和日志细节

- Resolve 项目名：
  `Untitled Project 2`
- 系统 OFX 安装路径：
  `C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx`
- GPU 日志路径：
  `C:\Users\LI\AppData\Local\Temp\DuXunFilmSim-gpu.log`
- 不要依赖 shell 的 `%TEMP%`，Resolve 进程实际写入的是上面这个用户 Temp 路径。
- 如果需要读新增日志，用“先数行、后 Skip”的方式；不要删除项目外日志。

## Computer Use 当前注意点

新 Computer Use runtime 路径此前可用：

```js
if (!globalThis.sky) {
  const { setupComputerUseRuntime } = await import('C:/Users/LI/.codex/plugins/cache/openai-bundled/computer-use/26.601.21317/scripts/computer-use-client.mjs');
  await setupComputerUseRuntime({ globals: globalThis });
}
```

经验：

- 坐标点击前先调用 `sky.get_window_state(...)`。
- Resolve 会重映射窗口句柄，坐标输入有时用 `sky.list_windows()` 枚举到的原始 `Window` 比 `state.window` 稳。
- 项目管理器里双击项目卡片有时失败；先选中项目卡片，再点右下角“打开”更稳。

## 不要回退的已修问题

- HDR 默认不再自动开启；`hdrEnabled` 默认是跳过。
- 只有用户启用 HDR 工作流且输入/输出 transform 是 PQ/HLG 时才执行 HDR film-working map。
- 自定义模块 profile 已联动下面可见滑块。
- 普通胶片默认不强制开启明显辉光/光晕；CineStill / no-remjet 类胶片才默认有明显红橙 halation。
- 手动选择 glow profile 时，即使当前胶片默认无 glow，也会给非零滑块基准，避免“下拉变了但参数没变”。
- CUDA 前不再先构建 CPU glow buffer。
- build scripts 已修过 `/Fo` 输出到 build 目录，避免 obj 写到项目根。

