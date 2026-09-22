# Windows Release and Auto-Update Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Publish tested Windows x64 user-level installers to `Naporing/microseismic-monitor-qt` and let the Qt application download, verify, install, and restart into the latest GitHub Release.

**Architecture:** The application owns a small asynchronous `UpdateManager` built on Qt Network. It queries the public GitHub Latest Release API, downloads the full Inno Setup installer and checksum, then launches the verified installer silently before exiting. A tag-triggered GitHub Actions workflow builds, tests, deploys Qt dependencies, creates the installer, calculates SHA-256, and uploads both assets to a Release.

**Tech Stack:** C++17, Qt 6 Core/Widgets/Network/Test, CMake/CTest, Inno Setup 7, GitHub Actions, GitHub CLI.

---

## Preconditions

- Execute in the current project, following the user's earlier decision not to create a worktree for this small project.
- Do not push or create the first public Release until the user confirms the release version and credentials are available.
- The local repository currently has no configured remote. Before adding `origin`, run `git ls-remote https://github.com/Naporing/microseismic-monitor-qt.git`; if the remote contains unrelated history, stop and reconcile instead of force-pushing.

### Task 1: Centralize application version and make tests CI-portable

**Files:**
- Create: `appversion.h.in`
- Create: `tests/test_appversion.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Write the failing version test**

Create `tests/test_appversion.cpp` and require:

```cpp
#include "appversion.h"
#include <QTest>
#include <QVersionNumber>

class AppVersionTest : public QObject
{
    Q_OBJECT
private slots:
    void exposesSemanticVersion()
    {
        QCOMPARE(QString(APP_VERSION), QString("1.0.0"));
        QCOMPARE(QVersionNumber::fromString(APP_VERSION),
                 QVersionNumber(1, 0, 0));
    }
};

QTEST_MAIN(AppVersionTest)
#include "test_appversion.moc"
```

**Step 2: Run the build and verify RED**

Run:

```powershell
& 'D:\develop\Qt\Tools\CMake_64\bin\cmake.exe' --build build --target test_appversion -j 4
```

Expected: target/header does not exist.

**Step 3: Add version generation and test target**

Change the project declaration to:

```cmake
project(Seismic_Waveforms_demo VERSION 1.0.0 LANGUAGES CXX)
configure_file(appversion.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/appversion.h @ONLY)
```

Create `appversion.h.in`:

```cpp
#pragma once
#define APP_VERSION "@PROJECT_VERSION@"
```

Add `${CMAKE_CURRENT_BINARY_DIR}/generated` to application and test include paths. Add `test_appversion` to CTest.

Replace the hard-coded local Qt paths in `set_tests_properties` with only `QT_QPA_PLATFORM=minimal`; rely on the invoking environment/GitHub Qt setup for `PATH`. Keep local verification commands explicitly prepending the Qt and MinGW bin directories.

**Step 4: Verify GREEN and regressions**

Run the full local build and CTest. Expected: 6/6 test groups pass.

**Step 5: Commit**

```powershell
git add CMakeLists.txt appversion.h.in tests/test_appversion.cpp
git commit -m "build: centralize application version"
```

### Task 2: Parse GitHub Releases and verify update assets

**Files:**
- Create: `updatemanager.h`
- Create: `updatemanager.cpp`
- Create: `tests/test_updatemanager.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Write failing pure-logic tests**

Cover these cases:

- `v1.2.0` is newer than local `1.0.0`.
- Equal and older versions are not updates.
- Invalid/missing `tag_name` is rejected.
- Draft/prerelease JSON is rejected defensively.
- Assets must be exactly `SeismicWaveforms-Setup-v1.2.0.exe` and the matching `.sha256`.
- Missing or non-HTTPS asset URLs are rejected.
- A checksum line such as `<64 hex>  filename.exe` parses successfully.
- SHA-256 validation succeeds for known bytes and fails after modification.

Desired API:

