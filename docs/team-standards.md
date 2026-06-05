# FilmSim Plugin -- 团队开发规范 v1.0

制定日期：2026-05-28
维护人：Senior Developer (高级开发工程师)
适用范围：film-sim-plugin 项目全体贡献者

---

## 零、执行总则

本项目每个开发环节先思考，再执行。开始写代码前必须列举可行方案并自动评估：范围、风险、视觉收益、性能成本、许可边界和可验证性。

遇到未知问题、效果实现瓶颈、性能瓶颈或第三方算法取舍时，必须先从 GitHub、官方文档或互联网检索可参考实现和约束，不允许因为一时看不到结果就跳过关键问题。

大的工程必须拆成可独立验证的子项目推进。每个子项目都要有明确目标、负责文件、测试方法和回退边界；能并行的调研、实现和验证任务应并行推进，但不得让多个任务无约束修改同一文件区域。

需要用户配合的事项必须当即提出，包括 DaVinci Resolve 操作、系统权限、重启宿主、安装依赖、确认授权文本、提供素材或允许关闭正在运行的工程。

所有完成声明必须基于新鲜验证证据：至少包含对应测试、编译、安装或 Resolve 宿主验证之一。无法验证时要说明具体阻塞点，不得用推测代替结果。

---

## 一、DCTL 编码规范

### 1.1 文件组织

```
dctl/
├── FilmSim-Common.h         ← 共享数学库（单一事实来源）
├── FilmSim-Contrast.dctl    ← 每个模块 ≤ 150 行
├── FilmSim-Matrix.dctl
├── FilmSim-Density.dctl
├── FilmSim-CMY.dctl
└── FilmSim-Core.dctl        ← 组合模块，调用 Common.h
```

**铁律 1: 绝不在模块 .dctl 中重复实现逻辑。** 所有算法实现在 Common.h 中。模块文件只包含：
- `#include "FilmSim-Common.h"`
- `DEFINE_UI_PARAMS(...)` 声明
- `transform()` 函数（调用 Common.h 函数）

### 1.2 命名约定

| 类型 | 约定 | 示例 |
|------|------|------|
| 共享函数 | `_film_` 前缀，snake_case | `_film_s_curve()`, `_film_luminance()` |
| 共享常量 | `FILM_` 前缀，UPPER_SNAKE | `FILM_EPSILON`, `FILM_MIDDLE_GRAY` |
| UI 参数 | UPPER_SNAKE，简短 | `CONTRAST`, `FILM_STOCK` |
| 局部变量 | snake_case | `log_exp`, `chroma_boost` |
| 矩阵数组 | `m_` 前缀 | `m_k2383`, `m_f3513` |

### 1.3 数值精度

- 所有字面量显式加 `f` 后缀：`0.18f`，不是 `0.18`
- 使用 `FILM_EPSILON`（1e-10f）而非 `0.0f` 做除数保护
- 使用 `_film_log2()` / `_film_exp2()` 包装器，而非裸 `log2f()` / `powf(2.0f, ...)`
- 除法前必须检查分母：`(denom > FILM_EPSILON) ? num / denom : fallback`

### 1.4 DEFINE_UI_PARAMS 规范

每个模块必须为所有可调参数提供 UI 控件：

```c
DEFINE_UI_PARAMS(NAME, Display Name, WIDGET_TYPE, default, min, max, step,
    "Tooltip text explaining what this does")
```

要求：
- `Display Name` 使用英文（Resolve 中文版会自动翻译常见术语）
- 参数范围合理且有颜色科学依据
- 步长足够精细（对比度 0.01，黑点 0.001）
- Tooltip 解释此参数改变什么视觉属性

### 1.5 注释规范

```c
/* 单行注释使用 C 风格 (DCTL 编译器兼容性) */

/*
 * 多行块注释格式
 * 每行以空格缩进对齐
 */

/* ---- 段落分隔使用 4 个短横线 ---- */
```

禁止：
- `//` C++ 风格单行注释（部分 DCTL 编译器不支持）
- 无注释的魔数（用 `#define` 或写注释说明含义）

---

## 二、色彩科学验证流程

### 2.1 每个 DCTL 模块必须通过以下检查

#### 恒等测试 (Identity Test)
```
输入 = 中性灰(0.18, 0.18, 0.18)
所有参数 = 默认值
期望: 输出 ≈ 输入 (容差 < 0.001)
```

#### 单调性测试 (Monotonicity Test)
```
对于 Contrast/Curve 类 DCTL:
输入递增 → 输出必须单调不减
```

#### 中性灰保持测试 (Neutral Preservation)
```
输入 = (x, x, x) 任何灰度值
期望: 输出 R=G=B (矩阵/密度/CMY 在默认参数下不应引入色偏)
```

#### 极端值测试 (Edge Cases)
```
输入 = (0, 0, 0)     → 输出不应为 NaN 或负值
输入 = (1, 1, 1)     → 输出不应爆炸
输入 = (100, 0, 0)   → 输出不应产生 NaN
```

### 2.2 修改色彩科学代码的流程

1. **先在 color-science.md 中记录理论依据**
2. **修改 Common.h 中的实现**（只改一处）
3. **更新所有受影响的测试期望值**
4. **在 Resolve 中做视觉 A/B 对比**（使用 Identity 预设对照）
5. **提交时附上前/后对比截图**

