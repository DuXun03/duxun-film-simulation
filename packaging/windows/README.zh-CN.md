# 独寻胶片模拟 v5.0 免费版 Windows 安装包

这是给不想自己编译源码的 Windows 用户准备的一键安装包。

## 安装

1. 关闭 DaVinci Resolve。
2. 解压整个 zip，不要只单独拖出某一个文件。
3. 双击 `Install.bat`。
4. 如果 Windows 弹出管理员权限确认，请选择“是”。
5. 安装完成后重启 DaVinci Resolve。
6. 在效果库中查找：

```text
OpenFX > DuXun > DuXun Film Simulation
```

## 卸载

双击 `Uninstall.bat`，按提示确认管理员权限。

## 常见问题

- 如果双击没有反应，请右键 `Install.bat`，选择“以管理员身份运行”。
- 如果安装脚本提示 DaVinci Resolve 正在运行，请先关闭 Resolve 再安装。
- 如果 Resolve 里看不到插件，请确认安装路径存在：

```text
C:\Program Files\Common Files\OFX\Plugins\DuXunFilmSim.ofx.bundle
```

## 包内容

- `Install.bat`: 一键安装脚本
- `Uninstall.bat`: 卸载脚本
- `DuXunFilmSim.ofx.bundle`: OpenFX 插件 bundle
- `README.zh-CN.md`: 中文说明
- `README.en.md`: English guide
- `LICENSE.txt`: 开源许可证
- `CHECKSUMS-SHA256.txt`: 文件校验值

## 许可证

DuXun Film Simulation 以 GPL-2.0-or-later 发布。源码和完整说明见 GitHub 仓库：

```text
https://github.com/DuXun03/duxun-film-simulation
```