```cpp
struct ReleaseInfo
{
    QVersionNumber version;
    QString tagName;
    QString releaseName;
    QString notes;
    QUrl installerUrl;
    QUrl checksumUrl;
    QString installerFileName;
};

class UpdateManager : public QObject
{
    Q_OBJECT
public:
    static std::optional<ReleaseInfo> parseRelease(
        const QByteArray &json,
        const QVersionNumber &currentVersion,
        QString *errorMessage = nullptr);
    static QByteArray parseSha256(const QByteArray &contents,
                                  QString *errorMessage = nullptr);
    static bool verifySha256(const QString &filePath,
                             const QByteArray &expectedHex);
};
```

**Step 2: Verify RED**

Build and run `test_updatemanager`; expected failure because the API is absent.

**Step 3: Implement only parsing and hashing**

Use `QJsonDocument`, `QJsonObject`, `QJsonArray`, `QVersionNumber`, `QCryptographicHash`, and `QFile`. Strip exactly one leading `v`, require a complete numeric semantic version, and construct expected asset names from `tag_name`.

**Step 4: Verify GREEN**

Run `test_updatemanager` and then all CTest groups.

**Step 5: Commit**

```powershell
git add CMakeLists.txt updatemanager.h updatemanager.cpp tests/test_updatemanager.cpp
git commit -m "feat: parse and verify update releases"
```

### Task 3: Add asynchronous checking and downloading

**Files:**
- Modify: `updatemanager.h`
- Modify: `updatemanager.cpp`
- Modify: `tests/test_updatemanager.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Write failing state tests**

Add tests for:

- only one request can run at a time;
- a successful latest-release reply emits `updateAvailable(ReleaseInfo)`;
- no newer version emits `upToDate()`;
- malformed JSON and network failure emit `failed(QString)`;
- installer bytes are written to a temporary update directory;
- checksum mismatch deletes the installer and emits failure;
- verified download emits `readyToInstall(QString)`.

Use a small fake `QNetworkAccessManager`/`QNetworkReply` in the test file. Inject the manager through the constructor; production creates its own manager when none is supplied.

Desired public surface:

```cpp
explicit UpdateManager(QObject *parent = nullptr,
                       QNetworkAccessManager *network = nullptr);
void checkForUpdates(bool silent = false);
void downloadUpdate(const ReleaseInfo &release);
bool isBusy() const;

signals:
    void checkingChanged(bool checking);
    void updateAvailable(const ReleaseInfo &release);
    void upToDate();
    void downloadProgress(qint64 received, qint64 total);
    void readyToInstall(const QString &installerPath);
    void failed(const QString &message, bool silent);
