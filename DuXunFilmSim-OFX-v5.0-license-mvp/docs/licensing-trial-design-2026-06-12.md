# DuXun Film Simulation v5.0 Licensing / Trial Design - 2026-06-12

## 目标与边界

目标是在 v5.0 已通过 beta 试装 QA 的基础上，加入可落地的 trial / activation / buyout 授权方案。当前 release artifact 保持为 `DuXunFilmSim-OFX-v5.0.zip`，已记录 zip SHA256 `1925C5C01A7EDB3CE303F5B27F1E86D9823D68D6C8A4441984E53301C16636BE` 和 OFX SHA256 `A508CA29788705D29D78E046FB3DF7D2144C29ED32BBE6CC6662DA2063F48062`。

本轮只做授权设计和 MVP 授权代码。不启动 HDR，不启动 OpenCL，不启动 Metal，不增加新 GPU 扩展，不做 LUT runtime loading。不改现有视觉默认值，不改 matrix，不改 Fuji Superia、Agfa Vista、CineStill 800T 的 visual baseline freeze。

## 推荐 MVP

- 24 小时 trial：首次在本机生成 trial state 时开始计时，trial 期间可预览，但输出始终带轻量水印。
- 本地 signed `license.json`：license 内容可读，但必须通过签名校验。
- 离线 challenge / response：用户生成 `activation-request.json`，签发端返回 signed `license.json`。
- 买断永久 license：`licenseType=buyout` 且 `expiresAt=null`。
- 插件内轻量校验：只读 ProgramData license/trial 文件、校验签名、校验 machineHash、加水印或清除水印。
- 授权工具先用命令行，不做 GUI。

## 授权模式

### Trial

- 时长：24 小时 trial。
- 开始时间：本机第一次生成 trial state 的 UTC 时间，即 `%PROGRAMDATA%\DuXun\FilmSim\trial.json` 的 `trialStartedAt`。
- 结束时间：`trialStartedAt + 24h`。
- 过期行为：插件仍可加载、仍可预览，但 render 输出强制水印，并在 Resolve UI / persistent message 中显示 trial expired。
- 防回拨：trial state 记录 `lastSeenAt`。如果系统时间早于 `lastSeenAt` 超过 10 分钟，状态视为 clock rollback，输出继续水印并提示用户安装正式 license。
- 删除 trial state：视为人为重置，不提供自动重新 trial；后续可在人工支持流程中发一次 reset entitlement。

### Activation

Activation 采用离线 challenge / response：

1. 用户运行 `scripts\generate_activation_request.bat`，生成 `%PROGRAMDATA%\DuXun\FilmSim\activation-request.json`。
2. 用户把 request 文件发给签发端。
3. 签发端用离线私钥运行 `license_sign_tool.py`，基于 request 的 `machineHash` 生成 signed `license.json`。
4. 用户运行 `scripts\install_license.bat path\to\license.json`，把 license 安装到 `%PROGRAMDATA%\DuXun\FilmSim\license.json`。
5. 插件下次 render 或点击 reload 后读取 license，签名和机器匹配通过后移除水印。

Activation license 可以是限期授权，也可以用于客服补发。若是正式买断，应使用 buyout。

### Buyout

Buyout 是永久授权 entitlement：

- `licenseType=buyout`
- `expiresAt=null`
- `features` 至少包含 `core` 和 `export`
- 绑定一个 machineHash
- 允许在换机流程中由人工签发新的 buyout license，不在插件内自动解绑旧机器

## 机器绑定策略

`machineHash` 使用机器指纹 canonical string 做 SHA-256，license 只保存 hash，不保存原始字段。推荐字段：

- Windows `MachineGuid`
- 系统盘 volume serial
- `PROCESSOR_ARCHITECTURE`
- `PROCESSOR_IDENTIFIER`

避免过度敏感字段：

