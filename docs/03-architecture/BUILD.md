# 构建、运行与部署

已验证：Windows x64，Qt6.8.3 msvc2022_64，VS2022 MSVC19.31，CMake生成器Visual Studio17 2022，C++17。

## SDK准备

Qt6.8的官方Windows支持矩阵包含MSVC2022。[Qt6.8 Windows](https://doc.qt.io/qt-6.8/windows.html)

使用已有Qt SDK即可。本项目采用`.tools/Qt`，不全局替换旧Kit；开发工具不上传Git。可使用aqtinstall从Qt镜像安装指定包：

```powershell
python -m venv .venv
./.venv/Scripts/python -m pip install aqtinstall==3.3.0
./.venv/Scripts/python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -O .tools/Qt --archives qtbase
./tools/build.ps1
```

本机使用Python3.12。外部SDK可传`-QtRoot`。代码使用动态Qt库，安装和分发时应保留适用第三方许可声明。

## 运行

```powershell
$env:PATH = "$PWD/.tools/Qt/6.8.3/msvc2022_64/bin;" + $env:PATH
./build/Release/GPUView.exe
```

点击教学事件按钮，后台构建可取消，也可重复请求验证合并。滚轮缩放、中键平移、左键框选/点击、Home全览、Esc清除；底部滑条改变首个可见轨道。

## 本地部署

```powershell
New-Item -ItemType Directory artifacts/GPUView -Force
Copy-Item build/Release/GPUView.exe artifacts/GPUView/
./.tools/Qt/6.8.3/msvc2022_64/bin/windeployqt.exe --release --no-translations artifacts/GPUView/GPUView.exe
```

正式发布仍需在没有开发SDK的干净Windows环境验证，并整理Qt/运行库许可。当前本机运行不代表发布验收已完成。

## 验证

build.ps1运行两个CTest套件并打印build/core-tests.txt和build/ui-tests.txt。可单独运行gpuview_tests.exe、gpuview_ui_tests.exe -platform offscreen，使用`-o 路径,txt`保存详细日志。离屏测试不代表物理显示延迟测试。

截图：`GPUView.exe -platform offscreen --screenshot assets/screenshots/timeline.png`。该模式载入百万模拟事件后截图退出；WindowsGUI程序在PowerShell里用Start-Process -Wait等待。读取系统字体用于本机渲染，不分发字体文件。
