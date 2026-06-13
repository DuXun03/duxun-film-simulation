# DuXun Film Simulation v5.0 受控公测交付包记录

日期：2026-06-13

## 交付结论

已生成第一批受控公测用户文件夹：

```text
C:\Users\LI\Desktop\DuXunFilmSim-v5.0-公测包-2026-06-13
```

该文件夹是用户侧交付物。它不是源码发布目录，也不包含签发端材料。

## 发布锚点

- 源候选包：`DuXunFilmSim-OFX-v5.0-license-mvp.zip`
- ZIP SHA256：`995B637F1F8EC6006741521540FF2D36D706687D106B70F66188B001F609E38E`
- OFX 文件：`DuXunFilmSim-OFX-v5.0-license-mvp\build\DuXunFilmSim.ofx`
- OFX SHA256：`E7C5E0D4FF7BDF8F12ECD32C19DDE9C98F154251D4C5584D4EAA4E5011714014`
- 产品 id：`duxun-filmsim-ofx-v5`
- 产品版本：`5.0`
- runtime key version：`v1`

原始 License MVP zip 未修改、未重打包。

## 桌面文件夹内容

根目录包含：

```text
00-请先阅读-中文使用说明.docx
一键安装插件.bat
生成激活请求.bat
安装授权文件.bat
反馈模板-请填写.docx
校验信息-SHA256.txt
DuXunFilmSim-OFX-v5.0-license-mvp
```

其中 `DuXunFilmSim-OFX-v5.0-license-mvp` 保留用户侧必要文件：

```text
build\DuXunFilmSim.ofx
scripts\install.bat
scripts\uninstall.bat
scripts\generate_activation_request.bat
scripts\generate_activation_request.py
scripts\install_license.bat
scripts\install_license.py
scripts\verify_license_test.py
scripts\duxun_license.py
README.md
RELEASE_NOTES.md
LICENSE
checksums\SHA256SUMS.txt
docs\licensing-mvp-qa-2026-06-12.md
docs\licensing-trial-design-2026-06-12.md
```

## 用户流程摘要

用户打开桌面交付文件夹后，流程为：

1. 阅读 `00-请先阅读-中文使用说明.docx`。
2. 关闭 DaVinci Resolve。
3. 双击 `一键安装插件.bat`，必要时右键以管理员身份运行。
4. 启动 Resolve，找到 `DuXun -> DuXun Film Simulation`。
5. 未授权时确认 24 小时试用和水印。
6. 在插件 `License` 区域点击 `Generate Activation Request`，或在有 Python 的机器上双击 `生成激活请求.bat`。
7. 将 `C:\ProgramData\DuXun\FilmSim\activation-request.json` 发回给 operator。
8. 收到 `license.json` 后双击 `安装授权文件.bat`，按提示选择授权文件。
9. 回到 Resolve 点击 `Reload License`，或重启 Resolve。
10. 成功状态应为 `License: buyout activated`，水印应消失。
11. 填写 `反馈模板-请填写.docx`。

根目录中文说明和反馈模板已从 Markdown 转为 Word 格式。对应 SHA256：

```text
00-请先阅读-中文使用说明.docx
ADF1A46E234256EC839D424F68FF9DF55D9DEA78E71C40A4364FCA5FCD3FF59D

反馈模板-请填写.docx
4B8B2F7B754BE253772494BF62F9DF8505DF5F79E77026F3D9FB71D434549960
```

DOCX 结构检查已通过：中文使用说明可解析出 87 个非空段落，反馈模板可解析出 46 个非空段落。当前机器缺少 DOCX 渲染器所需的外部转换程序，因此未完成 PNG 页面渲染视觉 QA。

## 安全检查结果

桌面交付文件夹的文件名扫描未发现以下用户包禁止项：

```text
license_sign_tool
private
_private
.pem
.key
secret
ledger
```

中文说明和根目录 bat 入口未包含签发端私钥路径、operator ledger 或内部 signing 命令。

仓库 private-key block 扫描无命中。

## 已知限制

- 当前仍是受控公测包，不是正式公开发行版。
- 仅支持离线激活，没有 license server。
- 真实授权必须由 operator 使用匹配 embedded public key 的生产 `v1` private key 签发。
- dry-run 或 staging key 不能用于真实客户授权。
- 生成激活请求的根目录脚本依赖系统 Python；无 Python 时，用户应使用 Resolve 插件面板里的 `Generate Activation Request`。
- 根目录 `安装授权文件.bat` 在 Python 可用时优先调用内置验证脚本；如果 Python 不可用或验证脚本不可用，会将 `license.json` 复制到 ProgramData，由插件 runtime 在 Resolve 内完成最终验证。
- v5.0 不支持自动吊销。
- 换机和重发授权仍是人工流程。
- 没有 GUI installer。
- CineStill 50D 仍是第一轮 daylight exterior pass。

## 发行边界

不要把以下材料加入桌面交付文件夹：

```text
private key
license_sign_tool.py
operator ledger
真实 customer data
真实 activation-request.json
真实 license.json
_private
签发工作区
```

后续如果需要更新桌面交付包，请新建带日期的交付记录，并重新记录 hash 与安全检查结果。