---

## 三、Git 工作流

### 3.1 分支策略

```
main          ← 稳定发布（仅通过 PR 合并）
  └── dev     ← 开发主线
       ├── feat/phase1.5-mcolor-port    ← 功能分支
       ├── feat/halation
       ├── fix/toe-div-zero
       └── docs/team-standards
```

### 3.2 提交信息格式

```
<type>(<scope>): <subject>

<body>

<footer>
```

类型：`feat`, `fix`, `refactor`, `docs`, `perf`, `test`
范围：`dctl`, `docs`, `presets`, `tests`

示例：
```
feat(dctl): add halation DCTL with gaussian approx

Implements red-channel halation scatter using exponential
falloff model from xjackyz/halation reference.

Closes #12
```

### 3.3 代码审查清单

每个 PR 必须检查：

- [ ] 所有逻辑仅在 Common.h 中实现（不在模块中重复）
- [ ] 参数已通过 DEFINE_UI_PARAMS 暴露
- [ ] 数值精度：使用 FILM_EPSILON 保护，字面量有 f 后缀
- [ ] Edge case 测试：0, 1, 负值, 极大值
- [ ] 恒等测试通过（默认参数 = 直通）
- [ ] 中性灰保持测试通过
- [ ] 代码注释完整（色彩科学原理、参数含义、算法步骤）

---

## 四、性能基准

### 4.1 目标

| 分辨率 | 目标帧率 | 每像素指令上限 |
|--------|----------|----------------|
| 1080p  | 实时 60fps | < 250 |
| 4K     | 实时 24fps | < 250 |
| 8K     | 实时 12fps | < 250 |

### 4.2 优化原则

1. **避免分支发散**：同一 warp 内的像素走不同分支会串行化执行
2. **使用 `_fabs()`, `_expf()` 等 GPU 内建函数**（单指令），避免 `if/else`
3. **`const` 数组优于 switch-case**：编译器更容易优化为常量加载
4. **尽量减少 `log2f()` 和 `powf()` 调用**：每个约 4-8 个 GPU 周期

### 4.3 性能回归检测

任何新增代码不应使 Core 管线总指令数超 250/px。
在 Resolve 中启用 DCTL 性能统计验证。

---

## 五、测试规范

### 5.1 测试素材要求

```
tests/
├── identity_test.exr     ← 纯中性灰渐变 (0-1 linear)
├── saturation_test.exr   ← 全饱和色卡 (RGB CMY 原色)
├── skin_tone_test.exr    ← 肤色参考 (ITU-R BT.2111 肤色线)
├── stress_test.exr       ← 极端值：NaN/Inf/负值/超大值
└── real_footage/         ← 真实素材 (ARRI/Sony/RED LogC)
    ├── daylight_ext.dpx
    ├── tungsten_int.dpx
    └── mixed_light.dpx
```

### 5.2 自动化测试（Python + OpenImageIO）

```python
# 示例：恒等测试框架
import OpenImageIO as oiio

def test_identity():
    ref = oiio.ImageBuf("identity_test.exr")
    result = apply_dctl("FilmSim-Core", ref)  # 默认参数
    diff = compute_diff(ref, result)
    assert diff.max_error < 0.001
```

---

## 六、知识传承

### 6.1 新人入门路径

1. 通读 `docs/color-science.md` -- 理解胶片色彩科学基础
2. 阅读 `FilmSim-Common.h` -- 理解共享库架构
3. 在 Resolve 中安装并实际使用每个模块
4. 阅读 `docs/integration-plan.md` -- 了解后续开源组件整合策略
5. 尝试在 Common.h 中添加一个小函数（如新的矩阵 Stock）

### 6.2 颜色科学参考

- Poynton, C. "Digital Video and HD: Algorithms and Interfaces" -- 数字视频圣经
- ACES 1.3 Specification (S-2014-003) -- 学院色彩编码
- Kennel, G. "Color and Mastering for Digital Cinema" -- 数字电影调色
- Cineon Digital Film System Documentation -- 胶片扫描标准

### 6.3 术语表

| 术语 | 英文 | 含义 |
|------|------|------|
| 特性曲线 | Characteristic Curve | D-log-H，密度对曝光对数的响应 |
| 趾部 | Toe | 暗部压缩区，薄膜特性 |
| 肩部 | Shoulder | 高光压缩区，过曝保护 |
| 染料密度 | Dye Density | CMY 染料云的浓度 |
| 减色法 | Subtractive Color | CMY 滤镜吸收光而非加光 |
| 交叉耦合 | Cross-Coupling | 乳剂层间的光谱重叠 |
| 打印机光源 | Printer Lights | 光学印片机的 RGB 曝光增量 |
| 光晕 | Halation | 片基反射红光形成的红色光晕 |
| IDT | Input Device Transform | 摄影机色彩空间到工作空间的转换 |
| ODT | Output Device Transform | 工作空间到显示设备的转换 |

---

## 七、变更日志

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-05-28 | 1.0 | 初始版本：DCTL 编码规范、色彩科学验证、Git 工作流、性能基准、测试规范 |
