# 色彩科学参考文档

## 1. 胶片特性曲线 (D-log-H Curve)

### Cineon 编码

Cineon 是 Kodak 开发的数字胶片扫描标准，将胶片密度映射为 10-bit 编码值：

```
CV = 685 * log10(Exposure) + 470

黑点:   CV = 95   (D-min = 0.2, base+fog)
白点:   CV = 685  (D-max = 2.0)
参考白: CV = 685  (90% 反射率)
18% 灰: CV = 470  (0.0 log exposure)
```

### 三段式曲线

```
Density
^
|          _______ Shoulder (高光压缩, 2+ stops above mid)
|        /
|      /    Straight Line (线性区域, gamma = 0.6)
|    /
|  /______ Toe (阴影压缩, 3- stops below mid)
|/
+-----------------------------------> log Exposure
```

## 2. 胶片光谱灵敏度

### 人眼 vs 胶片 vs 数字传感器

| 层 | 人眼峰值 | 胶片峰值 | 数字传感器峰值 |
|----|----------|----------|----------------|
| R  | 564 nm   | 580 nm   | 600 nm |
| G  | 534 nm   | 540 nm   | 530 nm |
| B  | 420 nm   | 430 nm   | 460 nm |

胶片红层对更长波长敏感，导致暖色调增强。

### 交叉耦合矩阵推导

从光谱灵敏度曲线积分得出 RGB 交叉耦合系数：

```
Kodak Vision3 典型矩阵（归一化）:
R_out = 1.10*R - 0.05*G - 0.05*B
G_out = 0.02*R + 1.04*G - 0.06*B
B_out = -0.06*R - 0.02*G + 1.08*B
```

## 3. 染料密度与 Beer-Lambert 定律

### 理想模型

Beer-Lambert: A = ε * c * l
- A = 吸光度（密度）
- ε = 摩尔消光系数
- c = 浓度
- l = 光程长度

### 实际偏差

在高浓度下，染料分子聚集导致偏离线性：

```
有效密度 ≈ ε * c / (1 + k * c)
```

k 为聚集常数，通常 0.1-0.3。

这导致高密度区域（暗部）饱和度压缩。

## 4. CMY 减色模型

### 密度叠加

在密度/对数域中，CMY 滤镜是线性的：

```
D_total = D_cyan + D_magenta + D_yellow + D_base
```

### 到 RGB 的转换

```
R_out = R_in * 10^(-D_cyan)
G_out = G_in * 10^(-D_magenta)
B_out = B_in * 10^(-D_yellow)
```

在 log2 空间（每 1 单位 = 1 stop）：

```
log2(R_out) = log2(R_in) - D_cyan / log10(2)
            ≈ log2(R_in) - D_cyan * 3.32
```

## 5. ACES 管线参考

### ACES 核心概念

```
Scene-Referred (ACES AP0, Linear)
    |
[IDT - Input Device Transform]
    |
[LMT - Look Modification Transform]  ← 创意调色
    |
[RRT - Reference Rendering Transform]  ← 胶片模拟显示变换
    |
[ODT - Output Device Transform]
    |
Display-Referred (Rec.709/P3/HDR)
```

### Cineon Film Log 到 ACEScct

```
Cineon → ACEScct 不是直接的，但可以近似：

// Cineon CV 到 Linear
lin = 10^((CV - 470) / 685)

// Linear 到 ACES2065-1 需要 3x3 矩阵
```

## 6. 光晕 (Halation) 物理模型

### 物理原理

光晕是由穿过乳剂层后在片基背面反射回的光线造成的：

```
入射光 → 乳剂层 → 片基层 → 反射 → 乳剂层（第二次）→ 光晕
```

### 数学模型

```
I_halation(r) = I(r) * α * exp(-r / σ)
```

- α: 反射系数 (0.01-0.05)
- σ: 散射半径 (像素, 5-50)
- r: 到光源的距离

### 实现为空间卷积

```
halation = gaussian_blur(input, sigma) * strength
halation = extract_red_channel(halation)  // 只有红光穿透
output = input + halation * redness_factor
```

注意: DCTL 无法直接做空间卷积（它是逐像素操作）。光晕必须用 OFX 或 Resolve FX 实现。

## 7. 胶片颗粒模型

### 颗粒特性

- 颗粒大小: 2-15 微米（取决于胶片速度）
- 分布: 泊松分布（每个染料云是离散的）
- 频率: RGB 三通道独立分布
- 时间: 帧间完全随机

### 3D 噪声实现

```
grain(x, y, frame) = noise3D(x/scale, y/scale, frame)
```

使用 Perlin/Simplex 3D 噪声，Z 轴为时间维度。

## 参考书目

- Kennel, G. (2006). "Color and Mastering for Digital Cinema"
- Poynton, C. (2012). "Digital Video and HD: Algorithms and Interfaces"
- ACES 1.3 Specification (S-2014-003)
- Cineon Digital Film System Documentation
- Dehancer Color Science Whitepaper
- Filmbox Technical Overview
