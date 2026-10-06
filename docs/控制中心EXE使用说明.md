# 控制中心 EXE 与源码启动

公开源码仓库不包含 `.exe`、安装包或构建缓存。本机之前生成的 EXE/安装包不随源码提交。从源码运行时，先安装 Python 3，再执行：

```powershell
py -3 -m pip install -r bridge_wroom/tools/requirements.txt
```

之后双击项目根目录的 `启动音频桥配置器.cmd`。脚本会优先运行你自行构建并放在根目录的 `CyberCatControlCenter.exe`；如果不存在，则启动 Python 配置器。

## 安装与卸载

如果已自行构建安装包，运行 `dist/CyberCatControlCenter-Setup-1.1.0.exe`。默认安装到当前用户的 `%LOCALAPPDATA%\Programs\CyberCatControlCenter`，无需管理员权限。可从 Windows“已安装的应用”或开始菜单卸载。卸载只移除安装器管理的程序和快捷方式，不处理源码、设备设置或用户文件。

本机既有 1.0.0 安装/卸载验证通过，但原始安装包和验证日志不在公开源码仓库。安装包未签名，Windows 可能显示“未知发布者”；不要为运行程序关闭系统防护。

安装器脚本和简体中文语言文件位于 `installer/`。构建安装包需本机安装 Inno Setup 6；构建工具、`.exe` 和日志都不提交到仓库。

## 本地构建 EXE

配置器源码位于 `bridge_wroom/tools/cybercat_bridge_config.py`，语言资源在同目录的 `cybercat_i18n.py`。使用 Python 3 构建：

```powershell
py -3 -m pip install -r bridge_wroom/tools/requirements.txt pyinstaller
py -3 -m PyInstaller --noconfirm --onefile --windowed --name CyberCatControlCenter --distpath dist --workpath diagnostics\exe-build --specpath diagnostics\exe-spec --hidden-import serial.tools.list_ports_windows --hidden-import cybercat_i18n --paths bridge_wroom\tools bridge_wroom\tools\cybercat_bridge_config.py
Copy-Item -LiteralPath .\dist\CyberCatControlCenter.exe -Destination .\CyberCatControlCenter.exe
```

如需生成安装包，把 EXE 放在项目根目录，再用 Inno Setup 6 编译 `installer/CyberCatControlCenter.iss`。打包方式不改变芯片固件，也不等于蓝牙卡顿问题已经修复。

WROOM 与 S3 分别选择实际串口并连接；串口号可能随插拔改变。使用前关闭占用同一端口的 Arduino 串口监视器或其他配置窗口。
