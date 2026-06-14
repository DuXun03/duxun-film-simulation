# Quickstart

DuXun Film Simulation 是 DaVinci Resolve 的免费 OpenFX 胶片模拟插件，当前主线为 v5.0，内置 54 个胶片预设。插件在 Resolve 中显示为 `DuXun Film Simulation`，分组为 `DuXun`。

## 1. 准备目录

把本仓库和 OpenFX SDK 放在同一个父目录：

```text
workspace/
├── film-sim-plugin/
└── openfx-sdk/
```

进入插件仓库：

```bat
cd film-sim-plugin
```

## 2. 运行测试

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

关闭 Resolve 后，在管理员终端运行：

```bat
scripts\install.bat
```

安装脚本会创建标准 OFX bundle 目录，并把构建产物复制到：

```text
C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx
```

脚本会输出 build hash 和 installed hash，两个 SHA256 必须一致。不要把 `build\DuXunFilmSim.ofx` 直接复制到 `Plugins` 根目录。

重启 DaVinci Resolve 后，在 Effects Library 中查找：

```text
DuXun -> DuXun Film Simulation
```

卸载本地安装：

```bat
scripts\uninstall.bat
```

## 5. 最小验证

1. 重启 DaVinci Resolve。
2. 打开一个项目和时间线。
3. 把 `DuXun -> DuXun Film Simulation` 拖到一个 clip 上。
4. 选择 Fuji Superia、Agfa Vista 或 CineStill 800T。
5. 调整颗粒、光晕、辉光、暗角或片门抖动，确认画面实时变化。

## 6. 插件没有出现时

先确认 `build\compile_log.txt` 中没有编译失败。

如果编译成功但 Resolve 仍不加载，关闭 Resolve 后重命名这个缓存文件，再重启 Resolve：

```text
%APPDATA%\Blackmagic Design\DaVinci Resolve\Support\OFXPluginCacheV2.xml
```

旧版本 Resolve 也可能使用：

```text
%APPDATA%\Blackmagic Design\DaVinci Resolve\Support\OFXPluginCache.xml
```
