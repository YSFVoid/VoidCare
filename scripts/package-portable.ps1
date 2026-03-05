param(
    [string]$BuildDir = "build",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$OutputDir = "dist",
    [switch]$IncludeWpf
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

function Resolve-PrimaryExe {
    param([string]$RootBuild, [string]$Config)

    $candidates = @(
        (Join-Path $RootBuild "app\$Config\VoidCare.exe"),
        (Join-Path $RootBuild "$Config\VoidCare.exe"),
        (Join-Path $RootBuild "VoidCare.exe")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    return $null
}

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

$primaryExe = Resolve-PrimaryExe -RootBuild $buildPath -Config $Configuration
if (-not $primaryExe) {
    throw "VoidCare.exe was not found in '$buildPath'. Build first (ImGui target)."
}
Copy-Item -Path $primaryExe -Destination (Join-Path $stagePath "VoidCare.exe") -Force

$assetSource = Join-Path $repoRoot "app\assets"
if (Test-Path $assetSource) {
    Copy-Item -Path $assetSource -Destination (Join-Path $stagePath "assets") -Recurse -Force
}

$windeployqt = Resolve-WindeployQtPath
if ([string]::IsNullOrWhiteSpace($windeployqt)) {
    throw "windeployqt not found. Ensure Qt SDK is installed and PATH or QT_DIR is set."
}
Write-Host "Using windeployqt at $windeployqt"

& $windeployqt --no-translations --no-opengl-sw (Join-Path $stagePath "VoidCare.exe")
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE."
}

if ($IncludeWpf) {
    $wpfProject = Join-Path $repoRoot "ui\VoidCare.Wpf.csproj"
    if (Test-Path $wpfProject) {
        $experimentalPath = Join-Path $stagePath "experimental-wpf"
        New-Item -ItemType Directory -Path $experimentalPath -Force | Out-Null
        & dotnet publish $wpfProject -c $Configuration -r win-x64 --self-contained true -p:PublishSingleFile=false -p:PublishReadyToRun=false -p:Platform=x64 -o $experimentalPath
        if ($LASTEXITCODE -ne 0) {
            throw "WPF publish failed with exit code $LASTEXITCODE."
        }

        $bridgeExe = Join-Path $buildPath "bridge\$Configuration\VoidCare.Bridge.exe"
        if (Test-Path $bridgeExe) {
            Copy-Item -Path $bridgeExe -Destination $experimentalPath -Force
            & $windeployqt --no-translations --no-opengl-sw (Join-Path $experimentalPath "VoidCare.Bridge.exe")
            if ($LASTEXITCODE -ne 0) {
                throw "windeployqt for bridge failed with exit code $LASTEXITCODE."
            }
        }
    } else {
        Write-Warning "WPF project not found at $wpfProject"
    }
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