```

Register `ReleaseInfo` with Qt's meta-type system. Set an explicit GitHub API `Accept` header, API version header, product `User-Agent`, safe redirect policy, and a finite transfer timeout.

Stream installer data into a temporary file under:

```text
%TEMP%/SeismicWaveformsDemo/update-vX.Y.Z/
```

Clean stale update directories on startup, but never delete the verified installer before the detached setup process has started.

**Step 2: Verify RED**

Run the focused update-manager tests and confirm the asynchronous tests fail for missing behavior.

**Step 3: Implement minimal network state machine**

Use one `QNetworkAccessManager`, one active `QNetworkReply`, and explicit state. Fetch the checksum first, then stream the installer. Reject duplicate calls while busy and always call `deleteLater()` on replies.

**Step 4: Verify GREEN and regressions**

Run `test_updatemanager` and the full suite.

**Step 5: Commit**

```powershell
git add CMakeLists.txt updatemanager.h updatemanager.cpp tests/test_updatemanager.cpp
git commit -m "feat: download verified application updates"
```

### Task 4: Add update controls to the main window

**Files:**
- Modify: `mainwindow.h`
- Modify: `mainwindow.cpp`
- Modify: `tests/test_mainwindow.cpp`
- Modify: `CMakeLists.txt`

**Step 1: Write failing UI tests**

Require:

- a `currentVersionLabel` displaying `版本 v1.0.0`;
- a `checkUpdateButton` displaying `检查更新`;
- checking disables the button and displays `检查中…`;
- an available release displays its version and release notes with “立即更新/稍后”;
- download progress updates the UI;
- manual failures are visible while silent startup failures remain hidden;
- `readyToInstall` launches the updater path through an injectable launcher and requests a normal application exit only when launch succeeds.

Do not execute a real installer from tests. Inject a callable whose production implementation delegates to `QProcess::startDetached`.

**Step 2: Verify RED**

Run focused `test_mainwindow` functions and confirm the new controls are missing.

**Step 3: Add the compact controls and signal wiring**

Add the version label and button to the existing top control strip without altering the waveform layout. Construct one `UpdateManager` as a child of `MainWindow` and connect its state signals to the UI.

Use these installer arguments:

```text
/VERYSILENT
/SUPPRESSMSGBOXES
/CLOSEAPPLICATIONS
/NORESTART
/UPDATE=1
```

After `startDetached` succeeds, stop acquisition and queue `QCoreApplication::quit()`. If launch fails, keep the application open and show an error.

Schedule one silent check a few seconds after startup; never show a dialog for its network failure.

**Step 4: Verify GREEN and regressions**

Run `test_mainwindow`, followed by the full build and CTest.

**Step 5: Commit**

```powershell
git add CMakeLists.txt mainwindow.h mainwindow.cpp tests/test_mainwindow.cpp
git commit -m "feat: add in-app update workflow"
```

### Task 5: Create the user-level Windows installer

**Files:**
- Create: `packaging/windows/SeismicWaveforms.iss`
- Create: `scripts/package-windows.ps1`
- Modify: `.gitignore`
- Modify: `README.md` if present; otherwise create it with build/package instructions

**Step 1: Add a packaging contract test/dry run**

The script must fail clearly when the version, installed deployment directory, or `ISCC.exe` is missing. It must produce the exact installer name expected by `UpdateManager`.

Use Pester only if already available; otherwise implement a `-ValidateOnly` mode and invoke it from CTest/PowerShell with both valid and invalid arguments.

**Step 2: Implement the Inno Setup script**

Core directives:

```ini
[Setup]
AppId={{stable-guid-created-once}}
AppName=Microseismic Monitor
AppVersion={#AppVersion}
DefaultDirName={localappdata}\Programs\SeismicWaveformsDemo
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
RestartApplications=no
OutputBaseFilename=SeismicWaveforms-Setup-v{#AppVersion}
Compression=lzma2
SolidCompression=yes

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Microseismic Monitor"; Filename: "{app}\Seismic_Waveforms_demo.exe"

[Run]
Filename: "{app}\Seismic_Waveforms_demo.exe"; Flags: nowait postinstall skipifsilent
Filename: "{app}\Seismic_Waveforms_demo.exe"; Flags: nowait; Check: WizardSilent
```

The packaging script configures a clean Release build with `BUILD_TESTING=OFF`, runs `cmake --install` into `dist/windows`, invokes `ISCC.exe` with `AppVersion` and `SourceDir`, and calculates a lowercase SHA-256 file containing the installer filename.

Add `dist/` and packaging output directories to `.gitignore`.

**Step 3: Build and inspect locally**

Run `scripts/package-windows.ps1 -Version 1.0.0`. Verify the installer and `.sha256` exist and their names match the update parser contract.

**Step 4: Manual clean-user smoke test**

Install without elevation, launch from the Start menu, verify the 100-channel screen and FFT drawer, then uninstall. Record the steps in `README.md`.

**Step 5: Commit**

```powershell
git add .gitignore README.md packaging/windows/SeismicWaveforms.iss scripts/package-windows.ps1
git commit -m "build: add Windows user installer"
```

### Task 6: Automate GitHub Release publication

**Files:**
- Create: `.github/workflows/release-windows.yml`
- Modify: `README.md`

**Step 1: Write the workflow contract**

Trigger on tags matching `v*.*.*` and allow manual dispatch for dry runs. Use `windows-2022`. Set default `permissions: contents: read`; grant only the publish job `contents: write`.

The workflow must:

1. Extract `1.2.3` from `refs/tags/v1.2.3`.
2. compare it with `PROJECT_VERSION` in `CMakeLists.txt`;
3. install/configure Qt 6.11.x MinGW and Inno Setup;
4. configure, build, and run all CTest groups;
5. call `scripts/package-windows.ps1`;
6. upload the installer as a workflow artifact for manual runs;
7. for tag runs, call:

```powershell
gh release create $env:GITHUB_REF_NAME `
  "dist/release/SeismicWaveforms-Setup-$env:GITHUB_REF_NAME.exe" `
  "dist/release/SeismicWaveforms-Setup-$env:GITHUB_REF_NAME.exe.sha256" `
  --verify-tag --generate-notes --latest
```

Use `GH_TOKEN: ${{ github.token }}` only in the release step. Pin third-party setup actions to reviewed commit SHAs during implementation rather than floating branches.

**Step 2: Validate workflow syntax locally**

Use an available YAML parser plus repository searches to verify paths, asset names, version extraction, and permissions. Do not claim the workflow is proven until it has run on GitHub.

**Step 3: Commit**

```powershell
git add .github/workflows/release-windows.yml README.md
git commit -m "ci: publish Windows releases from version tags"
```

### Task 7: Final verification and repository connection

**Files:**
- Modify only if verification finds defects.

**Step 1: Run local verification**

Run:

```powershell
$env:PATH='D:\develop\Qt\Tools\mingw1310_64\bin;D:\develop\Qt\6.11.2\mingw_64\bin;' + $env:PATH
& 'D:\develop\Qt\Tools\CMake_64\bin\cmake.exe' --build build -j 4
& 'D:\develop\Qt\Tools\CMake_64\bin\ctest.exe' --test-dir build --output-on-failure
& '.\scripts\package-windows.ps1' -Version 1.0.0
git diff --check
git status --short
```

Expected: all tests pass; installer and checksum are generated; no unintended tracked changes.

**Step 2: Review update security and failure paths**

Confirm HTTPS-only URLs, exact asset names, finite timeouts, checksum verification, no embedded tokens, no installer execution after validation failure, and no forced exit after launch failure.

**Step 3: Inspect the remote before connecting**

```powershell
git ls-remote https://github.com/Naporing/microseismic-monitor-qt.git
```

- If empty, add it as `origin` and push the chosen default branch after user approval.
- If non-empty, compare histories and stop for reconciliation instructions.
- Never force-push as part of this plan.

**Step 4: First release checkpoint**

Ask the user to approve the initial public release. After approval, push `v1.0.0`, wait for the GitHub Actions workflow, inspect its result, and verify that both expected assets appear in the Release.

**Step 5: End-to-end upgrade test**

Install v1.0.0, publish a test v1.0.1 release, use the in-app button to update, and verify the application restarts showing v1.0.1. Remove the test release only if the user explicitly requests deletion.

## Implementation record — 2026-09-22

- Tasks 1–6 implemented locally on the existing `master` checkout.
- Eight CTest groups pass, including update parsing/network/UI tests and packaging validation.
- Built `dist/release/SeismicWaveforms-Setup-v1.0.0.exe` and its matching SHA-256 file with Inno Setup 7.1.0.
- Temporary user-level installation, launch without Qt development paths (all four Qt DLLs loaded from the installation), and uninstall passed. Logs are under `build/install-smoke-364bd3ed20684d82bd36816e53931b2c`.
- Workflow YAML and PowerShell syntax checked locally; the workflow has not yet run on GitHub.
- `git ls-remote` showed an empty remote. No code/tag push or public Release has been performed.
- Remaining release checkpoint: confirm initial public version and GitHub credentials, connect/push the repository, then run Actions and the real v1.0.0 → v1.0.1 upgrade test. A clean Windows user validation also remains.

Implementation adjustments supported by local testing:

- CTest needs explicit Windows runtime search paths in this environment. These now come from the configured Qt target and compiler location instead of hard-coded drive paths.
- The existing Qt deployment uses `bin/` and `plugins/`; installer shortcuts launch `bin/Seismic_Waveforms_demo.exe`.
- The installer restarts via `/UPDATE=1` and a single `[Run]` entry, with Restart Manager relaunch disabled to prevent duplicate launches.
- Downloads use uniquely named temporary directories, bounded streamed data, cancellation, and incremental SHA-256 hashing. Tests inject network replies and the process launcher.
