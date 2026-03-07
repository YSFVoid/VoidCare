param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$OutputDir = "dist"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Path $PSScriptRoot -Parent
$outPath = Join-Path $repoRoot $OutputDir
dotnet publish (Join-Path $repoRoot "src\VoidCare.Cli\VoidCare.Cli.csproj") `
    -c $Configuration `
    -r win-x64 `
    --self-contained true `
    /p:PublishSingleFile=true `
    /p:DebugType=None `
    /p:DebugSymbols=false `
    -o $outPath
