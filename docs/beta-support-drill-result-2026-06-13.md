# DuXun Film Simulation License MVP Beta 支持彩排结果

日期：2026-06-13

## 范围

本文记录 DuXun Film Simulation v5.0 License MVP 的一次 Beta 支持闭环彩排结果。内容是可提交、可公开阅读的摘要。

私密彩排材料保存在 git 外部。本文不包含私钥、私密目录路径、真实客户资料、真实 ledger 数据、`activation-request.json` 内容或 `license.json` 内容。

本次彩排使用的 dry-run/staging 材料均只用于流程验证，不可用于真实客户授权。

## 彩排身份

- 彩排 run：`run-2026-06-13-001`
- 虚拟测试用户 id：`CUST-BETA-DRILL-001`
- 虚拟订单 id：`ORDER-BETA-DRILL-20260613-001`
- 虚拟支持 case：`CASE-BETA-DRILL-20260613-001`
- 外部归档代号：`duxun-filmsim-v5-beta-drill-2026-06-13/run-2026-06-13-001`
- 授权标记：`DRILL_ONLY_NOT_FOR_REAL_CUSTOMER_AUTHORIZATION`

## 发布锚点

- 包名：`DuXunFilmSim-OFX-v5.0-license-mvp.zip`
- ZIP SHA256：`995B637F1F8EC6006741521540FF2D36D706687D106B70F66188B001F609E38E`
- 预期 OFX SHA256：`E7C5E0D4FF7BDF8F12ECD32C19DDE9C98F154251D4C5584D4EAA4E5011714014`
- 产品 id：`duxun-filmsim-ofx-v5`
- 产品版本：`5.0`

彩排期间没有修改或重新打包 release zip。

## 基线检查结果

- `git status --short --branch`：彩排开始时为干净的 `main`。
- `python -m unittest discover -s tests -q`：`Ran 68 tests`，`OK`。
- `Get-FileHash -Algorithm SHA256 DuXunFilmSim-OFX-v5.0-license-mvp.zip`：与预期 ZIP hash 一致。
- release zip 内敏感材料审计：未发现 `license_sign_tool`、`private`、`.pem`、`.key`、`secret`、`ledger` 命中。
- 仓库 private-key block 扫描：无命中。

## 路径一：正常首次激活

结果：通过。

证据摘要：

- 已生成虚拟 `activation-request.json`，并归档在 git 外部。
- dry-run 签发材料仅用于本次彩排。
- 彩排 buyout `license.json` 在 git 外部签发。
- license 验签返回 `ok: license valid`。
- license 安装流程模拟到外部归档路径。
- 安装后的 license SHA256 与签发 license SHA256 一致。
- 最终彩排归档中未保留 dry-run private key。

该路径确认 operator handoff 可以支撑：

```text
收到 activation request -> 检查 request -> 签发 license -> 验签 -> 安装 license -> buyout activated
```

## 路径二：安装 license 后 Resolve 仍显示 trial

结果：通过。

彩排场景：

```text
install_license 显示成功，但 Resolve 仍显示 trial 或仍有 watermark。
```

证据摘要：

- 归档 license 在预期虚拟 machine hash 下验签通过。
- 使用虚拟错误 machine hash 复现了 machine mismatch 类排查路径。
- 支持记录覆盖了排查动作：点击 `Reload License`、重启 Resolve、确认 ProgramData 下 `license.json`、索取 License 面板截图、索取命令窗口截图、必要时检查 `license.log`。
- 未解决或 mismatch 情况的 `manual_review` 记录路径已彩排。

结论：当前用户测试指南和 operator runbook 足以帮助区分 Resolve 状态未刷新与 machine/license mismatch。

## 路径三：重发与换机判断

结果：通过。

重发判断：

- 同一个虚拟客户、同一个订单、同一个 machine hash。
- 判断结果为只重发已归档 license。
- 不需要重新签发 license。
- `reissueCount` 保持 `0`。

换机判断：

- 已在 git 外部创建新的虚拟换机 request。
- 换机 machine hash 与原始虚拟 request 不同。
- 首次换机判断为可批准。
- 换机 license 使用 dry-run 材料签发并验签通过。
- `reissueCount` 增加到 `1`。

可疑换机处理：

- 判断状态保持为 `manual_review` 或 `rejected_suspicious_request`。
- owner review 前不签发。
- 不向测试用户发送内部 ledger 行或私密签发细节。

## 外部归档内容

外部归档代号：

```text
duxun-filmsim-v5-beta-drill-2026-06-13/run-2026-06-13-001
```

归档类别摘要：

- 虚拟 activation request。
- 彩排 license 文件。
- 验签输出。
- 虚拟反馈。
- 彩排 ledger copy。
- 支持记录。
- dry-run public verification material。

归档保存在 git 外部。本仓库未包含私钥、实际 request 文件、实际 license 文件、真实客户资料或真实 ledger 行。

## 结论

通过。

本次支持彩排覆盖了要求的三条路径：

- 正常首次激活。
- license 安装成功但 Resolve 仍显示 trial。
- 重发与换机人工判断流程。

第一批受控真实用户 beta 发送前，没有剩余的 support-drill 阻断项。

## 继续遵守的边界

以下是发布边界，不是第一批受控 beta 发送的阻断项：

- 真实客户授权只能使用与包内 embedded public key 匹配的生产 `v1` private key。
- dry-run/staging key 永远不能用于真实客户 license。
- 真实客户资料、真实 ledger、私钥、签发工作区、`activation-request.json` 和 `license.json` 不进入 git。
- 除非发现阻断级问题并重新记录 readiness，否则保持 License MVP zip 冻结。
- 当前仍是受控 beta 发送，不是正式公开发布。
