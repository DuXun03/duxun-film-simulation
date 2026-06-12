# DuXun Film Simulation v5.0 Licensing MVP QA

Date: 2026-06-12

## Scope

本轮只收口 v5.0 授权 MVP：24 小时 trial、本地 signed `license.json`、离线 activation request、buyout license、OFX License UI、watermark 状态切换。

未改动范围：

- 未覆盖原始已验收的 `DuXunFilmSim-OFX-v5.0.zip`。
- 未启动 HDR、OpenCL、Metal、新 GPU 扩展或 LUT runtime loading。
- 未改 Fuji Superia / Agfa Vista / CineStill 800T visual baseline。

## Toolchain QA

临时目录运行完整 request -> sign -> verify -> install 闭环：

- `scripts\generate_activation_request.py` 写出 `activation-request.json`。
- `scripts\license_sign_tool.py --generate-private-key ... --public-key-output ...` 生成临时 ECDSA P-256 keypair。
- `scripts\license_sign_tool.py --license-type buyout ...` 签发 buyout `license.json`。
- `scripts\verify_license_test.py --public-key ... --machine-hash ... license.json` 返回 `ok: license valid`。
- 篡改 `orderId` 后 verify 失败：`signature: invalid signature`。
- 篡改 `machineHash` 后 verify 失败：`signature: invalid signature`。
- 过期 activated license verify 失败：`expired: license expired`。
- `scripts\install_license.py` 安装有效 buyout 后，再安装无效 license 返回拒绝，并确认已安装 license SHA256 未改变。

新增工具行为：

- `license_sign_tool.py` 支持 `--public-key-output`，方便 QA/staging key 验证。
- `verify_license_test.py` 和 `install_license.py` 支持 `--public-key`、`--machine-hash`，用于临时 key 和测试机器 hash；默认仍使用内置 public key 和本机 machine hash。

## OFX Runtime QA

用临时 C++ harness 直接 include `ofx/DuXunFilm/DuXunLicense.h` 验证 runtime 状态逻辑，`DUXUN_FILMSIM_LICENSE_DIR` 指向临时目录。

结果：

- 无 `license.json` 时返回 `code=trial_active`、`watermark=1`、`trial=1`、`signed=0`。
- 自动生成 `trial.json`，包含 `trialStartedAt`、`lastSeenAt`、`machineHash`、`localGuard`。
- 篡改 `trial.json.localGuard` 后返回 `code=trial_tampered`、`watermark=1`。
- 写入未来 `lastSeenAt` 且 guard 正确后返回 `code=clock_rollback`、`watermark=1`。
- 使用 activation request 签发 buyout license 后返回 `code=buyout`、`watermark=0`、`signed=1`。
- Python 工具与 C++ runtime 计算的 machine hash 一致：`6C5ADA7C9FB0EA00B4FA57E16BE296F8D584146C28EAC8938C8AA65A00A15A88`。

## Build And Install

验证命令：

```text
python -m unittest discover -s tests -q
Ran 68 tests in 0.753s
OK
```

```text
cmd /c build_plugin.bat
RC=0
BUILD_OK
```

新构建 OFX SHA256：

```text
E7C5E0D4FF7BDF8F12ECD32C19DDE9C98F154251D4C5584D4EAA4E5011714014
```

安装：

```text
cmd /c scripts\install.bat
Build hash:     e7c5e0d4ff7bdf8f12ecd32c19dde9c98f154251d4c5584d4eaa4e5011714014
Installed hash: e7c5e0d4ff7bdf8f12ecd32c19dde9c98f154251d4c5584d4eaa4e5011714014
[OK] Installed OpenFX v5.0 bundle
```

## License MVP Beta Package

Package name:

```text
DuXunFilmSim-OFX-v5.0-license-mvp.zip
```

Package SHA256 is recorded outside the archive in `DuXunFilmSim-OFX-v5.0-license-mvp.zip.sha256`.

Package smoke from extracted zip:

- `scripts\install.bat` installed the packaged OFX and the installed hash matched `build\DuXunFilmSim.ofx`.
- `scripts\generate_activation_request.bat` generated `activation-request.json`.
- A temporary staging ECDSA P-256 keypair signed a buyout `license.json`.
- `scripts\install_license.bat --public-key ... --machine-hash ... license.json` installed the staging buyout license.
- `scripts\verify_license_test.py --public-key ... --machine-hash ...` returned `ok: license valid`.
- Package audit found no private key material, `.pem`, `.key`, or `license_sign_tool.py`.

## Resolve Smoke

环境：DaVinci Resolve Studio 20，项目 `Untitled Project 2`。

步骤和结果：

- Resolve 启动后进入项目管理器，打开含素材的 `Untitled Project 2`。
- Color 页面 OpenFX/特效库搜索 `DuXun`，可见 `DuXun Film Simulation`。
- 将 `DuXun Film Simulation` 拖到当前节点，效果成功挂载。
- 右侧设置面板可见 `License` 参数组。
- `License` 组展开后可见 `Status`、`Generate Activation Request`、`Reload License`。
- 无 license 时 UI 状态显示 `License: 24h trial active, 24h left, watermark enabled`，viewer 出现斜线 watermark。
- 点击 `Generate Activation Request` 后确认写出 `%PROGRAMDATA%\DuXun\FilmSim\activation-request.json`。
- 点击 `Reload License`，无 license 状态保持 trial active 和 watermark。
- 用本机 request 签发并安装 buyout license 后，点击 `Reload License`。
- UI 状态变为 `License: buyout activated`，viewer watermark 消失。

ProgramData 记录：

```text
C:\ProgramData\DuXun\FilmSim\activation-request.json
C:\ProgramData\DuXun\FilmSim\license.json
C:\ProgramData\DuXun\FilmSim\logs\license.log
```

`license.log` 关键行：

```text
2026-06-12T15:39:48Z status=trial_active message="License: 24h trial active, watermark enabled"
2026-06-12T15:42:06Z status=buyout message="License: buyout activated"
```

## Signing Key Note

本轮生成了本地 MVP ECDSA P-256 signing key。只有 public key 被写入源码；private key 不在 git 仓库内。正式销售前需要将 private key 移到安全保管位置，确认备份、访问控制和后续换机重签流程。

## Remaining Risks

- 当前授权是 MVP，本轮没有实现联网激活、GUI installer、license server 或自动换机。
- ProgramData ACL 仍依赖现有系统权限，后续 installer 需要统一设置。
- Resolve smoke 是单机手工验证，不是自动化 UI 回归。
- Watermark 是轻量 overlay，足够 MVP 误用提醒，但不是强 DRM。
- private key custody 是发售前最高优先级风险；丢失 private key 会导致无法签发与当前 public key 匹配的新 license。
