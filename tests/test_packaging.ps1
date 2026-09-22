param([string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot))
$ErrorActionPreference = 'Stop'
$script = Join-Path $ProjectRoot 'scripts/package-windows.ps1'
if (-not (Test-Path -LiteralPath $script)) { throw 'Packaging script is missing' }
$versionText = Get-Content -LiteralPath (Join-Path $ProjectRoot 'CMakeLists.txt') -Raw
$version = [regex]::Match($versionText, 'project\(Seismic_Waveforms_demo\s+VERSION\s+(\d+\.\d+\.\d+)').Groups[1].Value
$fixture = Join-Path $ProjectRoot ('build/package-contract-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $fixture 'bin'),(Join-Path $fixture 'plugins/platforms'),(Join-Path $fixture 'plugins/tls') -Force | Out-Null
# Empty files are sufficient for validation; this test never invokes a compiler or installer.
foreach ($name in @('ISCC.exe','bin/Seismic_Waveforms_demo.exe','bin/Qt6Core.dll','bin/Qt6Gui.dll','bin/Qt6Widgets.dll','bin/Qt6Network.dll','plugins/platforms/qwindows.dll','plugins/tls/qschannelbackend.dll')) {
    New-Item -ItemType File -Path (Join-Path $fixture $name) | Out-Null
}
try {
    & $script -Version $version -ValidateOnly -DeploymentDirectory $fixture -IsccPath (Join-Path $fixture 'ISCC.exe')
    foreach ($case in @(
        @{Version='bad'; DeploymentDirectory=$fixture; IsccPath=(Join-Path $fixture 'ISCC.exe')},
        @{Version='99.99.99'; DeploymentDirectory=$fixture; IsccPath=(Join-Path $fixture 'ISCC.exe')},
        @{Version=$version; DeploymentDirectory=(Join-Path $fixture 'absent'); IsccPath=(Join-Path $fixture 'ISCC.exe')},
        @{Version=$version; DeploymentDirectory=$fixture; IsccPath=(Join-Path $fixture 'missing.exe')}
    )) {
        $rejected = $false
        try { & $script @case -ValidateOnly } catch { $rejected = $true }
        if (-not $rejected) { throw "Invalid packaging input was accepted: $($case | Out-String)" }
    }
    Write-Output 'Packaging contracts passed.'
} finally {
    $resolved = [IO.Path]::GetFullPath($fixture)
    $allowed = [IO.Path]::GetFullPath((Join-Path $ProjectRoot 'build')) + [IO.Path]::DirectorySeparatorChar
    if ($resolved.StartsWith($allowed, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
