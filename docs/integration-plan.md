# FilmSim Plugin -- 开源组件选型与整合方案

## 一、胶片模拟环节分解

```
输入 (Scene-Linear / DWG/DI)
  |
  ├── [A] Camera IDT ──────────── 摄影机Log→工作空间
  ├── [B] Film S-Curve ────────── 胶片特性曲线 (Shoulder/Toe)
  ├── [C] Color Matrix ────────── 光谱交叉耦合 (Film Stock)
  ├── [D] Dye Density ─────────── 染料密度→饱和度非线性
  ├── [E] CMY Color Head ──────── 减色色彩校正
  ├── [F] Display Transform ───── ACES→显示设备 (AgX/OpenDRT)
  ├── [G] Halation ────────────── 光晕 (红橙色光散射)
  ├── [H] Film Grain ──────────── 胶片颗粒 (3D噪声)
  ├── [I] Bloom / Glow ────────── 柔光辉光
  ├── [J] Gate Weave ──────────── 门幅晃动
  └── [K] Film Damage ─────────── 灰尘/划痕/毛发
```

**划分原则:**
- A-F 是色彩科学管线 (纯颜色变换, DCTL 搞定)
- G-K 是纹理效果管线 (空间域处理, OFX 或特殊 DCTL)

## 二、每环节最佳开源方案

### [A] Camera IDT -- 输入设备转换

| 候选 | Stars | 协议 | 关键能力 |
|------|-------|------|----------|
| Wavechaser/NamiColor | 116 | ? | 胶片扫描线性化, 密度域处理 |
| boomyjee/mycolor | ~1200 | MIT | 60+ camera IDT LUT (64^3 JSON) |
| mikaelsundell/PD-Transform | 65 | GPL? | 全色彩空间转换引擎 |

**选型: boomyjee/mycolor 的 IDT 系统**
理由: mycolor 已内置 60+ 个相机 IDT 3D LUT (ARRI/Sony/RED/Canon/Apple 全覆盖),
可直接从中提取 `.cube` LUT 文件或以 DCTL 方式集成。


### [B] Film S-Curve -- 胶片特性曲线

| 候选 | Stars | 协议 | 核心算法 |
|------|-------|------|----------|
| xtremestuff/resolve-dctl | 157 | MIT | S-Curves, Power-Knee, Inverse-S-Curves |
| MoazElgabry/DCTLs | 57 | GPL-3.0 | ME_Filmic Contrast v1.3 (最成熟的胶片曲线) |
| mikaelsundell/PD-Tonecurve | 65 | ? | Shoulder/Toe/Contrast 三参数 |

**选型: xtremestuff/resolve-dctl S-Curves + our shoulder/toe refinements**
理由: MIT 协议最宽松, xtremestuff 的多种 Sigmoid 函数可组合,
Power-Knee 提供专业的高光 roll-off。我们的肩膀/脚趾模型填补了电影曲线两端。
用 MIT 协议避免 GPL 传染。


### [C] Film Color Matrix -- 光谱交叉耦合

| 候选 | Stars | 协议 | 关键能力 |
|------|-------|------|----------|
| Our FilmSim-Matrix | - | MIT | 5 种 Stock 3x3 矩阵 |
| mikaelsundell/PD-LogC3-FilmMatrix | 65 | ? | ARRI 胶片矩阵 |
| boomyjee/mycolor | ~1200 | MIT | GLSL 中的光谱折射模型 |

**选型: 双来源组合**
1. 实用矩阵: FilmSim-Matrix (5 种常见 Stock, 直接集成)
2. 物理模型: mycolor 的光谱折射 (`refrakt()` 函数) -- 将 RGB 分解为 R-Y-G-C-B-M 六通道,
   每通道独立色相旋转和饱和度调整, 通过亮度分离遮罩加权混合。
   
**优先级: P0 = 实用矩阵 / P1 = 光谱折射模型**


### [D] Dye Density -- 染料密度→饱和度

| 候选 | Stars | 协议 | 核心算法 |
|------|-------|------|----------|
| Our FilmSim-Density | - | MIT | Beer-Lambert 偏差, 密度-饱和度曲线 |
| boomyjee/mycolor `dns()` | ~1200 | MIT | 三曲线模型: Hue/Sat/Lum vs Sat |
| MoazElgabry/ME_Hue-Sat_Ramp | 57 | GPL-3.0 | 色相-饱和度渐变 |

**选型: boomyjee/mycolor 的 `dns()` 函数 (MIT)**
理由: mycolor 的染料密度模型是目前开源中最完整的:
- 三组样条曲线控制 (Hue/Sat/Lum vs Sat)
- 密度增强受 maxComponent/max(result) 约束防止通道裁剪
- 高光滚降 (hlko) 和色度压缩 (crmko) 参数
- 已被用于电影长片的实际调色 (生产验证)
- MIT 协议, 可随意集成

