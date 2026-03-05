param(
    [string]$BuildDir = "build",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$OutputDir = "dist"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Path $PSScriptRoot -Parent
$buildPath = Join-Path $repoRoot $BuildDir
$outPath = Join-Path $repoRoot $OutputDir
$stagePath = Join-Path $outPath "VoidCare"

if (!(Test-Path $outPath)) {
    New-Item -ItemType Directory -Path $outPath | Out-Null
}
if (Test-Path $stagePath) {
    Remove-Item -Path $stagePath -Recurse -Force
}
New-Item -ItemType Directory -Path $stagePath | Out-Null

$exePath = Join-Path $buildPath "app\$Configuration\VoidCare.exe"
if (!(Test-Path $exePath)) {
    throw "VoidCare.exe was not found at $exePath. Build first."
}

Copy-Item -Path $exePath -Destination $stagePath -Force

function Resolve-WindeployQtPath {
    $windeployqt = (Get-Command windeployqt -ErrorAction SilentlyContinue).Source
    if (-not [string]::IsNullOrWhiteSpace($windeployqt)) {
        return $windeployqt
    }

    $candidates = @()

    if ($env:QT_DIR) {
        $qtDirCandidate = [Environment]::ExpandEnvironmentVariables($env:QT_DIR)
        if (Test-Path $qtDirCandidate) {
            $candidates += (Join-Path $qtDirCandidate "bin\windeployqt.exe")
            $candidates += (Join-Path $qtDirCandidate "..\..\..\bin\windeployqt.exe")
        }
    }

    if ($env:Qt6_DIR) {
        $qt6DirCandidate = [Environment]::ExpandEnvironmentVariables($env:Qt6_DIR)
        if (Test-Path $qt6DirCandidate) {
            $candidates += (Join-Path $qt6DirCandidate "..\..\..\bin\windeployqt.exe")
        }
    }

    $cacheFile = Join-Path $buildPath "CMakeCache.txt"
    if (Test-Path $cacheFile) {
        $qtDirLine = Select-String -Path $cacheFile -Pattern "^Qt6_DIR:[^=]*=" | Select-Object -First 1
        if ($qtDirLine) {
            $qt6Dir = ($qtDirLine.Line -split "=", 2)[1]
            if (-not [string]::IsNullOrWhiteSpace($qt6Dir)) {
                $candidates += (Join-Path $qt6Dir "..\..\..\bin\windeployqt.exe")
            }
        }
    }

    $qtRoot = "C:\Qt"
    if (Test-Path $qtRoot) {
        $kits = Get-ChildItem -Path $qtRoot -Directory -Recurse -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like "msvc*64" }
        foreach ($kit in $kits) {
            $candidates += (Join-Path $kit.FullName "bin\windeployqt.exe")
        }
    }

    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        try {
            $resolved = [System.IO.Path]::GetFullPath($candidate)
            if (Test-Path $resolved) {
                return $resolved
            }
        } catch {
        }
    }

    return $null
}

$windeployqt = Resolve-WindeployQtPath
if ([string]::IsNullOrWhiteSpace($windeployqt)) {
    throw "windeployqt not found. Ensure Qt SDK is installed and either PATH or QT_DIR is set."
}
Write-Host "Using windeployqt at $windeployqt"

& $windeployqt --qmldir (Join-Path $repoRoot "ui\qml") (Join-Path $stagePath "VoidCare.exe")
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE."
}

if (Test-Path (Join-Path $repoRoot "README.md")) {
    Copy-Item -Path (Join-Path $repoRoot "README.md") -Destination $stagePath -Force
}

$zipPath = Join-Path $outPath "VoidCare-portable-$Configuration.zip"
if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}
Compress-Archive -Path (Join-Path $stagePath "*") -DestinationPath $zipPath

Write-Host "Portable package created: $zipPath"