- 不采集用户名、路径、邮箱、IP、MAC 地址
- 不绑定 GPU 型号，因为显卡和驱动变更太常见
- 不绑定 Resolve 序列号或项目路径
- 不把原始 MachineGuid 写入 license

机器指纹版本为 `machine-v1`。未来若调整字段，新增 `machine-v2`，不直接改变 v1 语义。

## 换机 / 重置策略

- 默认每个 orderId 允许 2 次人工换机重签。
- 第 1 次换机：用户提交 orderId、旧 machineHash、新 activation request；人工签发新 license 并记录。
- 第 2 次换机：同上，但客服备注原因。
- 超过 2 次：人工判断，可要求购买新席位或撤销旧授权后重签。
- Trial reset：默认不开放自动 reset；特殊情况人工发一个短期 activated license。
- 插件不联网，不做自动席位回收。

## 本地文件布局

根目录：

```text
%PROGRAMDATA%\DuXun\FilmSim\
```

文件：

```text
%PROGRAMDATA%\DuXun\FilmSim\license.json
%PROGRAMDATA%\DuXun\FilmSim\trial.json
%PROGRAMDATA%\DuXun\FilmSim\activation-request.json
%PROGRAMDATA%\DuXun\FilmSim\logs\license.log
```

使用 ProgramData 的原因：

- ProgramData 是机器级应用数据位置，适合所有 Windows 用户共享同一台机器授权。
- 插件目录通常在 `C:\Program Files\Common Files\OFX\Plugins\...`，写入需要管理员权限，license 不应放在那里。
- 用户临时目录会被清理，也不适合跨用户和长期授权。
- ProgramData 便于 installer 创建目录和设置 ACL，也便于客服让用户定位文件。

## License 文件格式

`license.json` 为 UTF-8 JSON。字段固定：

```json
{
  "productId": "duxun-filmsim-ofx-v5",
  "licenseType": "buyout",
  "issuedAt": "2026-06-12T00:00:00Z",
  "expiresAt": null,
  "machineHash": "SHA256_HEX",
  "customerId": "cust_xxx",
  "orderId": "order_xxx",
  "features": ["core", "export"],
  "signature": "base64url-or-base64-signature",
  "keyVersion": "v1"
}
```

规则：

- `productId` 必须是 `duxun-filmsim-ofx-v5`。
- `licenseType` 只能是 `trial`、`activated`、`buyout`。
- `trial` 和 `activated` 必须有 UTC `expiresAt`。
- `buyout` 使用 `expiresAt=null` 表示永久授权。
- `machineHash` 必须等于当前机器计算值。
- `features` MVP 使用 `core` 和 `export`。
- `signature` 对除 `signature` 外的 canonical JSON payload 签名。
- `keyVersion` 用于后续公钥轮换。

## 安全模型

- license 内容可读，但必须签名。
- 插件只内置 public key。
- 私钥只保存在离线签发工具中，不进入插件、不进入 installer、不进入客户机器。
- MVP 使用 ECDSA P-256 + SHA-256。Python 签发工具输出 raw `r||s` 签名的 base64，插件通过 Windows CNG `BCryptVerifySignature` 校验。
- `license.json` 被篡改：canonical payload 与 signature 不匹配，状态为 invalid，输出水印。
- 复制 license 到另一台机器：signature 仍有效，但 `machineHash` 不匹配，状态为 machine mismatch，输出水印。
- 改系统时间：trial 使用 `lastSeenAt` 检查回拨；正式 activated license 如果系统时间明显异常，先提示用户修正时间，buyout 不依赖 expiresAt。
- 换机器：用户生成新 activation request，由人工流程重签 license。
- 代码签名另行规划。Windows 文件签名可用 Microsoft SignTool 对 `.ofx`、installer 或 zip 内可执行文件做 Authenticode 签名，但它不替代 license 签名。

## 用户体验

### 未授权 / trial

