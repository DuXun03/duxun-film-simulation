# DuXun Film Simulation 快速恢复指南

这是 DaVinci Resolve 的 OpenFX 胶片模拟插件，当前主线版本为 v5.0。插件在 Resolve 中显示为 `DuXun Film Simulation`，分组为 `DuXun`，当前内置 54 个胶片预设。

## 1. 进入项目

```bat
cd C:\Users\LI\Documents\调色插件开发\2026-05-28-19-53-09\film-sim-plugin
```

目录旁边应有 OpenFX SDK：

```text
C:\Users\LI\Documents\调色插件开发\2026-05-28-19-53-09\openfx-sdk
```

## 2. 检查基线

```bat
python -m unittest discover -s tests -q
```

通过时会显示 `OK`。

## 3. 编译 OpenFX 插件

```bat
build_plugin.bat
```

或：

```bat
scripts\compile_all.bat
```

编译产物：

```text
build\DuXunFilmSim.ofx
```

## 4. 安装到 DaVinci Resolve

```bat
scripts\install.bat
```

当前交付主线是 OpenFX v5.0。安装脚本会创建标准 OFX bundle 目录，并把构建产物复制到：

```text
C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx
```

脚本会输出 build hash 和 installed hash，两个 SHA256 必须一致。不要再把 `build\DuXunFilmSim.ofx` 直接复制到 `Plugins` 根目录。

重启 DaVinci Resolve 后，在 Effects Library 中查找：

```text
DuXun -> DuXun Film Simulation
```

重装同样使用 `scripts\install.bat`。脚本会覆盖同一路径的 v5.0 OFX 文件，并重新校验 build/install SHA256。

卸载开发安装使用：

```bat
scripts\uninstall.bat
```

## 5. 如果插件没有出现

先确认 `build\compile_log.txt` 或 `build\build_result.txt` 中没有编译失败。

如果编译成功但 Resolve 仍不加载，检查 Resolve 的 OFX 缓存。此前加载失败时，缓存里可能留下 `status=2` 的失败记录，导致 Resolve 后续启动不再尝试重新加载插件。

关闭 Resolve 后，优先重命名这个缓存文件再重启 Resolve：

```text
%APPDATA%\Blackmagic Design\DaVinci Resolve\Support\OFXPluginCacheV2.xml
```

如果有旧缓存文件，也一并检查：

```text
%APPDATA%\Blackmagic Design\DaVinci Resolve\Support\OFXPluginCache.xml
```

## 6. 最小用户验证

1. 重启 DaVinci Resolve。
2. 打开一个项目和时间线。
3. 在 Effects Library 中找到 `DuXun -> DuXun Film Simulation`。
4. 把效果拖到一个 clip 上。
5. 在效果控制中选择 Fuji Superia、Agfa Vista 或 CineStill 800T。
6. 确认画面有胶片化变化，再通过禁用效果或 identity 对比确认 OFX 正在生效。
7. 版本/二进制验证以 `scripts\install.bat` 输出的 SHA256 为准；发布包阶段再与 `checksums\SHA256SUMS.txt` 对比。

## 7. 当前不要混淆的两条线

- OpenFX v5.0 是当前主线和交付目标。
- 旧 DCTL/LUT package 安装方式不是当前交付主线。
- `dctl/`、`plugin/`、`presets/` 是算法和资源素材，后续可继续整合，但不是当前最小交付路径。
