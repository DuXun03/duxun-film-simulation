# DuXun Film Simulation

DaVinci Resolve OpenFX 胶片模拟插件 - v5.0 baseline.

当前主线是 `DuXun Film Simulation` OpenFX 插件。旧的 DCTL 模块、LUT、预设生成脚本仍保留在项目中，作为算法素材和后续 GPU/资源集成来源，但交付目标以 `build/DuXunFilmSim.ofx` 为准。

## 当前状态

- 宿主环境: DaVinci Resolve
- 插件形态: OpenFX image effect
- 插件分组: `DuXun`
- 插件名称: `DuXun Film Simulation`
- 插件 ID: `com.duxun.filmsim`
- 当前版本: v5.0
- 当前内置预设: 54 个 t3mujinpack 胶片预设
- 核心源码: `ofx/DuXunFilm/DuXunFilmSim.cpp`
- 当前构建产物: `build/DuXunFilmSim.ofx`
- 当前安装路径: `C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx`

## 项目结构

```text
film-sim-plugin/
├── README.md
├── QUICKSTART.md
├── build_plugin.bat
├── build/
│   ├── DuXunFilmSim.ofx
│   └── compile_log.txt
├── ofx/
│   ├── BUILD_GUIDE.md
│   └── DuXunFilm/
│       ├── DuXunFilmSim.cpp
│       └── DuXunFilmSim.def
├── scripts/
│   ├── compile_all.bat
│   ├── install.bat
│   ├── uninstall.bat
│   ├── test_ofx_mock.py
│   └── generate_film_luts.py
├── presets/
│   ├── film_stocks/
│   └── idt/
├── dctl/
├── plugin/
└── tests/
    └── test_ofx_baseline.py
```

## 功能范围

v5.0 OpenFX 插件已经把核心使用路径收敛到一个 Resolve 效果中：

- 品牌下拉: Agfa, CineStill, Fuji, Ilford, Kodak, Custom
- 胶片型号下拉: 54 个胶片预设
- 曲线参数: contrast, shoulder, toe
- 色彩参数: density, saturation
- 自定义胶片效果: 动态范围、成片反差、色彩校正、胶片颗粒、光晕、辉光、胶片损伤、胶片呼吸、片门抖动、边缘放大、暗角
- 每个自定义效果都有 Enabled 下拉，可选择“跳过”以完全绕过该模块
- 胶片颗粒、光晕、辉光、损伤、呼吸、片门抖动、暗角支持按胶片预设/感光度或画幅 profile 自动调整
- 边缘放大支持类似 Dehancer Overscan 的画幅比例预设，并保留精细强度滑块
- 输出参数: displayGamma
- 渲染优化: 1024 点 LUT 替代部分逐像素 `powf`

## 构建

在项目根目录运行：

```bat
build_plugin.bat
```

或从脚本目录运行：

```bat
scripts\compile_all.bat
```

两个脚本都会从当前位置推导路径，不再依赖旧的 WorkBuddy 目录。OpenFX SDK 头文件应位于项目同级目录：

```text
2026-05-28-19-53-09/
├── film-sim-plugin/
└── openfx-sdk/
```

## 安装到 Resolve

OpenFX v5.0 是当前主线和交付目标。安装时必须使用标准 OFX bundle 目录，不能把 `.ofx` 直接复制到 `Plugins` 根目录。

```bat
scripts\install.bat
```

安装脚本会创建目录并复制二进制：

```text
C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle\Contents\Win64\DuXunFilmSim.ofx
```

脚本结束时会输出 `build\DuXunFilmSim.ofx` 与已安装文件的 SHA256。两个 hash 必须一致。

然后重启 DaVinci Resolve。插件应出现在 Effects Library 的 `DuXun` 分组下，名称为 `DuXun Film Simulation`。

如果 Resolve 曾经缓存过加载失败状态，需要清理 OFX 插件缓存后再启动 Resolve。之前的排查记录显示，缓存失败状态会让 Resolve 后续启动直接跳过插件加载。