- 插件可加载，可预览效果。
- trial 或未授权输出带水印，避免正式交付误用。
- trial 期间 UI 显示剩余时间；过期后显示 trial expired。
- MVP 不强行改用户选择的 film preset，避免影响视觉 baseline；后续可增加“未授权仅开放部分预设”的策略。

### 授权失败

Resolve UI 中显示：

- `License: trial active, watermark enabled`
- `License: expired`
- `License: invalid signature`
- `License: machine mismatch`
- `License: buyout activated`

插件使用 persistent warning/message 提示状态，不弹复杂交互窗。

### 生成 activation request

推荐命令：

```bat
scripts\generate_activation_request.bat
```

输出：

```text
%PROGRAMDATA%\DuXun\FilmSim\activation-request.json
```

request 包含 `productId`、`requestType=offline_activation`、`machineHash`、`machineFingerprintVersion=machine-v1`、`createdAt` 和 `requestId`。

### 导入 license

推荐命令：

```bat
scripts\install_license.bat C:\path\to\license.json
```

脚本校验 license schema、signature、machineHash 后安装到：

```text
%PROGRAMDATA%\DuXun\FilmSim\license.json
```

Resolve 内点击 `Reload License` 或重启 Resolve 后生效。

## 工具链责任边界

### `generate_activation_request.bat`

输入：无，或可选输出路径。
输出：`activation-request.json`。
责任：调用 Python 工具生成本机 request，不接触私钥，不签发 license。

### `install_license.bat`

输入：签发端返回的 `license.json`。QA/staging 可选传入 `--public-key` 和 `--machine-hash` 覆盖默认校验上下文。
输出：安装到 ProgramData 的 `license.json`。
责任：安装前做 schema、signature、machineHash 校验；失败时不覆盖旧 license。

### `license_sign_tool.py`

输入：activation request、离线私钥、customerId、orderId、licenseType、expiresAt/days。
输出：signed `license.json`。生成临时离线 key 时可用 `--public-key-output` 额外写出 QA 校验用 public key PEM。
责任：只在离线签发环境使用；私钥由操作者提供，不从 repo 读取默认私钥。

### `verify_license_test.py`

输入：`license.json`。QA/staging 可选传入 `--public-key` 和 `--machine-hash`。
输出：signature、machineHash、expiresAt、features 校验结果。
责任：客服和开发本地诊断，不改变系统状态。

## 风险评估

- OFX 插件 UI 不适合复杂授权交互；复杂流程必须放到命令行工具或后续独立 GUI。
- Resolve 离线工作流常见，需要优先支持离线激活。
- Program Files 写入需要管理员权限，license 不应放在那里。
- ProgramData 目录 ACL 需要 installer 后续统一设置，避免普通用户无法写 activation request。
- 机器指纹过宽会误伤用户；本方案不绑定 GPU、MAC、用户名。
- 机器指纹过窄会降低复制防护；MVP 接受此风险，用人工换机和订单记录补足。
- 代码签名以后需要单独规划，Microsoft SignTool 可用于 Windows 文件签名。

## MVP 实现拆分

1. 文档与 schema：新增本文件，冻结 productId、licenseType、machineHash、features、signature、keyVersion 语义。
2. Python 授权工具核心：新增 `scripts/duxun_license.py`，实现机器 hash、canonical payload、ECDSA 签名/校验、request 生成。
3. 命令行工具：新增 `generate_activation_request.py`、`install_license.py`、`license_sign_tool.py`、`verify_license_test.py` 和 bat wrapper。
4. OFX runtime：新增 `ofx/DuXunFilm/DuXunLicense.h`，插件只读 ProgramData、校验 signed license、管理 24 小时 trial state。
5. OFX UI / render 挂点：在 `DuXunFilmSim.cpp` 增加 License 参数组、activation request 按钮、reload 按钮、persistent message、未授权水印。
6. 验证：运行 focused licensing tests、完整 Python unittest、可行时运行 `build_plugin.bat`。
