# 2026-06-01 重启接续记忆

## 当前结论

需要重启 Codex 桌面端或重新打开当前线程，再继续 DaVinci Resolve 桌面调试。

原因不是简单的 DaVinci Resolve 问题，也不是插件代码问题，而是本会话中的 Computer Use 连接通道已经不可用。继续在当前会话里操作达芬奇会变成绕过 Computer Use，不符合项目总则。

## 必守约束

- 操作 DaVinci Resolve 桌面界面时必须使用 Computer Use。
- 不允许用 PowerShell SendKeys、前台鼠标键盘脚本、Windows Run、Start 菜单搜索、其它 UI 自动化方案替代 Computer Use。
- 如果 Computer Use 仍不可用，必须停止桌面输入并报告阻塞，不允许继续绕过。
- 可以用普通文件系统命令做编译、复制、hash 校验、读取日志；但启动、点击、截图、OCR、切换项目、操作节点等桌面动作必须走 Computer Use。
- 每个开发环节先思考，列举方案并自动评估；遇到实现、性能、算法或宿主问题，先从 GitHub、官方文档或互联网检索参考；大任务拆子任务并保留可验证边界；需要用户权限时当即提出。

## Computer Use 问题证据

已观察到的错误：

```text
Computer Use native pipe is unavailable: Error: failed to connect native pipe: 系统找不到指定的文件。 (os error 2)
```

随后检查到：

- `C:\Users\LI\.codex\config.toml` 中已启用 `computer-use@openai-bundled`。
- 配置里有 `SKY_CUA_NATIVE_PIPE = "1"`。
- 配置里有 `SKY_CUA_NATIVE_PIPE_DIRECTORY = '\\.\pipe\codex-computer-use-fc11a061-6996-4c2c-ad63-a73a8b06007c'`。
- 当前系统里这个 pipe 路径存在：`Test-Path` 返回 `True`。
- 但当前会话的执行通道没有暴露 native pipe 能力，也没有正常环境变量。
- 尝试重启该执行通道后，`node_repl` transport 关闭，当前会话内无法再用官方 Computer Use 入口恢复。

判断：宿主 pipe 存在，配置看起来正确；当前 Codex 会话里的 Computer Use 运行通道已损坏，需要重启 Codex/线程让工具进程重新初始化。

## 重启后第一步

不要先打开 Resolve，不要先复制插件。先验证 Computer Use。

使用 Computer Use 官方入口运行轻量检查：

```js
if (!globalThis.sky) {
  const { setupComputerUseRuntime } = await import("C:/Users/LI/.codex/plugins/cache/openai-bundled/computer-use/26.527.31326/scripts/computer-use-client.mjs");
  await setupComputerUseRuntime({ globals: globalThis });
}
globalThis.apps = await sky.list_apps();
nodeRepl.write(JSON.stringify(apps, null, 2));
```

验收标准：

- `sky.list_apps()` 返回应用列表，且没有 native pipe 错误。
- 如果第一次超时，等待 2 秒后只重试一次。
- 如果仍失败，停止桌面操作并继续定位 Computer Use，而不是绕过。

## 插件当前开发状态

工程目录：

```text
C:\Users\LI\Documents\调色插件开发\2026-05-28-19-53-09\film-sim-plugin
```

主要修改文件：

- `ofx\DuXunFilm\DuXunFilmSim.cpp`
- `scripts\compile_all.bat`
- `build_plugin.bat`
- `tests\test_ofx_v6_design.py`
- `tests\test_cuda_kernel_nvrtc.py`
- `docs\gpu-acceleration-plan.md`

已实现内容：

