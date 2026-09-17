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

### VS Code

1. 用VS Code打开仓库根目录；安装推荐的Microsoft C/C++扩展（本机已安装）。CMake Tools为可选配置辅助，F5使用项目脚本，无需先选Kit。
2. 在src/app/main.cpp的MainWindow创建处设断点。
3. 按F5选择`GPUView (MSVC Debug)`；预启动任务调用Windows PowerShell构建Debug，然后cppvsdbg启动build/Debug/GPUView.exe。
4. 也可运行任务`GPUView: test Debug`构建并跑31个实际用例。Ctrl+Shift+B默认构建Debug。

.vscode中的路径基于`${workspaceFolder}`，不含本机盘符。项目默认Qt位置为`.tools/Qt/6.8.3/msvc2022_64`；外置SDK可在本机调整任务的`-QtRoot`和IntelliSense路径，不提交个人路径。

### Visual Studio 2022

打开 [GPUView.sln](../../GPUView.sln)，选Debug/x64，设置断点并F5。工程只有一个GPUView调试入口，内部core/adapters等库仍由CMake分别构建。Build/Rebuild/Clean分别映射到同一个脚本的相应动作。

该入口使用VS的Makefile项目机制调用CMake，不重复定义编译选项。sources.props和filters由tools/sync-vs-sources.ps1枚举目录生成，源码Include统一是`src\...`等相对路径。新增/删除源码后正常构建会同步；VS若提示工程文件改变，重新加载即可。也可以先单独运行同步脚本再打开工程。

工程内的源码、输出、包含目录用相对路径，调试命令/工作目录使用`$(ProjectDir)`定位，不写固定盘符。PDB由Debug构建产生，Qt运行库自动部署到exe旁，IDE无需修改全局PATH。

**路径边界**：公开、可迁移的Studio入口是项目根目录的GPUView.sln/GPUView.vcxproj。build/GPUView.sln等文件由CMake生成，缓存/工具依赖会含本机绝对路径，不是可迁移工程，不提交仓库。搬目录或换机器后重新构建生成build，不复制旧缓存。CMake的CMAKE_USE_RELATIVE_PATHS已失效，不能靠它强制原生生成器全相对化。[CMake说明](https://cmake.org/cmake/help/latest/variable/CMAKE_USE_RELATIVE_PATHS.html)、[VS Makefile项目](https://learn.microsoft.com/en-us/cpp/build/reference/creating-a-makefile-project?view=msvc-170)

### 命令行

```powershell
./tools/build.ps1 -Configuration Debug
./build/Debug/GPUView.exe
# 仅构建，不跑测试
./tools/build.ps1 -Configuration Debug -SkipTests
```

脚本通过windeployqt部署本地调试依赖，VS安装提供开发运行库。所有.ps1以UTF-8 BOM保存，保证Windows PowerShell5.1正确读取中文注释。该Debug输出用于本机开发，不能当作免安装正式发布包。

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

build.ps1运行两个CTest套件并打印build/core-tests-Debug.txt和build/ui-tests-Debug.txt（Release使用相应后缀）。CTest为离屏测试显式指向SDK插件目录，避免windeployqt的桌面插件部署覆盖测试搜索路径。每次测试前清空该配置日志，不复用旧结果。离屏测试不代表物理显示延迟测试。

截图：`GPUView.exe -platform offscreen --screenshot assets/screenshots/timeline.png`。该模式载入百万模拟事件后截图退出；WindowsGUI程序在PowerShell里用Start-Process -Wait等待。读取系统字体用于本机渲染，不分发字体文件。
