# 开源效果借鉴清单

检索日期：2026-05-30

目标：为当前 DaVinci Resolve OFX 胶片模拟插件补齐类似 Dehancer / FilmBox 的功能层级，包括扩展、印片、调色头、胶片颗粒、光晕、辉光、胶片损伤、胶片呼吸、片门抖动、过扫描、暗角等模块。

## 结论

授权状态更新：用户已联系相关作者并取得授权；MIT 项目可直接移植，GPL 项目按 GPL 兼容方式移植和发布。

优先路线不是直接拼接大量第三方库，而是：

1. 先把可控、许可清晰、效果明显的 shader 算法移植成 Resolve 友好的 OFX C++ 实现。
2. GPL 项目可以移植，但必须同步 GPL 兼容声明、源码获取说明和第三方声明。
3. 色彩管理后续再接 OpenColorIO / ACES；当前阶段先用轻量内置转换和明确的输入/输出选项保证 Resolve 中能实时调试。

## 候选项目

| 项目 | 许可 | 适合借鉴的模块 | 判断 |
| --- | --- | --- | --- |
| [dddfault/NativeEnhancer-FE](https://github.com/dddfault/NativeEnhancer-FE) | MIT | 胶片颗粒、光晕、扩散/辉光、胶片呼吸、片门抖动、LUT 胶片外观 | 优先。许可友好，功能和我们要做的“胶片瑕疵层”高度重合。建议先移植算法思路，不引入 ReShade 运行时。 |
| [NatronGitHub/openfx-misc](https://github.com/NatronGitHub/openfx-misc) | GPL-2.0 | OFX 参数组织、ColorMatrix、ColorTransform、Gamma、Transform 等插件结构 | 可以按 GPL 兼容方式选择性移植。优先移植小型无重依赖算法，不直接引入 Natron SupportExt 架构。 |
| [NatronGitHub/openfx-arena](https://github.com/NatronGitHub/openfx-arena) | GPL-2.0 / 部分 LGPL | Wave、Roll、Texture、Polaroid 等空间扰动和纹理类效果 | 主要做效果参考。部分模块依赖 ImageMagick/OpenCL 或使用 Resolve 不支持的 OFX 特性，直接集成优先级低。 |
| [martymcmodding/qUINT](https://github.com/martymcmodding/qUINT) | 未见开放式复制许可，README 标注 all rights reserved | Bloom、Lightroom 风格调色、Deband、Sharpen | 只读算法思路，不复制代码。Bloom 可以自行实现 separable blur / threshold glow。 |
| [crosire/reshade-shaders](https://github.com/crosire/reshade-shaders) | 仓库页未显示清晰统一许可 | ReShade FX 写法、LUT、通用后期处理结构 | 只做参考。若以后引用单个文件，必须逐文件确认许可。 |
| [AcademySoftwareFoundation/OpenColorIO](https://github.com/AcademySoftwareFoundation/OpenColorIO) | BSD-3-Clause | 专业色彩管理、LUT、ACES 兼容配置 | 中期方向。依赖较重，适合在插件稳定后作为准确色彩管理后端。 |
| [aces-aswf/aces-core](https://github.com/aces-aswf/aces-core) | Apache-2.0 | ACES 核心变换、tonescale、gamut compress、矩阵和传递函数 | 中期方向。适合补齐 ACES / scene-linear 工作流，不适合作为当前 UI 占位阶段的第一步。 |

## 功能映射

| 我们的中文模块 | 近期可借鉴来源 | 第一版实现建议 |
| --- | --- | --- |
| 扩展 | 自研 + ACES/OpenColorIO 参考 | 先做黑白点扩展、toe/shoulder 强度和软裁切；后续换成更完整的密度域模型。 |
| 印片 | ACES、OpenColorIO、Natron ColorTransform 参考 | 先做 printStrength、contrast、tone scale；后续加入印片 LUT 或密度曲线。 |
| 调色头 | 自研 | 做 CMY 减色三滑块，工作在对数/密度近似域，不用普通 RGB 加减。 |
| 胶片颗粒 | NativeEnhancer-FE | 先做亮度相关颗粒，强度、尺寸、彩色/单色、随机种子；后续加 temporal grain。 |
| 光晕 | NativeEnhancer-FE | 先做高亮阈值 + 红橙偏色扩散；CPU 版先小半径，后续优化为多 pass 模糊。 |
| 辉光 | NativeEnhancer-FE、qUINT 参考 | 自研 threshold bloom，避免复制 qUINT 代码。 |
| 胶片损伤 | NativeEnhancer-FE 纹理思路 + openfx-arena Texture 参考 | 第一版用程序噪声生成灰尘/划痕开关和强度，不依赖外部贴图。 |
| 胶片呼吸 | NativeEnhancer-FE | 做按时间变化的曝光、色温、饱和度微抖动。 |
| 片门抖动 | NativeEnhancer-FE + openfx-arena Roll/Wave 参考 | 做亚像素平移和轻微旋转；需要安全采样边界。 |
| 过扫描 | 自研 + openfx-arena Transform 参考 | 做 scale/crop，配合片门抖动隐藏边缘。 |
| 暗角 | 自研 | 做中心、半径、羽化、强度；可立即落地。 |

## 下一步实现顺序

P0：先完成产品表面和可测试路径

- 中文自定义区保持为分组滑块。
- 让所有滑块变动立即触发重新渲染。
- 继续用 Resolve 项目 `Untitled Project 2` 做主机加载测试。

P1：先做“看得见”的三类真实效果

- 暗角：低风险，最容易确认实时反馈。
- 胶片颗粒：参考 NativeEnhancer-FE，做亮度相关颗粒。
- 光晕/辉光：做阈值提取和小半径扩散，让高光有胶片感。

P2：再做“胶片运动感”

- 胶片呼吸：随时间轻微改变曝光/色温/饱和度。
- 片门抖动 + 过扫描：先做平移，后续加旋转。
- 胶片损伤：从程序灰尘/划痕开始，不先引入贴图库。

P3：最后升级色彩科学

- 把当前内置输入/输出转换替换为 OpenColorIO 或 ACES 变换。
- 为每个胶片预设建立明确的输入工作空间、胶片矩阵、印片曲线和输出显示变换。
- 只有在许可确认后才引入第三方代码；否则保留“参考思路，自研实现”。

## 许可边界

- MIT 项目可以引用或移植，但需要保留版权和许可证声明。
- GPL 项目可以移植，前提是整个插件按 GPL 兼容方式发布，并在 `THIRD_PARTY_NOTICES.md` 中记录来源和授权。
- README 标注 all rights reserved 或没有明确 license 的 shader，只能作为效果观察和算法启发。
- 如果后续从 NativeEnhancer-FE 直接移植任何函数，需要新增 `THIRD_PARTY_NOTICES.md` 并记录来源文件、作者和 MIT license。

## 2026-05-30 进一步调研更新

用户已说明已联系相关作者并取得授权；MIT 项目可直接移植，GPL 项目按 GPL 兼容发布策略移植。当前实现已经先把 NativeEnhancer-FE 风格的颗粒、呼吸、片门抖动、光晕、辉光思路移植为 CPU OFX 版本，后续应按下列优先级推进。

| 优先级 | 项目 | 许可状态 | 覆盖模块 | 可移植点 |
| --- | --- | --- | --- | --- |
| P0 | [zhirendashu/CineStill-800T-Film-Simulation-Zhirendashu-CINESTILL-LAB](https://github.com/zhirendashu/CineStill-800T-Film-Simulation-Zhirendashu-CINESTILL-LAB) | 仓库未见显式开源 License；按用户取得的作者授权处理 | CineStill 800T look、halation、grain、3D LUT | `src/halo.py` 的红通道高光 mask、阈值压缩、Gaussian blur、红橙/品红叠加，以及 LUT 管线。当前 OFX 已先做视频帧实时版的共享 glow buffer，下一步接入该项目的 LUT/参数范围。 |
| P0 | [dddfault/NativeEnhancer-FE](https://github.com/dddfault/NativeEnhancer-FE) | MIT | halation、diffusion/bloom、film breath、gate weave、film grain、3D LUT atlas | 已是实时 shader 管线，适合作为 GPU/多 pass 架构蓝本。当前 CPU 版只移植效果模型；下一步应把 blur 和 LUT sampler 拆成 GPU kernel。 |
| P0 | [JanLohse/spectral_film_lut](https://github.com/JanLohse/spectral_film_lut) | MIT | negative/print stock、printer lights、color head、film LUT | 适合作为 Film Stock / Print Stock / Color Head 的颜色科学核心。建议先离线烘焙 LUT，再接实时 3D LUT sampler。 |
| P1 | [mltframework/mlt oldfilm](https://github.com/mltframework/mlt/tree/master/src/modules/oldfilm) | LGPL-2.1 | dust、scratch/lines、flicker、jitter、grain、vignette | 适合补胶片损伤模块，先移植随机灰尘、竖线划痕、亮度抖动和 jitter 的参数结构。 |
| P1 | [wavequant/cinesuite](https://github.com/wavequant/cinesuite) | MIT | halation、grain、dust、vignette、light leaks | Python/OpenCV 原型，适合校准 UI 参数范围和 CPU 参考结果，不作为主架构。 |
| P1 | [andlgr/py-digilation](https://github.com/andlgr/py-digilation) | MIT | halation | 红通道高光 mask + 多尺度 Gaussian blur，可作为 CineStill halation 的备用对照。 |
| P1 | [Godot Creative Cinema / Film Framing](https://godotshaders.com/shader/creative-cinema-film-framing/) | CC0 | gate weave、projector flicker、framing、overscan、vignette | 页面 shader 代码可自由使用，适合补片门抖动、过扫描和投影机闪烁。 |
| P2 | [rajawski/emula](https://github.com/rajawski/emula) | MIT | film profiles、tone curves、grain、halation、vignette | 单文件 OpenCV 实现，参数结构可参考，颜色核心优先级低于 spectral_film_lut。 |
| P2 | [Godot Old Movie Shader](https://godotshaders.com/shader/old-movie-shader/) | CC0 | dust/speckles、scratch lines、projector flicker、vignette | 适合做 stylized/lo-fi 胶片损伤预设。 |
| P3 | [CarVac/filmulator-gui](https://github.com/CarVac/filmulator-gui) | GPL-3.0 | development simulation、tone response | 物理启发价值高，但工程复杂且 GPL-3.0 传染边界更重，作为长期研究参考。 |

### 移植顺序

1. NativeEnhancer-FE：先稳定实时效果架构，覆盖 grain、halation、bloom、breath、gate weave。
2. spectral_film_lut：建立负片、印片、Printer Lights 和 Film Stock 颜色核心。
3. zhirendashu CineStill：在授权前提下做 CineStill 800T 专用 preset，包括 halation、grain、LUT 和参数命名。
4. MLT oldfilm + Godot CC0 shaders：补 dust、scratch、flicker、overscan、vignette、damage。
5. cinesuite、py-digilation、emula：作为参数范围和 CPU 参考实现，不作为主架构。

### GPU 加速路线

OpenFX GPU 不是简单开关。Resolve 可通过 OpenFX GPU Render 扩展给插件传 GPU buffer/queue，但插件必须声明 GPU 支持、包含 GPU 扩展头，并实现对应 OpenCL/CUDA/Metal 或 OpenGL 路径，同时保留 CPU fallback。短期先优化 CPU 预览：共享 downsample glow buffer、减少每像素重复 blur、关闭 tile 避免跨像素边界错误。中期按 NativeEnhancer-FE 的多 pass shader 思路，把 halation/bloom、grain 和 3D LUT sampler 迁移到 GPU。
