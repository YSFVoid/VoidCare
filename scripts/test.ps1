param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Path $PSScriptRoot -Parent
dotnet test (Join-Path $repoRoot "tests\VoidCare.Tests\VoidCare.Tests.csproj") -c $Configuration