**移植工作: GLSL → DCTL (语法高度兼容, 主要是数学函数名转换)**


### [E] CMY Color Head -- 减色色彩校正

| 候选 | Stars | 协议 | 关键能力 |
|------|-------|------|----------|
| Our FilmSim-CMY | - | MIT | 对数域 CMY 密度 + 通道串扰 |
| mikaelsundell/PD-Cineon-Exposure | 65 | ? | Cineon 密度域曝光 + 伪色辅助 |

**选型: FilmSim-CMY (自研, MIT)**
理由: 减色模型相对简单, 自研的质量已经足够。
PD-Cineon-Exposure 可作补充 (Cineon 工作流兼容性)。


### [F] Display Transform -- 显示变换

| 候选 | Stars | 协议 | 核心算法 |
|------|-------|------|----------|
| sobotka/AgX-Resolve | ? | ? | AgX 图像形成 (被 UffyLook 使用) |
| jedypod/open-display-transform | 479 | GPL-3.0 | 4 种方案, OpenDRT 最完善 |

**选型: sobotka/AgX-Resolve**
理由: 
1. 已被 RichardUffy 在 Resolve 工作流中实战验证
2. 实现比 OpenDRT 更简洁 (适合集成)
3. AgX 在色彩科学社区广泛认可 (被 Blender Filmic 继承者采用)
4. 注意确认许可证 (Troy Sobotka 的 AgX 原始仓库是 GPL-3.0, 需确认 Resolve 版)
5. 如协议不兼容, 备选: 参考 IOLITE 的 Minimal AgX Implementation (更容易移植)


### [G] Halation -- 光晕

| 候选 | Stars | 协议 | 实现方式 |
|------|-------|------|----------|
| xjackyz/halation | ? | MIT? | 物理启发式光晕 DCTL |
| hotgluebanjo/halation-dctl | ? | ? | 简单光晕 (被 Uffy 集成) |
| boomyjee/mycolor `skatter()` | ~1200 | MIT | 阴影/高光散射, YIQ 亮度保持 |

**选型: 组合方案 (MIT 先行)**
1. 参考 xjackyz/halation 的物理模型 (指数函数 + 模糊)
2. 移植 mycolor 的 `skatter()` 散射模型 (MIT协议更安全):
   - 阴影散射: `exp(-5.5 * lum)`, 色相距离遮罩
   - 高光散射: `exp(-2.2 * lum^4)` roll-off
   - YIQ 亮度保持, 防止散射改变整体亮度
3. 注意: DCTL 无法做空间卷积 (需要 OFX)。但 xjackyz 的 DCTL 版 halation
   说明可以用 DCTL 内嵌的近似模糊实现 (GPU 代价较高)。

**实现路径: 先用 DCTL 近似模糊实现, Phase 3 升级到 OFX + Metal 做真正的空间卷积**


### [H] Film Grain -- 胶片颗粒

| 候选 | Stars | 协议 | 核心算法 |
|------|-------|------|----------|
| mattdesl/glsl-film-grain | 202 | MIT | 3D 噪声函数, 自然颗粒 |
| goldkiss2010-ai/noisecles-dctl | 1 | ? | ΔY 噪声潜力单色粒子场 |
| boomyjee/mycolor | ~1200 | MIT | (无独立颗粒, 依赖外部) |

**选型: mattdesl/glsl-film-grain (MIT)**
理由:
1. 202 Stars 的开源验证 + MIT 协议
2. 3D 噪声方法比 hash 函数颗粒更自然
3. 已提供完整的混合策略 (soft-light blend + 亮度自适应 + smoothstep 阈值)
4. 移植难度低: GLSL → DCTL (几乎 1:1 翻译)
5. 附带的 mix 逻辑可直接用于亮度相关的颗粒可见度控制:
   ```glsl
   float luminance = luma(backgroundColor);
   float response = smoothstep(0.05, 0.5, luminance);
   // 暗部减少颗粒可见度
   ```

**移植工作: 核心 `grain()` 函数 + `noise3D()` 实现 → DCTL**


### [I] Bloom / Glow -- 柔光

无专用开源 DCTL。

**选型: 从 Halation 代码派生**
Bloom 是光晕的柔和版本 -- 将光晕的红色限制去掉, 改为全色散射,
散射半径增大, 强度降低。


### [J] Gate Weave & [K] Film Damage -- 机械/物理损伤

无开源 DCTL。UffyLook 使用 Resolve 内置 OFX。

**选型: Phase 3 处理 (OFX 插件)**
这两个效果本质上是空间/时间域变换 + 叠加, DCTL 难以优雅实现。
建议用 Resolve 内置 OFX 或者 Phase 3 用 C++ OFX 实现。


### 完整开源项目 "拿来就用" 的方案

**终极整合方案: boomyjee/mycolor (MIT) 作为核心引擎**

