# Microseismic Monitor · 微震阵列监测 Demo

Qt 6 / C++17 的 100 路波形演示程序。包含持续正弦测试、地震形态模拟、五级振幅颜色，以及双击通道打开的时域 / FFT 底部详情。100 路采集共用一个定时器。

## Windows 安装与更新

支持 Windows 10/11 x64。从 [GitHub Releases](https://github.com/Naporing/microseismic-monitor-qt/releases) 下载 `SeismicWaveforms-Setup-vX.Y.Z.exe` 并安装。安装位置默认是 `%LOCALAPPDATA%\Programs\SeismicWaveformsDemo`，无需管理员权限，支持开始菜单启动和 Windows 设置中的卸载。

点击顶部“检查更新”，确认“立即更新”后，软件会下载、校验、静默安装并重启。下载中可取消，网络错误不影响采集。程序启动 5 秒后也会检查一次，检查失败不弹窗。首次正式发布前，手动检查会提示尚未找到公开版本。

更新安装器在临时目录保存，失败或取消的下载会清理；交给安装器执行的文件保留，超过 7 天后由后续启动清理。安装器可用 `/LOG` 日志排查，默认位于 `%TEMP%`。如果静默安装未完成，可重新运行当前版本或从 Releases 下载完整安装器。

当前项目产物尚未配置 Authenticode 签名，Windows 可能显示“未知发布者”。SHA-256 用于发现下载损坏，不替代发布者签名。

## 本地构建与测试

推荐与 CI 一致的 Qt 6.11.2 MinGW x64、MinGW 13.1、CMake，以及 Inno Setup 7.1。按实际安装路径设置环境：

```powershell
$env:QT_ROOT_DIR = 'D:\develop\Qt\6.11.2\mingw_64'
$env:PATH = "$env:QT_ROOT_DIR\bin;D:\develop\Qt\Tools\mingw1310_64\bin;D:\develop\Qt\Tools\CMake_64\bin;" + $env:PATH
cmake -S . -B build -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON "-DCMAKE_PREFIX_PATH=$env:QT_ROOT_DIR"
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

测试使用假网络回复和假的安装器启动函数，不访问 GitHub、不执行真实更新。CMake 自动为 Windows 测试配置所选 Qt 与编译器的运行路径。

## 生成安装包

```powershell
./scripts/package-windows.ps1 -Version 1.0.0 -IsccPath 'C:\Program Files\Inno Setup 7\ISCC.exe'
```

版本必须与 `CMakeLists.txt` 中的项目版本一致。脚本使用独立的 `build/package-release` Release 构建，将 Qt DLL、平台插件和 Windows TLS 插件部署到 `dist/windows`，再生成：

- `dist/release/SeismicWaveforms-Setup-v1.0.0.exe`
- `dist/release/SeismicWaveforms-Setup-v1.0.0.exe.sha256`

打包会重建脚本专用的 `dist/windows` 部署目录，请勿在其中保存手动文件。可以用 `-ValidateOnly -DeploymentDirectory <已部署目录>` 检查打包输入。需要显式路径时支持 `-QtRoot`、`-CMakePath`、`-IsccPath`。未来代码签名应加在安装器生成后、计算 SHA-256 之前。

## 发布流程

仓库：<https://github.com/Naporing/microseismic-monitor-qt>。

首次公开推送与发布须先确认版本及 GitHub 账号权限。连接仓库前检查 `git ls-remote`，确认历史兼容后配置 `origin`；不要强制推送。

1. 修改 `CMakeLists.txt` 的 `VERSION`，例如 `1.0.1`，提交并通过测试。
2. 推送代码。在 GitHub Actions 手动运行 **Windows release**，可只测试和打包，不公开发布。
3. 正式发布时创建并推送对应标签：

```powershell
git tag v1.0.1
git push origin master
git push origin v1.0.1
```

标签触发 Windows 工作流：校验版本 → 构建及 CTest → Qt 部署 → Inno Setup → SHA-256 → GitHub Release。只有发布任务拥有仓库写权限。手动运行只保留 Actions 构建产物。发布附件名是客户端协议的一部分，不能随意更改；已发布标签也不要复用覆盖。

## 发布前验证清单

本地编译、单元测试和打包不能替代下面的实际安装验收：

可先运行临时安装检查（若当前用户已有本软件安装，脚本会拒绝执行）：

```powershell
./scripts/test-windows-installer.ps1 -InstallerPath ./dist/release/SeismicWaveforms-Setup-v1.0.0.exe
```

该脚本在项目 `build` 的独立目录安装，清空 Qt 开发环境路径后检查实际加载的 DLL，再卸载测试副本；日志保留在该目录。它不替代干净 Windows 用户和在线升级测试。

- 在没有 Qt 开发环境的普通 Windows 用户账户首次安装，确认无提权、开始菜单可启动。
- 查看 100 路波形，切换正弦频率，打开 FFT 底部详情。
- 取消下载、断网后继续采集；手动检查失败应显示错误。
- 先安装 v1.0.0，再发布获准的 v1.0.1，从软件内确认更新，验证自动安装、只重启一次且显示 v1.0.1。
- 从 Windows 设置卸载，确认程序和快捷方式移除。

首次 GitHub 工作流运行、干净用户安装和跨版本在线升级仍需实际执行后才能标记通过。

接口依据：[Qt 部署](https://doc.qt.io/qt-6/windows-deployment.html)、[Qt 网络请求](https://doc.qt.io/qt-6/qnetworkrequest.html)、[Inno Setup 参数](https://jrsoftware.org/ishelp/topic_setupcmdline.htm)、[GitHub Release 命令](https://cli.github.com/manual/gh_release_create)。