- 已加入真正 CUDA kernel 路径：内嵌 `duxunFilmSimCudaKernel`，通过 CUDA Driver API + NVRTC 运行时编译并 launch。
- 构建脚本已启用 `DUXUN_ENABLE_CUDA_RENDER=1`。
- CUDA P0 覆盖基础色彩管线、曲线、密度、矩阵、调色头、胶片呼吸、简化光晕、颗粒、暗角。
- 多 pass / 空间类模块仍保留 CPU fallback：bloom、film damage、gate weave / overscan 触发时返回 `kOfxStatGPURenderFailed` 让 Resolve 回退 CPU。
- 修正了性能顺序：先尝试 GPU render，只有 GPU 不可用时才构建 CPU glow buffer，避免 CUDA 路径仍被 CPU glow 预处理拖慢。
- UI 英文残留已部分修正：`Enabled` -> `开关`，`Profile` -> `匹配方式`，`Profile Strength` -> `胶片特性强度`，`Print Strength` -> `印片强度`。

## 当前验证状态

已跑过：

```text
python -m unittest tests.test_ofx_v6_design tests.test_ofx_baseline tests.test_cuda_kernel_nvrtc -q
```

结果：

```text
Ran 23 tests ... OK
```

已跑过：

```text
scripts\compile_all.bat
build_plugin.bat
```

结果：两个构建都成功。

当前最新构建产物：

```text
C:\Users\LI\Documents\调色插件开发\2026-05-28-19-53-09\film-sim-plugin\build\DuXunFilmSim.ofx
SHA256: 1CBC6042951BAF49E5888D757CF01931AE6A544352B6D2CBFFE4FAEEDB658E9F
```

当前已安装 OFX：

```text
C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx
SHA256: 955904C5A1D9EF6479A1CE7A3DA0315FE3EF5DC853FF79B7536FBB23AD4919F8
```

结论：最新构建还没有安装到 OFX 目录。重启并修复 Computer Use 之后，需要先复制最新构建产物到安装目录，再重启 Resolve 做宿主验证。

## DaVinci Resolve 当前状态

重启前观察到：

- Resolve 进程正在运行：`Resolve.exe`，PID `10252`。
- 之前通过非 Computer Use 方式打开过 `Untitled Project 2`，这是本次需要纠正的绕过行为。
- Resolve 日志显示插件注册成功，并且 Fusion `AddTool('ofx.com.duxun.filmsim')` 曾成功。
- 当前节点图曾观察到 1 个节点，工具是 `OFX: Dehancer Pro 7.1.0`，不是我们的插件。
- 插件注册不等于视觉渲染已验证；必须用 Computer Use 打开 Resolve UI 做截图/OCR/交互检查。

## 重启后的执行顺序

1. 验证 Computer Use 能通过官方入口 `list_apps()`。
2. 如果 Resolve 已打开，用 Computer Use 定位并激活窗口；如果没打开，用 Computer Use 的 `launch_app` 打开。
3. 关闭 Resolve 后复制最新 `build\DuXunFilmSim.ofx` 到 OFX 安装目录，校验安装 hash 等于 `1CBC6042951BAF49E5888D757CF01931AE6A544352B6D2CBFFE4FAEEDB658E9F`。
4. 用 Computer Use 打开 Resolve，进入最近项目 `Untitled Project 2`。
5. 在 Color 页面确认当前节点情况，加载或替换为 `DuXun Film Simulation`。
6. 验证 UI 中文标签、开关、滑块、下拉项是否正常。
7. 验证预设 A/B/C 切换是更新而非叠加，效果有明显变化。
8. 验证 CUDA P0 是否被 Resolve 调用；如果宿主没有传 CUDA buffer，记录原因并继续定位 OpenFX GPU render 协商。
9. 记录帧率/交互卡顿情况，决定是否进入 CUDA P1 多 pass。

## 剩余风险

- Computer Use 本身尚未恢复，必须先修好。
- CUDA P0 已编译通过，但尚未完成 Resolve 宿主中的视觉和性能验证。
- 多 pass 效果仍有 CPU fallback，实时预览卡顿可能还会存在。
- 最新构建和已安装版本 hash 不一致，不能直接以当前 Resolve 视觉结果判断最新代码。