## 视觉基线冻结

第二轮 Resolve A/B 结果已经固化：

- Fuji Superia 100/200/400/800/1600/HG 1600: visual baseline frozen
- Agfa Vista 100/200/400: visual baseline frozen
- CineStill 800T: visual baseline frozen，CPU 与 CUDA halation 路径均使用红橙 leakage，避免白色/黄白 halo。
- CineStill 50D: 保持第一轮 daylight exterior pass 状态，第二轮未重新打开调参。

证据入口：

- `docs/resolve-visual-ab-2026-06-11.md`
- `docs/visual-ab/2026-06-11/`
- `docs/restart-handoff-2026-06-11.md`

## 发布前交付入口

发布前安装包、重装、卸载、Resolve OFX 缓存清理和最小用户验证从这里继续：

```text
docs/release-preflight-2026-06-12.md
```

License MVP beta 交付包已经单独收口，不覆盖无授权 beta 包：

```text
DuXunFilmSim-OFX-v5.0-license-mvp.zip
docs/licensing-mvp-qa-2026-06-12.md
docs/license-key-custody-and-issuance-2026-06-13.md
```

用户侧授权流程只需要生成 `activation-request.json`、接收 `license.json`、运行 `scripts\install_license.bat`，然后在 Resolve 中点击 `Reload License` 或重启 Resolve。签发端 private key custody、buyout 签发、换机重签和拒签策略只记录在内部 docs 中，不进入用户分发包。

当前开发包安装/重装仍使用：

```bat
scripts\install.bat
```

卸载开发安装使用：

```bat
scripts\uninstall.bat
```

清理 Resolve OFX 缓存前必须先关闭 Resolve。本机已确认的缓存路径：

```text
%APPDATA%\Blackmagic Design\DaVinci Resolve\Support\OFXPluginCacheV2.xml
```

## 验证

运行完整单元测试：

```bat
python -m unittest discover -s tests -q
```

该检查会确认：

- 构建脚本不再包含旧 WorkBuddy 绝对路径
- 文档描述的是 OpenFX v5.0 主线
- 安装脚本使用标准 OFX bundle 路径并输出 build/installed hash
- 核心源码元数据仍为 `com.duxun.filmsim`、`DuXun Film Simulation`、v5.0、54 个预设

## 重要历史记录

项目曾从 DCTL 原型推进到 OpenFX 插件。旧 DCTL/LUT package 安装方式不是当前交付主线；`scripts\install.bat` 现在只安装 OpenFX v5.0 bundle。`docs/code-audit-2026-05-28.md` 主要对应早期 DCTL 架构，不能代表当前 v5.0 OpenFX 代码状态。

已解决的关键 OpenFX 加载问题：

- 使用 `/MT` 静态链接，减少运行时 DLL 依赖
- 使用 `.def` 导出 `OfxGetNumberOfPlugins`、`OfxGetPlugin`、`OfxSetHost`，不声明 `LIBRARY`，避免 `.dll` 输出名与 `.ofx` 目标名冲突
- 使用 `kOfxImageEffectPluginApi`
- 支持 Filter 和 General 上下文
- `OfxSetHost(NULL)` 返回 `kOfxStatOK`
- 使用 `/SUBSYSTEM:CONSOLE` 匹配已验证的 PE 特征

## 下一步

当前推荐先把 v5.0 visual baseline、无授权 beta 包和 License MVP beta 包都保持冻结，再进入功能扩展：

1. 发售前复核 `docs/license-key-custody-and-issuance-2026-06-13.md`，确认 actual production private key 的备份、访问控制和签发 ledger。
2. 用 `DuXunFilmSim-OFX-v5.0-license-mvp.zip.sha256` 校验用户包未被改动。
3. 保持 package README / release notes 为用户侧流程，不加入签发端 private key 细节。
4. 后续如需集成 DCTL/LUT 资源，先设计资源包和 runtime 加载方案；这不是当前交付主线。
