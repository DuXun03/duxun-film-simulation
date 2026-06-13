# DuXun Film Simulation License MVP Beta 发布准备状态

日期：2026-06-13

## 候选包

- 包名：`DuXunFilmSim-OFX-v5.0-license-mvp.zip`
- ZIP SHA256：`995B637F1F8EC6006741521540FF2D36D706687D106B70F66188B001F609E38E`
- OFX SHA256：`E7C5E0D4FF7BDF8F12ECD32C19DDE9C98F154251D4C5584D4EAA4E5011714014`
- 产品 id：`duxun-filmsim-ofx-v5`
- 产品版本：`5.0`
- runtime key version：`v1`
- 当前状态：ready for first controlled beta send，仍不是正式公开发布。

除非发现阻断级问题并重新记录原因与新 hash，否则不要修改或重新打包该 zip。

Beta support drill 已通过。该包可以从 operator handoff 推进到第一批受控 beta 发送。

## Beta 交付文档

- Operator runbook：`docs/beta-operator-runbook-2026-06-13.md`
- 用户测试指南：`docs/beta-customer-test-guide-2026-06-13.md`
- 反馈模板：`docs/beta-feedback-template-2026-06-13.md`
- Beta support drill：`docs/beta-support-drill-2026-06-13.md`
- Beta support drill 结果：`docs/beta-support-drill-result-2026-06-13.md`
- Operator ledger 模板：`docs/beta-operator-ledger-template-2026-06-13.md`
- License key custody 参考：`docs/license-key-custody-and-issuance-2026-06-13.md`
- Licensing QA 记录：`docs/licensing-mvp-qa-2026-06-12.md`

## QA 结论

当前已验证基线：

- 单元测试通过：`python -m unittest discover -s tests -q`。
- package SHA256 与记录的候选包 hash 一致。
- License MVP package audit 未发现 `license_sign_tool`、`private`、`.pem`、`.key`、`secret` 条目。
- tracked source scan 未发现 private-key block。
- Resolve smoke 已验证 trial、activation request、license install、`Reload License`、buyout status 和 watermark removal。
- Beta support drill 已通过正常首次激活、trial-stuck 排查、重发和换机判断路径。
- Fuji Superia、Agfa Vista、CineStill 800T visual baseline 仍保持冻结。

该包适合发送给能够按手工离线激活流程操作并提供结构化反馈的受控 beta 测试用户。

当前放行判断：第一批受控 beta send 的 release gate 已通过。彩排归档仍保存在 git 外部，公开摘要见 `docs/beta-support-drill-result-2026-06-13.md`。

## 允许的 Beta 范围

允许：

- 将冻结的 License MVP zip 发送给选定 beta 测试用户。
- 让测试用户安装 OFX package，生成 `activation-request.json` 并回传。
- 在订单或 beta 资格确认后，手工签发 buyout `license.json`。
- 使用反馈模板收集反馈。
- 对不同 preset、素材类型和 GPU 做基础画面质量对比。
- 在离线 operator ledger 中记录激活、重发、换机和拒签决定。

## 已知限制

- 仅支持离线激活，没有 license server。
- v5.0 不支持自动吊销。
- 换机需要人工处理。
- Watermark 是 MVP 阶段的误用提醒，不是强 DRM。
- Resolve UI smoke 是人工验证，不是自动化 UI 回归。
- ProgramData 权限依赖当前 Windows 环境和 installer 行为。
- 没有 GUI installer。
- 本包不包含 HDR、OpenCL、Metal、新 GPU 扩展或 LUT runtime loading。
- CineStill 50D 仍停留在第一轮 daylight exterior pass 状态。

## 不要做

- 不要修改视觉算法、冻结参数或 licensing runtime。
- 不要 rebuild、overwrite 或 repackage `DuXunFilmSim-OFX-v5.0-license-mvp.zip`。
- 不要把私钥、签发工作区、operator ledger、私密路径或客户归档放进 git。
- 不要把 `license_sign_tool.py`、private key、`.pem`、`.key` 或内部 operator 材料放进客户包。
- 不要用 dry-run 或 staging key 给真实客户签发 license。
- 不要把当前状态描述为正式公开发布。

真实签发必须使用与包内 public key 匹配的生产 `v1` private key。dry-run key 不能用于真实客户授权。

## 下一步 Beta 流程

对每位 beta 测试用户：

1. 发送冻结 zip 和 `docs/beta-customer-test-guide-2026-06-13.md`。
2. 接收 `activation-request.json`。
3. 按 `docs/beta-operator-runbook-2026-06-13.md` 操作。
4. 只回传 `license.json`。
5. 请测试用户填写 `docs/beta-feedback-template-2026-06-13.md`。
6. 使用 `docs/beta-operator-ledger-template-2026-06-13.md` 作为字段参考，在离线 ledger 中记录结果、machine hash、签发时间和反馈状态。

已完成的 first-send gate：

1. 使用虚拟测试用户数据执行了 `docs/beta-support-drill-2026-06-13.md`。
2. 在 git 外部归档了彩排 request、license、验签输出、反馈和 ledger copy。
3. 已确认没有 private key、真实客户资料、真实 ledger 或私密材料进入 git。

下一次 readiness 更新请新建带日期的文档，不要直接覆盖候选包 hash。
