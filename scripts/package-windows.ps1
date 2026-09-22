[CmdletBinding()]
param(
    [string]$Version,
    [string]$QtRoot = $env:QT_ROOT_DIR,
    [string]$IsccPath,
    [string]$CMakePath = 'cmake',
    [string]$DeploymentDirectory,
    [switch]$ValidateOnly
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$cmakeText = Get-Content -LiteralPath (Join-Path $projectRoot 'CMakeLists.txt') -Raw
$projectVersion = [regex]::Match($cmakeText, 'project\(Seismic_Waveforms_demo\s+VERSION\s+(\d+\.\d+\.\d+)').Groups[1].Value
if ($Version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') {
    throw 'Version must be an explicit X.Y.Z version, for example -Version 1.0.0'
}
if ($Version -ne $projectVersion) { throw "Version $Version differs from CMake version $projectVersion" }
if (-not $IsccPath) {
    $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($command) { $IsccPath = $command.Source }
    foreach ($candidate in @(
        (Join-Path $projectRoot 'build/tools/inno-7/ISCC.exe'),
        "$env:ProgramFiles\Inno Setup 7\ISCC.exe",
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
    )) {
        if (-not $IsccPath -and (Test-Path -LiteralPath $candidate -PathType Leaf)) { $IsccPath = $candidate }
    }
}
if (-not $IsccPath -or -not (Test-Path -LiteralPath $IsccPath -PathType Leaf)) { throw 'ISCC.exe not found; supply -IsccPath' }
if (-not $DeploymentDirectory) { $DeploymentDirectory = Join-Path $projectRoot 'dist/windows' }
$DeploymentDirectory = [IO.Path]::GetFullPath($DeploymentDirectory)

if (-not $ValidateOnly) {
    if (-not $QtRoot) {
        $qmake = Get-Command qmake.exe -ErrorAction SilentlyContinue
        if ($qmake) { $QtRoot = Split-Path -Parent (Split-Path -Parent $qmake.Source) }
    }
    if (-not $QtRoot -or -not (Test-Path -LiteralPath (Join-Path $QtRoot 'bin/windeployqt.exe'))) {
        throw 'Qt MinGW x64 was not found; supply -QtRoot or set QT_ROOT_DIR'
    }
    $buildDirectory = Join-Path $projectRoot 'build/package-release'
    & $CMakePath -S $projectRoot -B $buildDirectory -G 'MinGW Makefiles' '-DCMAKE_BUILD_TYPE=Release' '-DBUILD_TESTING=OFF' "-DCMAKE_PREFIX_PATH=$QtRoot"
    if ($LASTEXITCODE -ne 0) { throw 'Release configuration failed' }
    & $CMakePath --build $buildDirectory --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Release build failed' }
    # Only the script-owned default deployment directory may be cleared.
    $expected = [IO.Path]::GetFullPath((Join-Path $projectRoot 'dist/windows'))
    if ($DeploymentDirectory -ne $expected) { throw 'Build mode requires the project dist/windows directory' }
    foreach ($directory in @((Join-Path $projectRoot 'dist'), $DeploymentDirectory)) {
        if ((Test-Path -LiteralPath $directory) -and ((Get-Item -LiteralPath $directory).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'Deployment directory must not be a symbolic link or junction'
        }
    }
    if (Test-Path -LiteralPath $DeploymentDirectory) { Remove-Item -LiteralPath $DeploymentDirectory -Recurse -Force }
    & $CMakePath --install $buildDirectory --prefix $DeploymentDirectory --config Release
    if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed' }
}

foreach ($relativePath in @('bin/Seismic_Waveforms_demo.exe', 'bin/Qt6Core.dll', 'bin/Qt6Gui.dll',
                           'bin/Qt6Widgets.dll', 'bin/Qt6Network.dll', 'plugins/platforms/qwindows.dll',
                           'plugins/tls/qschannelbackend.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $DeploymentDirectory $relativePath) -PathType Leaf)) {
        throw "Deployment is incomplete: missing $relativePath"
    }
}
$installerName = "SeismicWaveforms-Setup-v$Version.exe"
if ($ValidateOnly) {
    Write-Output "Validated Windows packaging inputs for $installerName"
    return
}
$outputDirectory = Join-Path $projectRoot 'dist/release'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
& $IsccPath "/DAppVersion=$Version" "/DSourceDir=$DeploymentDirectory" "/DOutputDir=$outputDirectory" (Join-Path $projectRoot 'packaging/windows/SeismicWaveforms.iss')
if ($LASTEXITCODE -ne 0) { throw 'Inno Setup compilation failed' }
$installerPath = Join-Path $outputDirectory $installerName
if (-not (Test-Path -LiteralPath $installerPath)) { throw "Installer was not produced: $installerName" }
# Add Authenticode signing here, before computing the digest, when a certificate is available.
$digest = (Get-FileHash -LiteralPath $installerPath -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$installerPath.sha256", "$digest  $installerName`n", [Text.Encoding]::ASCII)
Write-Output "Installer: $installerPath"
Write-Output "SHA-256:   $installerPath.sha256"
