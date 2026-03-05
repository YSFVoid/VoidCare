param(
    [string]$BuildDir = "build",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$BuildTests,
    [switch]$SkipWpf
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Path $PSScriptRoot -Parent
$buildPath = Join-Path $repoRoot $BuildDir

$cmakeExe = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if ([string]::IsNullOrWhiteSpace($cmakeExe)) {
    $vsCmake = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (Test-Path $vsCmake) {
        $cmakeExe = $vsCmake
    } else {
        throw "cmake not found on PATH and VS bundled cmake not found."
    }
}

if (!(Test-Path $buildPath)) {
    throw "Build directory '$buildPath' does not exist. Run scripts/configure.ps1 first."
}

& $cmakeExe --build $buildPath --config $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

if ($BuildTests) {
    $cacheFile = Join-Path $buildPath "CMakeCache.txt"
    if (Test-Path $cacheFile) {
        $qtDirLine = Select-String -Path $cacheFile -Pattern "^Qt6_DIR:[^=]*=" | Select-Object -First 1
        if ($qtDirLine) {
            $qt6Dir = ($qtDirLine.Line -split "=", 2)[1]
            if (-not [string]::IsNullOrWhiteSpace($qt6Dir)) {
                $qtBin = Join-Path $qt6Dir "..\\..\\..\\bin"
                $qtBinResolved = [System.IO.Path]::GetFullPath($qtBin)
                if (Test-Path $qtBinResolved) {
                    $env:PATH = "$qtBinResolved;$env:PATH"
                    Write-Host "Using Qt runtime PATH prefix: $qtBinResolved"
                }
            }
        }
    }

    $ctestExe = (Get-Command ctest -ErrorAction SilentlyContinue).Source
    if ([string]::IsNullOrWhiteSpace($ctestExe)) {
        $ctestExe = Join-Path (Split-Path $cmakeExe -Parent) "ctest.exe"
    }
    & $ctestExe --test-dir $buildPath -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "CTest run failed with exit code $LASTEXITCODE."
    }
}

if (-not $SkipWpf) {
    $wpfProject = Join-Path $repoRoot "ui\\VoidCare.Wpf.csproj"
    if (Test-Path $wpfProject) {
        & dotnet build $wpfProject -c $Configuration -p:Platform=x64
        if ($LASTEXITCODE -ne 0) {
            throw "WPF build failed with exit code $LASTEXITCODE."
        }

        $publishDir = Join-Path $buildPath "wpf_publish\\$Configuration"
        & dotnet publish $wpfProject -c $Configuration -r win-x64 --self-contained true -p:PublishSingleFile=false -p:PublishReadyToRun=false -p:Platform=x64 -o $publishDir
        if ($LASTEXITCODE -ne 0) {
            throw "WPF publish failed with exit code $LASTEXITCODE."
        }
    } else {
        Write-Warning "WPF project not found at $wpfProject"
    }
}

Write-Host "Build completed ($Configuration)."