mycolor 是目前开源世界最完整的胶片模拟引擎:
- 完整 GLSL 管线: 光谱折射 → 染料密度 → 光散射 → ACES RRT
- 已附带 DCTL 移植版本 (mycolor.dctl)
- 60+ camera IDT
- 24 种 ODT
- MIT 协议

**直接策略:**
1. Fork mycolor, 取其 mycolor.dctl
2. 移除不需要的部分 (如 WebGL UI 层)
3. 添加我们优化的 FilmS-Curve + FilmS-Matrix(5 stocks)
4. 添加 mattdesl/glsl-film-grain 颗粒
5. 添加 xjackyz/halation 光晕
6. 用 AgX-Resolve 替换或补充 ACES RRT
7. 打包为统一插件


## 三、整合架构

```
FilmSim-Plugin (统一 DCTL 管线)
│
├─── IDT (from mycolor IDT LUTs)
│
├─── 色彩科学核心 (DCTL)
│    ├── FilmS-Contrast  ─── 自研 S-Curve + xtremestuff Power-Knee
│    ├── FilmS-Matrix    ─── 自研 5-Stock 矩阵 + mycolor refrakt()
│    ├── FilmS-Density   ─── mycolor dns() 移植
│    ├── FilmS-CMY       ─── 自研减色模型
│    └── FilmS-Display   ─── AgX-Resolve / OpenDRT
│
├─── 纹理效果 (DCTL)
│    ├── FilmS-Halation  ─── xjackyz/halation + mycolor skatter()
│    ├── FilmS-Grain     ─── mattdesl/glsl-film-grain 移植
│    └── FilmS-Bloom     ─── Halation 派生
│
└─── 管线整合
     └── FilmS-Core      ─── 一键调用所有模块
```

## 四、移植工作量估算

| 组件 | 来源 | 移植难度 | 预计行数 | 协议 |
|------|------|----------|----------|------|
| IDT 系统 | mycolor | 直接使用 LUT | ~50 | MIT |
| 光谱折射 | mycolor refrakt() | GLSL→DCTL 中 | ~200 | MIT |
| 染料密度 | mycolor dns() | GLSL→DCTL 中 | ~150 | MIT |
| 光散射 | mycolor skatter() | GLSL→DCTL 中 | ~100 | MIT |
| 胶片颗粒 | mattdesl/glsl-film-grain | GLSL→DCTL 中 | ~80 | MIT |
| 光晕 | xjackyz/halation | 直接参考代码 | ~80 | 待确认 |
| 显示变换 | AgX-Resolve | 直接集成 | ~200 | 待确认 |
| 色彩矩阵 | 自研 | 已完成 | - | MIT |
| S曲线 | xtremestuff+自研 | 已完成 | - | MIT |
| CMY减色 | 自研 | 已完成 | - | MIT |

**总新增行数: ~860 行 DCTL + 提取 LUT 文件**

## 五、MIT 协议安全清单

**可直接使用 (MIT):**
- mycolor 全管线 ✅
- mattdesl/glsl-film-grain ✅
- xtremestuff/resolve-dctl ✅
- Demystify-Color/DCTLs ✅
- 自研组件 ✅

**需确认协议后使用:**
- xjackyz/halation (未标注协议) ⚠️
- AgX-Resolve (源自 GPL-3.0 AgX, Resolve 版未知) ⚠️
- NamiColor (未标注) ⚠️
- photographic-dctls (未标注) ⚠️

**不可直接使用 (GPL, 需规避):**
- MoazElgabry/DCTLs (GPL-3.0) ❌
- jedypod/open-display-transform (GPL-3.0) ❌
- UffyLook (未标注, 但依赖于多个未标注项目) ⚠️

## 六、推荐执行路线

**Phase 1 (已完成): 自研核心 DCTL 骨架**
- FilmS-Contrast, FilmS-Matrix, FilmS-Density, FilmS-CMY, FilmS-Core
- 5 种 Stock, 完整的色彩科学文档

**Phase 1.5 (本次): 集成 mycolor 引擎核心**
1. 从 mycolor 提取 `refrakt()`, `dns()`, `skatter()` GLSL 代码
2. 移植为独立的 DCTL 文件 (FilmS-Refraction, FilmS-Scatter)
3. A/B 测试与自研版本的差异

**Phase 2: 集成纹理效果**
1. 移植 glsl-film-grain → FilmS-Grain.dctl
2. 集成 xjackyz/halation → FilmS-Halation.dctl
3. 派生 FilmS-Bloom.dctl

**Phase 3: 显示变换 + IDT**
1. 集成 AgX-Resolve → FilmS-Display.dctl
2. 提取 mycolor IDT LUTs → IDT 预设包

**Phase 4: 统一封装**
1. 更新 FilmS-Core 包含所有模块
2. 添加参数系统
3. 3D LUT 导出功能
