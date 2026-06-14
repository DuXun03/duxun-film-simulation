# DuXun Film Simulation

[中文](README.md) | [English](README.en.md)

免费的 DaVinci Resolve OpenFX 胶片模拟插件。

DuXun Film Simulation 把常用胶片外观、颗粒、光晕、辉光、片门运动和印片响应收进一个 Resolve 效果里。插件本体不需要联网、账号、兑换码或额外服务；构建和源码都放在仓库中，适合学习、二次开发和本地调色工作流。

## 亮点

- 54 个内置胶片预设，覆盖 Agfa、CineStill、Fuji、Ilford、Kodak 和自定义入口
- 一体化 OpenFX 效果，不再依赖旧的 DCTL 分散安装方式
- 可调曲线、密度、饱和度、印片强度、胶片颗粒、光晕、辉光、暗角、胶片损伤、胶片呼吸、片门抖动和边缘放大
- 中文参数分组，常用滑块集中在一个效果面板里
- 预设会同步可见的颗粒、光晕、辉光和印片默认值，切换胶片型号时更接近真实工作流
- CPU 路径完整可用，CUDA 路径带保护性 fallback，方便后续继续优化实时性能
- 免费开放，无试用限制、无水印、无后台依赖

## 当前状态

- 宿主环境: DaVinci Resolve
- 插件形态: OpenFX image effect
- 插件分组: `DuXun`
- 插件名称: `DuXun Film Simulation`
- 插件 ID: `com.duxun.filmsim`
- 当前版本: v5.0
- 核心源码: `ofx/DuXunFilm/DuXunFilmSim.cpp`
- 构建产物: `build/DuXunFilmSim.ofx`
- Windows 安装路径: `C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx`

## 普通用户安装

不会编译代码的 Windows 用户，建议直接下载 GitHub Releases 里的预编译安装包：

```text
DuXunFilmSimulation-v5.0-free-windows.zip
```

下载地址：

```text
https://github.com/DuXun03/duxun-film-simulation/releases
```

使用方式：

1. 关闭 DaVinci Resolve。
2. 下载并解压 zip。
3. 双击 `Install.bat`。
4. 如果 Windows 弹出管理员权限确认，选择“是”。
5. 重启 Resolve，在 `OpenFX > DuXun > DuXun Film Simulation` 中找到插件。

安装包内包含中文和英文说明，也提供 `Uninstall.bat`。

## 项目结构

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

## 构建要求

- Windows 10/11
- DaVinci Resolve
- Visual Studio 2022 Build Tools with C++ toolchain
- OpenFX SDK headers in the sibling directory:

```text
2026-05-28-19-53-09/
├── film-sim-plugin/
└── openfx-sdk/
```

## 构建

下面是开发者从源码构建的方式。普通用户请优先使用 Releases 中的一键安装包。

在项目根目录运行：

```bat
build_plugin.bat
```

或：

```bat
scripts\compile_all.bat
```

成功后会生成：

```text
build\DuXunFilmSim.ofx
```

## 安装到 Resolve

先关闭 DaVinci Resolve，然后在管理员终端运行：

```bat
scripts\install.bat
```

安装脚本会复制标准 OpenFX bundle，并输出 build/install SHA256。两个 hash 一致后，重启 Resolve，在 Effects Library 中查找：

```text
DuXun -> DuXun Film Simulation
```

卸载本地安装：

```bat
scripts\uninstall.bat
```

## 验证

运行单元测试：

```bat
python -m unittest discover -s tests -q
```

重点检查：

- 构建脚本使用仓库相对路径
- OpenFX 元数据保持为 `com.duxun.filmsim` / `DuXun Film Simulation` / v5.0
- 免费公开版不包含付费、兑换、在线服务或水印逻辑
- GPU 日志分析、CUDA kernel 文本和主渲染路径仍可静态验证

## 文档

- `QUICKSTART.md`: 本地构建、安装和 Resolve 排查
- `docs/color-science.md`: 胶片曲线、密度、CMY 与光晕模型笔记
- `docs/gpu-acceleration-plan.md`: OpenFX GPU/CUDA 优化路线
- `docs/preset-resource-mapping-2026-06-05.md`: 内置胶片预设与资源映射
- `docs/GITHUB_LISTING.md`: GitHub 仓库简介、topics 和发布文案

## 许可证

本项目以 GPL-2.0-or-later 发布。第三方来源和参考项目记录在 `THIRD_PARTY_NOTICES.md`。
