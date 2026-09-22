# Windows 发布与自动更新设计

## 目标与范围

为 `Naporing/microseismic-monitor-qt` 增加 Windows x64 发布能力，并允许用户在软件内检查 GitHub Releases、下载新版本、自动安装并重新启动。

本期范围：

- 公开 GitHub 仓库与公开 Releases。
- Windows 10/11 x64。
- 首次使用用户级安装器，安装到当前用户目录，不请求管理员权限。
- 后续由软件自动下载完整安装器并静默覆盖升级。
- 采用语义化版本号和 `vX.Y.Z` Git 标签。
- 更新为可选操作，不实现强制更新、差分更新、更新频道或后台服务。
- 第一版不要求购买 Authenticode 证书，但保留后续签名入口。

## 方案选择

采用“Qt 内置更新检查 + Inno Setup 用户级安装器”。

未采用的方案：

- Qt Installer Framework：组件仓库与维护工具更完整，但对当前单体 Demo 过重。
- ZIP + 自制替换器：能够自动更新，但需要额外处理运行中文件锁、替换失败和回滚。

Inno Setup 支持非管理员安装、静默运行、关闭占用文件的应用并重新启动应用，适合将同一个安装器同时用于首次安装和后续升级：

- https://jrsoftware.org/ishelp/topic_setup_privilegesrequired.htm
- https://jrsoftware.org/ishelp/topic_setupcmdline.htm
- https://jrsoftware.org/ishelp/topic_setup_closeapplications.htm

## 版本与发布资产

CMake 项目声明版本，例如：

```cmake
project(Seismic_Waveforms_demo VERSION 1.0.0 LANGUAGES CXX)
```

版本通过编译定义提供给程序。正式发布标签严格使用 `v1.0.0` 格式；GitHub Release 的 `tag_name` 是更新判断的唯一远端版本来源。

每个 Release 发布两个资产：

- `SeismicWaveforms-Setup-v1.0.0.exe`
- `SeismicWaveforms-Setup-v1.0.0.exe.sha256`

不把 GitHub 自动生成的源码 ZIP 当作更新包。

## 应用内更新架构

新增独立的 `UpdateManager`，使用一个 `QNetworkAccessManager` 异步执行网络请求，不阻塞 100 路波形刷新。它只负责：

1. 请求 `https://api.github.com/repos/Naporing/microseismic-monitor-qt/releases/latest`。
2. 解析 `tag_name`、发布说明与资产下载地址。
3. 使用 `QVersionNumber` 比较当前版本和远端版本。
4. 下载安装器及 SHA-256 文件到 `QStandardPaths::TempLocation` 下的专用临时目录。
5. 使用 `QCryptographicHash::Sha256` 校验安装器。
6. 校验成功后通过 `QProcess::startDetached` 启动静默安装器，再正常退出主程序。

GitHub Latest Release API 与 Qt 网络接口：

- https://docs.github.com/en/rest/releases
- https://doc.qt.io/qt-6/qnetworkaccessmanager.html

更新状态限定为：空闲、检查中、已是最新版、发现新版本、下载中、校验中、准备安装、失败。一次只允许一个检查或下载任务。

## 界面与交互

在顶部控制栏增加紧凑的版本入口，例如：

```text
版本 v1.0.0   检查更新
```

交互流程：

1. 用户点击“检查更新”。
2. 检查期间按钮禁用并显示“检查中…”。
3. 已是最新版时给出简短提示。
4. 有新版本时显示版本号、发布说明以及“立即更新/稍后”按钮。
5. 用户确认后显示下载进度；下载期间仍可观察界面，但不能重复触发更新。
6. 校验成功后提示即将重启，停止采集、启动安装器并退出。

程序启动后可延迟数秒静默检查一次；只有发现新版时才提示，网络错误保持安静。手动检查失败则明确显示错误。

## 安装器

新增 Inno Setup 脚本，核心约束：

- 固定 `AppId`，保证新版识别为同一应用。
- `PrivilegesRequired=lowest`。
- 默认目录为 `{localappdata}\Programs\SeismicWaveformsDemo`。
- 安装 x64 Release 构建及 Qt 部署目录中的全部运行库与插件。
- 创建当前用户开始菜单快捷方式和卸载入口。
- 交互式首次安装完成后允许启动应用。
- 更新模式使用 `/VERYSILENT /SUPPRESSMSGBOXES /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS /NORESTART`。
- 安装器版本、输出文件名由发布版本统一注入，避免手工维护多处版本号。

Qt 部署仍使用现有 `qt_generate_deploy_app_script`/CMake install 流程生成完整目录；Qt 官方部署说明：

- https://doc.qt.io/qt-6/cmake-deployment.html
- https://doc.qt.io/qt-6/windows-deployment.html

## GitHub Actions 发布流水线

新增 tag 触发的 Windows 工作流：

1. 校验标签符合 `vX.Y.Z`，并与 CMake 项目版本一致。
2. 检出代码并准备 Qt 6.11.x、MinGW、CMake 与 Inno Setup。
3. 配置 Release 构建并执行全部测试。
4. 执行 `cmake --install` 生成 Qt 运行目录。
5. 编译 Inno Setup 安装器。
6. 计算 SHA-256 文件。
7. 使用 GitHub CLI 创建 Release、自动生成发布说明并上传两个资产。

工作流仅授予发布任务 `contents: write`，其余权限保持只读。创建 Release 的官方命令与权限说明：

- https://cli.github.com/manual/gh_release_create
- https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax

## 安全与失败处理

- 只接受 HTTPS GitHub API 与 Release 下载地址。
- 严格匹配约定的安装器资产名，不执行任意 Release 附件。
- SHA-256 不一致时删除下载文件并禁止安装。
- JSON 缺字段、版本格式非法、无对应资产、网络超时或磁盘写入失败均转为可读错误。
- 下载先写临时文件，完成并校验后才允许执行。
- 不在客户端内保存 GitHub Token；公开 Releases 无需凭据。
- SHA-256 可发现下载损坏，但不能替代发布者身份认证。正式对外分发时应增加 Authenticode 代码签名，减少 Windows“未知发布者”提示。

## 测试策略

- 单元测试版本标签解析与版本比较。
- 使用本地假数据测试 GitHub Release JSON 解析、资产选择和异常字段。
- 使用临时文件测试 SHA-256 成功与失败路径。
- `UpdateManager` 的网络层通过可注入接口或本地假回复测试，不访问真实 GitHub。
- 主窗口测试按钮状态与用户确认流程，不实际启动安装器。
- 发布工作流执行现有 5 组 CTest，再构建安装器。
- 首次发布前在干净 Windows 用户环境手工验证首次安装、覆盖升级、取消更新、断网和卸载。

## 验收标准

- `v1.0.0` 标签可自动产出可安装的 Windows x64 Release。
- 安装过程无需管理员权限，程序可从开始菜单启动和卸载。
- 旧版本能发现 `v1.0.1`，显示说明，下载并校验安装器。
- 用户确认后无需手动下载、解压或再次选择安装目录，软件自动升级并重新启动。
- 更新失败不破坏当前可运行版本。
- 无网络或 GitHub 不可用时不影响波形采集与 FFT。
