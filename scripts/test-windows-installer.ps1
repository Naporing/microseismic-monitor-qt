[CmdletBinding()]
param([Parameter(Mandatory)][string]$InstallerPath)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$installer = (Resolve-Path -LiteralPath $InstallerPath).Path
$registryKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{DDA39D2A-DFCE-48A9-A769-B683EF01F46A}_is1'
if (Test-Path -LiteralPath $registryKey) { throw 'An installed copy already exists; use a clean Windows user for the smoke test.' }
$target = Join-Path $projectRoot ('build/install-smoke-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $target | Out-Null
$setupLog = Join-Path $target 'setup.log'
$uninstallLog = Join-Path $target 'uninstall.log'
$arguments = @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/NOICONS',('/DIR="' + $target + '"'),('/LOG="' + $setupLog + '"'))
$uninstaller = Join-Path $target 'unins000.exe'
try {
    $setup = Start-Process -FilePath $installer -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
    if ($setup.ExitCode -ne 0) { throw "Setup failed: $($setup.ExitCode). See $setupLog" }
    $installed = Get-ItemProperty -LiteralPath $registryKey
    if ($installed.InstallLocation.TrimEnd('\') -ne $target.TrimEnd('\')) { throw 'Unexpected installation location' }
    $exe = Join-Path $target 'bin/Seismic_Waveforms_demo.exe'
    if (-not (Test-Path -LiteralPath $exe)) { throw 'Installed executable is missing' }
    $originalPath = $env:PATH
    $originalPlugins = $env:QT_PLUGIN_PATH
    $originalPlatform = $env:QT_QPA_PLATFORM_PLUGIN_PATH
    try {
        $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
        $env:QT_PLUGIN_PATH = ''
        $env:QT_QPA_PLATFORM_PLUGIN_PATH = ''
        $app = Start-Process -FilePath $exe -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden -PassThru
        try {
            Start-Sleep -Seconds 3
            $app.Refresh()
            if ($app.HasExited) { throw "Installed app failed to start: $($app.ExitCode)" }
            # A hidden GUI has no Process.MainWindowHandle. Inspect loaded dependencies before closing the test process.
            $qtModules = @($app.Modules | Where-Object { $_.ModuleName -match '^Qt6(Core|Gui|Widgets|Network)\.dll$' })
            if ($qtModules.Count -ne 4) { throw 'Expected Qt modules were not loaded' }
            foreach ($module in $qtModules) {
                if (-not $module.FileName.StartsWith($target + '\', [StringComparison]::OrdinalIgnoreCase)) {
                    throw "Loaded a development DLL: $($module.FileName)"
                }
            }
            Write-Output "PASS: user-level install and standalone launch, version $($installed.DisplayVersion)"
        } finally {
            if (-not $app.HasExited) { Stop-Process -Id $app.Id }
        }
    } finally {
        $env:PATH = $originalPath
        $env:QT_PLUGIN_PATH = $originalPlugins
        $env:QT_QPA_PLATFORM_PLUGIN_PATH = $originalPlatform
    }
} finally {
    if (Test-Path -LiteralPath $uninstaller) {
        $remove = Start-Process -FilePath $uninstaller -ArgumentList @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/LOG="' + $uninstallLog + '"')) -WindowStyle Hidden -Wait -PassThru
        if ($remove.ExitCode -ne 0) { throw "Uninstall failed: $($remove.ExitCode). See $uninstallLog" }
        if (Test-Path -LiteralPath $registryKey) { throw 'Uninstall registry entry remains' }
        if (Test-Path -LiteralPath (Join-Path $target 'bin/Seismic_Waveforms_demo.exe')) { throw 'Installed executable remains' }
        Write-Output "PASS: uninstall; logs retained in $target"
    }
}
