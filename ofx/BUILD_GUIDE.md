# DuXun Film Simulation OpenFX v5.0 编译指南

本文档描述当前可工作的 OpenFX v5.0 baseline。核心源码是 `ofx/DuXunFilm/DuXunFilmSim.cpp`，当前内置 54 个胶片预设，插件名称为 `DuXun Film Simulation`。

## 前置条件

- Windows
- Visual Studio 2022 Build Tools
- OpenFX SDK 头文件
- Python 3，用于运行基线检查

目录布局应为：

```text
2026-05-28-19-53-09/
├── film-sim-plugin/
└── openfx-sdk/
    └── include/
```

## 编译方式

从 `film-sim-plugin` 根目录运行：

```bat
build_plugin.bat
```

或：

```bat
scripts\compile_all.bat
```

两个脚本都会自动推导：

```text
PROJECT_ROOT  = film-sim-plugin
WORKSPACE_ROOT = film-sim-plugin 的上一级目录
OFX_INC = WORKSPACE_ROOT\openfx-sdk\include
SRC = PROJECT_ROOT\ofx\DuXunFilm
OUT = PROJECT_ROOT\build
```

## 当前关键编译参数

```text
/O2
/MT
/LD
/DNOMINMAX
/DWIN32_LEAN_AND_MEAN
/D_CRT_SECURE_NO_WARNINGS
/DEF:ofx\DuXunFilm\DuXunFilmSim.def
/SUBSYSTEM:CONSOLE
```

这些参数来自之前 Resolve 加载失败排查后的稳定组合。不要随意改回 `/MD`，也不要移除 `.def` 导出文件。`.def` 文件只保留 `EXPORTS`，不声明 `LIBRARY`，避免链接器为 `.dll` 名称生成导出信息。

## 输出

```text
build\DuXunFilmSim.ofx
build\compile_log.txt
```

成功时，`compile_log.txt` 应包含：

```text
EXIT_CODE=0
SUCCESS
```

## 安装

```bat
copy build\DuXunFilmSim.ofx "C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx"
```

重启 DaVinci Resolve。插件应出现在：

```text
Effects Library -> DuXun -> DuXun Film Simulation
```

## 基线检查

```bat
python tests\test_ofx_baseline.py
```

检查内容：

- 构建脚本不包含旧 WorkBuddy 绝对路径
- README、QUICKSTART、本文档都描述 OpenFX v5.0 baseline
- 源码仍声明 `com.duxun.filmsim`、`DuXun Film Simulation`、v5.0 和 54 个预设

## 资源说明

`presets/film_stocks/`、`presets/idt/`、`dctl/` 和 `plugin/` 仍是重要素材，但当前 OpenFX v5.0 baseline 尚未把这些资源完整打包进 OFX bundle。后续应先建立资源映射和安装策略，再扩展插件 UI。
