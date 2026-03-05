param(
    [string]$BuildDir = "build",
    [ValidateSet("Debug", "Release")]
    [string]$BuildType = "Release",
    [string]$DiscordClientId = "1479026910574153921",
    [string]$QtDir = "",
    [switch]$DisableCli,
    [switch]$DisableImGui,
    [switch]$EnableImGui,
    [switch]$BuildWpf
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Path $PSScriptRoot -Parent
$buildPath = Join-Path $repoRoot $BuildDir
if (!(Test-Path $buildPath)) {
    New-Item -ItemType Directory -Path $buildPath | Out-Null
}

function Resolve-CmakeExe {
    $cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
    if (![string]::IsNullOrWhiteSpace($cmake)) {
        return $cmake
    }

    $vsCmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $vsCmake) {
        return $vsCmake
    }

    throw "cmake not found on PATH and VS bundled cmake not found."
}

function Resolve-Qt6DirFromInput([string]$inputPath) {
    if ([string]::IsNullOrWhiteSpace($inputPath)) {
        return $null
    }

    $expanded = [Environment]::ExpandEnvironmentVariables($inputPath)
    if (!(Test-Path $expanded)) {
        return $null
    }

    $item = Get-Item $expanded
    if ($item.PSIsContainer) {
        $direct = Join-Path $item.FullName "Qt6Config.cmake"
        if (Test-Path $direct) {
            return $item.FullName
        }

        $nested = Join-Path $item.FullName "lib\cmake\Qt6"
        if (Test-Path (Join-Path $nested "Qt6Config.cmake")) {
            return $nested
        }
    } else {
        if ($item.Name -ieq "Qt6Config.cmake") {
            return $item.Directory.FullName
        }
    }

    return $null
}

function Resolve-Qt6DirAuto {
    $candidates = @()

    $fromQt6Dir = Resolve-Qt6DirFromInput $env:Qt6_DIR
    if ($fromQt6Dir) {
        $candidates += $fromQt6Dir
    }

    $fromQtDir = Resolve-Qt6DirFromInput $env:QT_DIR
    if ($fromQtDir) {
        $candidates += $fromQtDir
    }

    $qtRoot = "C:\Qt"
    if (Test-Path $qtRoot) {
        $versions = Get-ChildItem -Path $qtRoot -Directory -ErrorAction SilentlyContinue
        foreach ($versionDir in $versions) {
            $kits = Get-ChildItem -Path $versionDir.FullName -Directory -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -like "msvc*64" }
            foreach ($kit in $kits) {
                $candidate = Join-Path $kit.FullName "lib\cmake\Qt6"
                if (Test-Path (Join-Path $candidate "Qt6Config.cmake")) {
                    $candidates += $candidate
                }
            }
        }
    }

    if ($candidates.Count -eq 0) {
        return $null
    }

    return ($candidates | Select-Object -Unique | Sort-Object | Select-Object -Last 1)
}

$cmakeExe = Resolve-CmakeExe

$resolvedQt6Dir = Resolve-Qt6DirFromInput $QtDir
if (-not $resolvedQt6Dir) {
    $resolvedQt6Dir = Resolve-Qt6DirAuto
}

$cmakeArgs = @(
    "-S", $repoRoot,
    "-B", $buildPath,
    "-G", "Visual Studio 17 2022",
    "-A", "x64",
    "-DVOIDCARE_DISCORD_CLIENT_ID=$DiscordClientId",
    "-DCMAKE_BUILD_TYPE=$BuildType"
)

$cliValue = if ($DisableCli) { "OFF" } else { "ON" }
$imguiEnabled = $EnableImGui -and -not $DisableImGui
$imguiValue = if ($imguiEnabled) { "ON" } else { "OFF" }
$wpfValue = if ($BuildWpf) { "ON" } else { "OFF" }
$bridgeValue = if ($BuildWpf) { "ON" } else { "OFF" }

$cmakeArgs += "-DVOIDCARE_BUILD_CLI=$cliValue"
$cmakeArgs += "-DVOIDCARE_BUILD_IMGUI=$imguiValue"
$cmakeArgs += "-DVOIDCARE_BUILD_WPF=$wpfValue"
$cmakeArgs += "-DVOIDCARE_BUILD_BRIDGE=$bridgeValue"

if ($resolvedQt6Dir) {
    $cmakeArgs += "-DQt6_DIR=$resolvedQt6Dir"
    Write-Host "Using Qt6_DIR=$resolvedQt6Dir"
} else {
    Write-Warning "Qt6 was not auto-discovered. Pass -QtDir <Qt kit root or ...\lib\cmake\Qt6>."
}

& $cmakeExe @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE."
}

Write-Host "Configured VoidCare at $buildPath"
